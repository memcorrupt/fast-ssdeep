#define NAPI_VERSION 3

#include <errno.h>
#include <node_api.h>
#include <string.h>
#include <stdlib.h>

#include "../ssdeep/fuzzy.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define FUZZYTASK_HASH 0
#define FUZZYTASK_COMPARE 1

#define NAPI_INTERNAL_ERROR_MESSAGE "ssdeep node addon api internal error"
#define FUZZY_OOM_MESSAGE "ssdeep could not allocate memory"

#define NAPI_ERROR_RETHROW         \
    napi_throw_error(env, NULL, NAPI_INTERNAL_ERROR_MESSAGE);
#define NAPI_ERROR_RETHROW_RETURN  \
    NAPI_ERROR_RETHROW             \
    return;
#define NAPI_ERROR_RETHROW_RETURN_NULL  \
    NAPI_ERROR_RETHROW             \
    return NULL;

#define NAPI_CALL_BASE(env, call, onError) do {     \
    if((call) != napi_ok){                          \
        onError                                     \
    }                                               \
} while(0)

#define NAPI_CALL_OR_THROW(env, call) NAPI_CALL_BASE(env, call, NAPI_ERROR_RETHROW_RETURN_NULL)
#define NAPI_CALL_OR_THROW_VOID(env, call) NAPI_CALL_BASE(env, call, NAPI_ERROR_RETHROW_RETURN)

struct FuzzySequenceContents {
    size_t length;
    char data[];
};

struct FuzzyTask {
    napi_deferred deferred;
    napi_async_work work;

    unsigned char type;
};

struct FuzzyHashTask {
    struct FuzzyTask header;

    struct FuzzySequenceContents *contents;

    int error;
    char result[FUZZY_MAX_RESULT];
};

typedef char FuzzyCompareHashes[2][FUZZY_MAX_RESULT + 1];
struct FuzzyCompareTask {
    struct FuzzyTask header;

    FuzzyCompareHashes hashes;
    int result;
};

struct FuzzyNodeMethod {
    const char *name;
    napi_callback method;
    bool async;
};

static void FuzzyTaskFree(struct FuzzyTask *task){
    if(task->type == FUZZYTASK_HASH)
        free(((struct FuzzyHashTask *) task)->contents);

    free(task);
}

static napi_status FuzzyRejectDeferred(napi_env env, napi_deferred deferred, const char *message){
    napi_status status;
    napi_value errorMsg, errorObj;

    if((status = napi_create_string_latin1(env, message, NAPI_AUTO_LENGTH, &errorMsg)) != napi_ok)
        return status;
    if((status = napi_create_error(env, NULL, errorMsg, &errorObj)) != napi_ok)
        return status;

    return napi_reject_deferred(env, deferred, errorObj);
}

static void FuzzyWorkerExecute(napi_env env, void *data){
    struct FuzzyTask *task = (struct FuzzyTask *) data;

    switch(task->type){
        case FUZZYTASK_HASH: {
            struct FuzzyHashTask *hashTask = (struct FuzzyHashTask *) data;
            if(fuzzy_hash_buf((unsigned char *)hashTask->contents->data, hashTask->contents->length, hashTask->result))
                hashTask->error = errno;
            break;
        }
        case FUZZYTASK_COMPARE: {
            struct FuzzyCompareTask *compareTask = (struct FuzzyCompareTask *) data;
            compareTask->result = fuzzy_compare(compareTask->hashes[0], compareTask->hashes[1]);
            break;
        }
        default: {
            napi_throw_error(env, NULL, "ssdeep can't execute invalid worker task");
            break;
        }
    }
}

