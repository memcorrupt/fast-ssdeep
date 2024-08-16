#define NAPI_VERSION 3

#include <errno.h>
#include <node_api.h>
#include <string.h>
#include <stdlib.h>

#include "../ssdeep/fuzzy.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define FUZZYTASK_HASH 0
#define FUZZYTASK_COMPARE 1

#define NAPI_ERROR_RETURN_NULL return NULL;
#define NAPI_ERROR_RETHROW         \
    napi_throw_error(env, NULL, "ssdeep node addon api internal error");
#define NAPI_ERROR_RETHROW_RETURN  \
    NAPI_ERROR_RETHROW             \
    return;
#define NAPI_ERROR_RETHROW_RETURN_NULL  \
    NAPI_ERROR_RETHROW             \
    return NULL;
#define NAPI_ERROR_RETHROW_CLEANUP \
    NAPI_ERROR_RETHROW             \
    goto cleanup;

#define NAPI_CALL_BASE(env, call, onError) do {     \
    if((call) != napi_ok){                          \
        onError                                     \
    }                                               \
} while(0)

#define NAPI_ASSERT(env, cond) do {                 \
    if(!(cond)){                                    \
        NAPI_ERROR_RETHROW_RETURN_NULL              \
    }                                               \
} while(0)
    

#define NAPI_CALL(env, call) NAPI_CALL_BASE(env, call, NAPI_ERROR_RETURN_NULL)
#define NAPI_CALL_OR_THROW(env, call) NAPI_CALL_BASE(env, call, NAPI_ERROR_RETHROW_RETURN_NULL)
#define NAPI_CALL_OR_THROW_VOID(env, call) NAPI_CALL_BASE(env, call, NAPI_ERROR_RETHROW_RETURN)
#define NAPI_CALL_OR_THROW_CLEANUP(env, call) NAPI_CALL_BASE(env, call, NAPI_ERROR_RETHROW_CLEANUP)

struct FuzzySequenceContents {
    size_t length;
    char data[];
};

struct FuzzyTask {
    napi_deferred deferred;
    napi_value promise;
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

    if(status != napi_ok){
        napi_throw_error(env, NULL, "ssdeep task scheduling failed");
        goto cleanup;
    }

    char formatBuf[256];
    const char *error = NULL;
    napi_value result;

    switch(task->type){
        case FUZZYTASK_HASH: {
            struct FuzzyHashTask *hashTask = (struct FuzzyHashTask *) data;

            if(hashTask->error){
                snprintf(formatBuf, sizeof(formatBuf), "Could not calculate ssdeep hash: %s\n", strerror(hashTask->error));
                error = formatBuf;
            }else{
                NAPI_CALL_OR_THROW_CLEANUP(env, napi_create_string_latin1(env, hashTask->result, NAPI_AUTO_LENGTH, &result));
            }
            break;
        }
        case FUZZYTASK_COMPARE: {
            struct FuzzyCompareTask *compareTask = (struct FuzzyCompareTask *) data;

            if(compareTask->result == -1){
                error = "Could not compare malformed ssdeep hashes";
            }else{
                NAPI_CALL_OR_THROW_CLEANUP(env, napi_create_int32(env, compareTask->result, &result));
            }
            break;
        }
        default: {
            napi_throw_error(env, NULL, "ssdeep can't complete invalid worker task");
            goto cleanup;
        }
    }

    if(error){
        napi_value errorMsg;
        NAPI_CALL_OR_THROW_CLEANUP(env, napi_create_string_latin1(env, error, NAPI_AUTO_LENGTH, &errorMsg));

        napi_value errorObj;
        NAPI_CALL_OR_THROW_CLEANUP(env, napi_create_error(env, NULL, errorMsg, &errorObj));

        NAPI_CALL_OR_THROW_CLEANUP(env, napi_reject_deferred(env, task->deferred, errorObj));
    }else{
        NAPI_CALL_OR_THROW_CLEANUP(env, napi_resolve_deferred(env, task->deferred, result));
    }

cleanup:
    if(task->type == FUZZYTASK_HASH){
        struct FuzzyHashTask *hashTask = (struct FuzzyHashTask *) data;
        free(hashTask->contents);
    }

    status = napi_delete_async_work(env, task->work);
    free(task);

    NAPI_CALL_OR_THROW_VOID(env, status);
}

struct FuzzySequenceContents *FuzzyGetSequenceContents(napi_env env, napi_value value, const char *errorMessage){
    napi_valuetype valueType;
    NAPI_CALL(env, napi_typeof(env, value, &valueType));

    bool isString = valueType == napi_string;
    size_t inputSize;
    struct FuzzySequenceContents *contents;

