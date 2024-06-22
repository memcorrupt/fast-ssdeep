#define NAPI_VERSION 3

#include <errno.h>
#include <node_api.h>
#include <string.h>
#include <stdlib.h>

#include "fuzzy.h"

//TODO: general code cleanup to make this FOSS worthy
//TODO: better js errors to explain ssdeep
//TODO: full test coverage
//TODO: ensure all napi errors are handled and make it cleaner
//TODO: add documentation and typescript types and other public package devqol stuff
/*
hash(string|Buffer)
hashSync(string|Buffer)
compare(string|Buffer, string|Buffer)
compareSync(string|Buffer, string|Buffer)

string/buffer (arrayview etc?)
*/

struct FuzzyHashTask {
    napi_deferred deferred;
    napi_value promise;
    napi_async_work work;
    
    unsigned char *buffer;
    size_t length;

    int error;
    char result[FUZZY_MAX_RESULT];
};

struct FuzzyCompareTask {
    napi_deferred deferred;
    napi_value promise;
    napi_async_work work;

    char hashes[2][FUZZY_MAX_RESULT + 1];
    int result;
};

static void FuzzyHashWorkerExecute(napi_env env, void *data){
    struct FuzzyHashTask *task = (struct FuzzyHashTask *) data;

    if(fuzzy_hash_buf(task->buffer, task->length, task->result))
        task->error = errno;
}

//TODO: throw unhandled error if status != napi_ok
static void FuzzyHashWorkerComplete(napi_env env, napi_status status, void *data){
    struct FuzzyHashTask *task = (struct FuzzyHashTask *) data;

    if(status != napi_ok){
        printf("bruh hash?"); //TODO: handle this properly
    }else if(!task->error){
        napi_value result;
        status = napi_create_string_latin1(env, task->result, NAPI_AUTO_LENGTH, &result);

        status = napi_resolve_deferred(env, task->deferred, result);
    }else{
        char formatErr[256];
        snprintf(formatErr, sizeof(formatErr), "Could not calculate ssdeep hash: %s\n", strerror(task->error));

        napi_value errorMsg;
        status = napi_create_string_latin1(env, formatErr, NAPI_AUTO_LENGTH, &errorMsg);

        napi_value errorObj;
        status = napi_create_error(env, NULL, errorMsg, &errorObj);
        
        status = napi_reject_deferred(env, task->deferred, errorObj);
    }

    status = napi_delete_async_work(env, task->work);
    free(task);
}

static void FuzzyCompareWorkerExecute(napi_env env, void *data){
    struct FuzzyCompareTask *task = (struct FuzzyCompareTask *) data;

    task->result = fuzzy_compare(task->hashes[0], task->hashes[1]);
}

//TODO: throw unhandled error if status != napi_ok
//TODO: dedupe code between FuzzyHashWorkerComplete & FuzzyCompareWorkerComplete
static void FuzzyCompareWorkerComplete(napi_env env, napi_status status, void *data){
    struct FuzzyCompareTask *task = (struct FuzzyCompareTask *) data;

    if(status != napi_ok){
        printf("bruh compare?\n"); //TODO: handle this properly
    }else if(task->result == -1){
        napi_value errorMsg;
        status = napi_create_string_latin1(env, "Could not compare malformed ssdeep hashes", NAPI_AUTO_LENGTH, &errorMsg);

        napi_value errorObj;
        status = napi_create_error(env, NULL, errorMsg, &errorObj);
        
        status = napi_reject_deferred(env, task->deferred, errorObj);
    }else{
        napi_value result;
        status = napi_create_int32(env, task->result, &result);

        status = napi_resolve_deferred(env, task->deferred, result);
    }

    status = napi_delete_async_work(env, task->work);
    free(task);
}