static void FuzzyWorkerComplete(napi_env env, napi_status status, void *data){
    struct FuzzyTask *task = (struct FuzzyTask *) data;

    char formatBuf[256];
    const char *error = NULL;
    napi_value result = NULL;

    if(status != napi_ok){
        error = "ssdeep task scheduling failed";
    }else{
        switch(task->type){
            case FUZZYTASK_HASH: {
                struct FuzzyHashTask *hashTask = (struct FuzzyHashTask *) data;

                if(hashTask->error){
                    snprintf(formatBuf, sizeof(formatBuf), "Could not calculate ssdeep hash: %s", strerror(hashTask->error));
                    error = formatBuf;
                }else if(napi_create_string_latin1(env, hashTask->result, NAPI_AUTO_LENGTH, &result) != napi_ok){
                    error = NAPI_INTERNAL_ERROR_MESSAGE;
                }
                break;
            }
            case FUZZYTASK_COMPARE: {
                struct FuzzyCompareTask *compareTask = (struct FuzzyCompareTask *) data;

                if(compareTask->result == -1){
                    error = "Could not compare malformed ssdeep hashes";
                }else if(napi_create_int32(env, compareTask->result, &result) != napi_ok){
                    error = NAPI_INTERNAL_ERROR_MESSAGE;
                }
                break;
            }
            default: {
                error = "ssdeep can't complete invalid worker task";
                break;
            }
        }
    }

    napi_status settled = error
        ? FuzzyRejectDeferred(env, task->deferred, error)
        : napi_resolve_deferred(env, task->deferred, result);

    napi_status deleted = napi_delete_async_work(env, task->work);
    FuzzyTaskFree(task);

    NAPI_CALL_OR_THROW_VOID(env, settled);
    NAPI_CALL_OR_THROW_VOID(env, deleted);
}

static napi_value FuzzyScheduleTask(napi_env env, struct FuzzyTask *task, const char *resourceName){
    napi_value resource, promise;

    if(napi_create_string_utf8(env, resourceName, NAPI_AUTO_LENGTH, &resource) != napi_ok)
        goto fail;
    if(napi_create_async_work(env, NULL, resource, FuzzyWorkerExecute, FuzzyWorkerComplete, task, &task->work) != napi_ok)
        goto fail;
    if(napi_create_promise(env, &task->deferred, &promise) != napi_ok)
        goto fail_work;
    if(napi_queue_async_work(env, task->work) != napi_ok)
        goto fail_promise;

    return promise;

fail_promise:
    FuzzyRejectDeferred(env, task->deferred, "ssdeep task scheduling failed");
    napi_delete_async_work(env, task->work);
    FuzzyTaskFree(task);
    return promise;

fail_work:
    napi_delete_async_work(env, task->work);
fail:
    FuzzyTaskFree(task);
    NAPI_ERROR_RETHROW_RETURN_NULL
}

struct FuzzySequenceContents *FuzzyGetSequenceContents(napi_env env, napi_value value, const char *errorMessage){
    napi_valuetype valueType;
    NAPI_CALL_OR_THROW(env, napi_typeof(env, value, &valueType));

    bool isString = valueType == napi_string;
    size_t inputSize;
    struct FuzzySequenceContents *contents;

    if(isString){
        NAPI_CALL_OR_THROW(env, napi_get_value_string_latin1(env, value, NULL, 0, &inputSize));

        contents = (struct FuzzySequenceContents *) malloc(sizeof(struct FuzzySequenceContents) + inputSize + 1);
        if(contents == NULL){
            napi_throw_error(env, NULL, FUZZY_OOM_MESSAGE);
            return NULL;
        }

        if(napi_get_value_string_latin1(env, value, contents->data, inputSize + 1, &inputSize) != napi_ok){
            free(contents);
            NAPI_ERROR_RETHROW_RETURN_NULL
        }
    }else{
        bool isBuffer;
        NAPI_CALL_OR_THROW(env, napi_is_buffer(env, value, &isBuffer));

        if(!isBuffer){
            napi_throw_type_error(env, NULL, errorMessage);
            return NULL;
        }

        char *origBuf;
        NAPI_CALL_OR_THROW(env, napi_get_buffer_info(env, value, (void **)&origBuf, &inputSize));

        contents = (struct FuzzySequenceContents *) malloc(sizeof(struct FuzzySequenceContents) + inputSize);
        if(contents == NULL){
            napi_throw_error(env, NULL, FUZZY_OOM_MESSAGE);
            return NULL;
        }

        memcpy(contents->data, origBuf, inputSize);
    }

    contents->length = inputSize;
    return contents;
}