    if(isString){
        NAPI_CALL(env, napi_get_value_string_latin1(env, value, NULL, 0, &inputSize));

        contents = (struct FuzzySequenceContents *) malloc(sizeof(struct FuzzySequenceContents) + inputSize + 1);
        NAPI_CALL(env, napi_get_value_string_latin1(env, value, contents->data, inputSize + 1, &inputSize));
    }else{
        bool isBuffer;
        NAPI_CALL(env, napi_is_buffer(env, value, &isBuffer));

        if(!isBuffer){
            napi_throw_type_error(env, NULL, errorMessage);
            return NULL;
        }

        char *origBuf;
        NAPI_CALL(env, napi_get_buffer_info(env, value, (void **)&origBuf, &inputSize));

        contents = (struct FuzzySequenceContents *) malloc(sizeof(struct FuzzySequenceContents) + inputSize);
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
        NAPI_CALL(env, napi_throw_type_error(env, NULL, "1 argument expected"));
    }

    struct FuzzySequenceContents *contents = FuzzyGetSequenceContents(env, args[0], "first argument must be a string/buffer");
    NAPI_ASSERT(env, contents != NULL);

    struct FuzzyHashTask *task = (struct FuzzyHashTask *) malloc(sizeof(struct FuzzyHashTask));
    NAPI_ASSERT(env, task != NULL);

    if(async){
        task->contents = contents;
        task->error = 0;
        task->header.type = FUZZYTASK_HASH;

        NAPI_CALL_OR_THROW(env, napi_create_promise(env, &task->header.deferred, &task->header.promise));

        napi_value resourceName;
        NAPI_CALL_OR_THROW(env, napi_create_string_utf8(env, "fast-ssdeep: hash", NAPI_AUTO_LENGTH, &resourceName));

        NAPI_CALL_OR_THROW(env, napi_create_async_work(env, NULL, resourceName, FuzzyWorkerExecute, FuzzyWorkerComplete, task, &task->header.work));
        NAPI_CALL_OR_THROW(env, napi_queue_async_work(env, task->header.work));

        return task->header.promise;
    }else{
        char hash[FUZZY_MAX_RESULT];
        if(!fuzzy_hash_buf((unsigned char *)contents->data, contents->length, hash)){
            napi_value result;
            NAPI_CALL_OR_THROW_CLEANUP(env, napi_create_string_latin1(env, hash, NAPI_AUTO_LENGTH, &result));

            return result;
        }else{
            int error = errno;

            char formatErr[256];
            snprintf(formatErr, sizeof(formatErr), "Could not calculate ssdeep hash: %s\n", strerror(error));

            napi_throw_error(env, NULL, formatErr);
        }

cleanup:
        free(contents);
        return NULL;
    }
}

static napi_value FuzzyCompare(napi_env env, napi_callback_info cbinfo){
    napi_value args[2];
    size_t argc = ARRAY_SIZE(args);
    bool *asyncPtr;

    NAPI_CALL_OR_THROW(env, napi_get_cb_info(env, cbinfo, &argc, args, NULL, (void **)&asyncPtr));

    bool async = *asyncPtr;

    if(argc < 2){
        NAPI_CALL(env, napi_throw_type_error(env, NULL, "2 arguments expected"));
    }

    struct FuzzyCompareTask *task;
    FuzzyCompareHashes hashes;
    char (*hashesPtr)[FUZZY_MAX_RESULT + 1];
    
    if(async){
        task = (struct FuzzyCompareTask *) malloc(sizeof(struct FuzzyCompareTask));
        NAPI_ASSERT(env, task != NULL);

        task->header.type = FUZZYTASK_COMPARE;
        task->result = -1;

        hashesPtr = task->hashes;
    }else{
        hashesPtr = hashes;
    }

    for(int i = 0; i < 2; i++){
        napi_valuetype argType;
        NAPI_CALL_OR_THROW(env, napi_typeof(env, args[i], &argType));
        
        if(argType != napi_string){
            NAPI_CALL(env, napi_throw_type_error(env, NULL,
                i == 0 ? "first argument must be a string" : "second argument must be a string"));
        }

        NAPI_CALL_OR_THROW(env, napi_get_value_string_latin1(env, args[i], hashesPtr[i], FUZZY_MAX_RESULT + 1, NULL));
    }
    
    if(async){
        NAPI_CALL_OR_THROW(env, napi_create_promise(env, &task->header.deferred, &task->header.promise));

        napi_value resourceName;
        NAPI_CALL_OR_THROW(env, napi_create_string_utf8(env, "fast-ssdeep: compare", NAPI_AUTO_LENGTH, &resourceName));

        NAPI_CALL_OR_THROW(env, napi_create_async_work(env, NULL, resourceName, FuzzyWorkerExecute, FuzzyWorkerComplete, task, &task->header.work));
        NAPI_CALL_OR_THROW(env, napi_queue_async_work(env, task->header.work));

        return task->header.promise;
    }else{
        int score = fuzzy_compare(hashes[0], hashes[1]);

        if(score != -1){
            napi_value result;
            NAPI_CALL_OR_THROW(env, napi_create_int32(env, score, &result));

            return result;
        }else{
            NAPI_CALL(env, napi_throw_error(env, NULL, "Could not compare malformed ssdeep hashes")); 
        }

        return NULL;
    }
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