static napi_value FuzzyHash(napi_env env, napi_callback_info cbinfo){
    napi_status status;
    napi_value args[1];
    size_t argc = sizeof(args) / sizeof(napi_value); //TODO: use ARRAY_SIZE

    status = napi_get_cb_info(env, cbinfo, &argc, args, NULL, NULL);
    if(status != napi_ok) return NULL;

    if(argc < 1){
        status = napi_throw_type_error(env, NULL, "1 argument expected");
        if(status != napi_ok) return NULL;
    }

    napi_valuetype argType;
    napi_typeof(env, args[0], &argType);
    if(argType != napi_string){
        status = napi_throw_type_error(env, NULL, "first argument must be a string");
        if(status != napi_ok) return NULL;
    }

    size_t inputSize;
    status = napi_get_value_string_latin1(env, args[0], NULL, 0, &inputSize);
    if(status != napi_ok) return NULL;

    unsigned char *buf = malloc(inputSize + 1);
    status = napi_get_value_string_latin1(env, args[0], (char *)buf, inputSize + 1, &inputSize);
    if(status != napi_ok) return NULL;

    struct FuzzyHashTask *task = (struct FuzzyHashTask *)malloc(sizeof(struct FuzzyHashTask));
    if(task == NULL) return NULL;

    task->buffer = buf;
    task->length = inputSize;
    task->error = 0;

    status = napi_create_promise(env, &task->deferred, &task->promise);
    if(status != napi_ok) return NULL;

    napi_value resourceName;
    status = napi_create_string_utf8(env, "fast-ssdeep: hash", NAPI_AUTO_LENGTH, &resourceName);
    if(status != napi_ok) return NULL;

    status = napi_create_async_work(env, NULL, resourceName, FuzzyHashWorkerExecute, FuzzyHashWorkerComplete, task, &task->work);
    if(status != napi_ok) return NULL;

    status = napi_queue_async_work(env, task->work);
    if(status != napi_ok) return NULL;

    return task->promise;
}

//TODO: dedupe between FuzzyHash and FuzzyCompare
static napi_value FuzzyCompare(napi_env env, napi_callback_info cbinfo){
    napi_status status;
    napi_value args[2];
    size_t argc = sizeof(args) / sizeof(napi_value); //TODO: use ARRAY_SIZE

    status = napi_get_cb_info(env, cbinfo, &argc, args, NULL, NULL);
    if(status != napi_ok) return NULL;

    if(argc < 2){
        status = napi_throw_type_error(env, NULL, "2 arguments expected");
        if(status != napi_ok) return NULL;
    }

    struct FuzzyCompareTask *task = (struct FuzzyCompareTask *)malloc(sizeof(struct FuzzyCompareTask));
    if(task == NULL) return NULL;

    for(int i = 0; i < 2; i++){
        napi_valuetype argType;
        napi_typeof(env, args[i], &argType);
        
        if(argType != napi_string){
            status = napi_throw_type_error(env, NULL,
                i == 0 ? "first argument must be a string" : "second argument must be a string");
            if(status != napi_ok) return NULL;
        }

        status = napi_get_value_string_latin1(env, args[i], task->hashes[i], FUZZY_MAX_RESULT + 1, NULL);
        if(status != napi_ok) return NULL;
    }

    task->result = -1;

    status = napi_create_promise(env, &task->deferred, &task->promise);
    if(status != napi_ok) return NULL;

    napi_value resourceName;
    status = napi_create_string_utf8(env, "fast-ssdeep: compare", NAPI_AUTO_LENGTH, &resourceName);
    if(status != napi_ok) return NULL;

    status = napi_create_async_work(env, NULL, resourceName, FuzzyCompareWorkerExecute, FuzzyCompareWorkerComplete, task, &task->work);
    if(status != napi_ok) return NULL;

    status = napi_queue_async_work(env, task->work);
    if(status != napi_ok) return NULL;

    return task->promise;
}