static napi_value FuzzyHash(napi_env env, napi_callback_info cbinfo){
    napi_value args[1];
    size_t argc = ARRAY_SIZE(args);
    bool *asyncPtr;

    NAPI_CALL_OR_THROW(env, napi_get_cb_info(env, cbinfo, &argc, args, NULL, (void **)&asyncPtr));

    bool async = *asyncPtr;

    if(argc < 1){
        napi_throw_type_error(env, NULL, "1 argument expected");
        return NULL;
    }

    struct FuzzySequenceContents *contents = FuzzyGetSequenceContents(env, args[0], "first argument must be a string/buffer");
    if(contents == NULL)
        return NULL;

    if(async){
        struct FuzzyHashTask *task = (struct FuzzyHashTask *) malloc(sizeof(struct FuzzyHashTask));
        if(task == NULL){
            free(contents);
            napi_throw_error(env, NULL, FUZZY_OOM_MESSAGE);
            return NULL;
        }

        task->header.type = FUZZYTASK_HASH;
        task->contents = contents;
        task->error = 0;

        return FuzzyScheduleTask(env, &task->header, "fast-ssdeep: hash");
    }else{
        char hash[FUZZY_MAX_RESULT];
        int failed = fuzzy_hash_buf((unsigned char *)contents->data, contents->length, hash);
        int error = errno;
        free(contents);

        if(failed){
            char formatErr[256];
            snprintf(formatErr, sizeof(formatErr), "Could not calculate ssdeep hash: %s", strerror(error));

            napi_throw_error(env, NULL, formatErr);
            return NULL;
        }

        napi_value result;
        NAPI_CALL_OR_THROW(env, napi_create_string_latin1(env, hash, NAPI_AUTO_LENGTH, &result));

        return result;
    }
}

static napi_value FuzzyCompare(napi_env env, napi_callback_info cbinfo){
    napi_value args[2];
    size_t argc = ARRAY_SIZE(args);
    bool *asyncPtr;

    NAPI_CALL_OR_THROW(env, napi_get_cb_info(env, cbinfo, &argc, args, NULL, (void **)&asyncPtr));

    bool async = *asyncPtr;

    if(argc < 2){
        napi_throw_type_error(env, NULL, "2 arguments expected");
        return NULL;
    }

    struct FuzzyCompareTask *task = NULL;
    FuzzyCompareHashes hashes;
    char (*hashesPtr)[FUZZY_MAX_RESULT + 1] = hashes;

    if(async){
        task = (struct FuzzyCompareTask *) malloc(sizeof(struct FuzzyCompareTask));
        if(task == NULL){
            napi_throw_error(env, NULL, FUZZY_OOM_MESSAGE);
            return NULL;
        }

        task->header.type = FUZZYTASK_COMPARE;
        task->result = -1;

        hashesPtr = task->hashes;
    }

    for(int i = 0; i < 2; i++){
        napi_valuetype argType;
        if(napi_typeof(env, args[i], &argType) != napi_ok){
            free(task);
            NAPI_ERROR_RETHROW_RETURN_NULL
        }

        if(argType != napi_string){
            free(task);
            napi_throw_type_error(env, NULL,
                i == 0 ? "first argument must be a string" : "second argument must be a string");
            return NULL;
        }

        if(napi_get_value_string_latin1(env, args[i], hashesPtr[i], FUZZY_MAX_RESULT + 1, NULL) != napi_ok){
            free(task);
            NAPI_ERROR_RETHROW_RETURN_NULL
        }
    }

    if(async)
        return FuzzyScheduleTask(env, &task->header, "fast-ssdeep: compare");

    int score = fuzzy_compare(hashes[0], hashes[1]);

    if(score == -1){
        napi_throw_error(env, NULL, "Could not compare malformed ssdeep hashes");
        return NULL;
    }

    napi_value result;
    NAPI_CALL_OR_THROW(env, napi_create_int32(env, score, &result));

    return result;
}

const struct FuzzyNodeMethod functions[] = {
    {
        "hash",
        FuzzyHash,
        true,
    },
    {
        "compare",
        FuzzyCompare,
        true
    },
    {
        "hashSync",
        FuzzyHash,
        false
    },
    {
        "compareSync",
        FuzzyCompare,
        false
    }
};

NAPI_MODULE_INIT(/* napi_env env, napi_value exports */) {
    for(size_t i = 0; i < sizeof(functions) / sizeof(struct FuzzyNodeMethod); i++){
        const struct FuzzyNodeMethod *functionDef = &functions[i];
        napi_value function;

        NAPI_CALL_OR_THROW(env, napi_create_function(env, functionDef->name, NAPI_AUTO_LENGTH, functionDef->method, (void *)&functionDef->async, &function));
        NAPI_CALL_OR_THROW(env, napi_set_named_property(env, exports, functionDef->name, function));
    }

    return exports;
}