//TODO: dedupe between FuzzyHash and FuzzyHashSync and FuzzyHashWorkerComplete
static napi_value FuzzyHashSync(napi_env env, napi_callback_info cbinfo){
    napi_status status;
    napi_value args[1];
    size_t argc = sizeof(args) / sizeof(napi_value); //TODO: use ARRAY_SIZE

    status = napi_get_cb_info(env, cbinfo, &argc, args, NULL, NULL);
    if(status != napi_ok) return NULL;

    if(argc < 1){
        status = napi_throw_type_error(env, NULL, "1 argument expected");
        if(status != napi_ok) return NULL;
    }

    napi_valuetype argType;
    napi_typeof(env, args[0], &argType);
    if(argType != napi_string){
        status = napi_throw_type_error(env, NULL, "first argument must be a string");
        if(status != napi_ok) return NULL;
    }

    size_t inputSize;
    status = napi_get_value_string_latin1(env, args[0], NULL, 0, &inputSize);
    if(status != napi_ok) return NULL;

    unsigned char *buf = malloc(inputSize + 1);
    status = napi_get_value_string_latin1(env, args[0], (char *)buf, inputSize + 1, &inputSize);
    if(status != napi_ok) return NULL;

    char hash[FUZZY_MAX_RESULT];
    if(!fuzzy_hash_buf(buf, inputSize, hash)){
        napi_value result;
        napi_create_string_latin1(env, hash, NAPI_AUTO_LENGTH, &result);

        return result;
    }else{
        errno_t error = errno;

        char formatErr[256];
        snprintf(formatErr, sizeof(formatErr), "Could not calculate ssdeep hash: %s\n", strerror(error));

        status = napi_throw_error(env, NULL, formatErr);
        if(status != napi_ok) return NULL;
    }

    return NULL;
}

static napi_value FuzzyCompareSync(napi_env env, napi_callback_info cbinfo){
    napi_status status;
    napi_value args[2];
    size_t argc = sizeof(args) / sizeof(napi_value); //TODO: use ARRAY_SIZE

    status = napi_get_cb_info(env, cbinfo, &argc, args, NULL, NULL);
    if(status != napi_ok) return NULL;

    if(argc < 2){
        status = napi_throw_type_error(env, NULL, "2 arguments expected");
        if(status != napi_ok) return NULL;
    }

    char hashes[2][FUZZY_MAX_RESULT + 1];
    for(int i = 0; i < 2; i++){
        napi_valuetype argType;
        napi_typeof(env, args[i], &argType);
        
        if(argType != napi_string){
            status = napi_throw_type_error(env, NULL,
                i == 0 ? "first argument must be a string" : "second argument must be a string");
            if(status != napi_ok) return NULL;
        }

        status = napi_get_value_string_latin1(env, args[i], hashes[i], FUZZY_MAX_RESULT + 1, NULL);
        if(status != napi_ok) return NULL;
    }

    int score = fuzzy_compare(hashes[0], hashes[1]);

    if(score != -1){
        napi_value result;
        napi_create_int32(env, score, &result);

        return result;
    }else{
        status = napi_throw_error(env, NULL, "Could not compare malformed ssdeep hashes"); 
        if(status != napi_ok) return NULL;  
    }

    return NULL;
}

//TODO: move struct defs or other stuff into a header file or hoist to top?
struct FuzzyNodeMethod {
    const char *name;
    napi_callback method;
};

struct FuzzyNodeMethod functions[] = {
    {
        "hash",
        FuzzyHash
    },
    {
        "compare",
        FuzzyCompare
    },
    {
        "hashSync",
        FuzzyHashSync
    },
    {
        "compareSync",
        FuzzyCompareSync
    }
};

NAPI_MODULE_INIT(/* napi_env env, napi_value exports */) {
    napi_status status;

    for(size_t i = 0; i < sizeof(functions) / sizeof(struct FuzzyNodeMethod); i++){
        struct FuzzyNodeMethod functionDef = functions[i];
        napi_value function;

        status = napi_create_function(env, functionDef.name, NAPI_AUTO_LENGTH, functionDef.method, NULL, &function);
        if(status != napi_ok) return NULL;

        status = napi_set_named_property(env, exports, functionDef.name, function);
        if(status != napi_ok) return NULL;
    }

    return exports;
}