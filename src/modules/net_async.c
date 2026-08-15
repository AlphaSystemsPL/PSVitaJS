#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/kernel/threadmgr/mutex.h>

#include "../env.h"

#define VITAJS_NET_POOL_SIZE      (1024 * 1024)
#define VITAJS_HTTP_POOL_SIZE     (1024 * 1024)
#define VITAJS_HTTP_READ_CHUNK    (16 * 1024)
#define VITAJS_HTTP_MAX_RESPONSE  (8 * 1024 * 1024)

typedef enum NetRequestState
{
    NET_REQUEST_IDLE = 0,
    NET_REQUEST_RUNNING,
    NET_REQUEST_DONE,
    NET_REQUEST_ERROR
} NetRequestState;

typedef struct NetRequest
{
    NetRequestState state;

    char *url;

    int status;
    char *response_body;
    size_t response_length;

    int error_code;
    char error_operation[96];

    /*
     * These JSValues are owned and touched ONLY by the QuickJS/main thread.
     * The worker thread must never access QuickJS.
     */
    JSValue resolve;
    JSValue reject;
} NetRequest;

static void *net_memory = NULL;

static int net_module_loaded = 0;
static int http_module_loaded = 0;
static int net_initialized = 0;
static int netctl_initialized = 0;
static int http_initialized = 0;

static SceUID request_mutex = -1;

static NetRequest request = {
    .state = NET_REQUEST_IDLE,
    .url = NULL,
    .status = 0,
    .response_body = NULL,
    .response_length = 0,
    .error_code = 0,
    .error_operation = {0},
    .resolve = JS_UNDEFINED,
    .reject = JS_UNDEFINED,
};

static void lock_request(void)
{
    if (request_mutex >= 0)
        sceKernelLockMutex(request_mutex, 1, NULL);
}

static void unlock_request(void)
{
    if (request_mutex >= 0)
        sceKernelUnlockMutex(request_mutex, 1);
}

static JSValue make_error(JSContext *ctx, const char *operation, int code)
{
    char message[160];

    if (code != 0)
    {
        snprintf(
            message,
            sizeof(message),
            "%s failed: 0x%08X",
            operation,
            (unsigned int)code
        );
    }
    else
    {
        snprintf(
            message,
            sizeof(message),
            "%s",
            operation
        );
    }

    JSValue error = JS_NewError(ctx);

    if (JS_IsException(error))
        return error;

    JS_SetPropertyStr(
        ctx,
        error,
        "message",
        JS_NewString(ctx, message)
    );

    JS_SetPropertyStr(
        ctx,
        error,
        "code",
        JS_NewInt32(ctx, code)
    );

    return error;
}

static JSValue make_response(
    JSContext *ctx,
    int status,
    const char *body,
    size_t body_length
)
{
    JSValue response = JS_NewObject(ctx);

    if (JS_IsException(response))
        return response;

    JS_SetPropertyStr(
        ctx,
        response,
        "status",
        JS_NewInt32(ctx, status)
    );

    JS_SetPropertyStr(
        ctx,
        response,
        "ok",
        JS_NewBool(ctx, status >= 200 && status < 300)
    );

    JS_SetPropertyStr(
        ctx,
        response,
        "body",
        JS_NewStringLen(
            ctx,
            body ? body : "",
            body_length
        )
    );

    return response;
}

static int net_ensure_initialized(JSContext *ctx)
{
    int ret;

    if (http_initialized)
        return 0;

    if (request_mutex < 0)
    {
        request_mutex = sceKernelCreateMutex(
            "vitajs_net_mutex",
            0,
            1,
            NULL
        );

        if (request_mutex < 0)
        {
            JS_ThrowInternalError(
                ctx,
                "sceKernelCreateMutex failed: 0x%08X",
                (unsigned int)request_mutex
            );

            return -1;
        }
    }

    if (!net_module_loaded)
    {
        ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

        if (ret < 0)
        {
            JS_ThrowInternalError(
                ctx,
                "sceSysmoduleLoadModule(NET) failed: 0x%08X",
                (unsigned int)ret
            );

            return -1;
        }

        net_module_loaded = 1;
    }

    if (!net_initialized)
    {
        net_memory = malloc(VITAJS_NET_POOL_SIZE);

        if (!net_memory)
        {
            JS_ThrowOutOfMemory(ctx);
            return -1;
        }

        SceNetInitParam net_param;
        memset(&net_param, 0, sizeof(net_param));

        net_param.memory = net_memory;
        net_param.size = VITAJS_NET_POOL_SIZE;
        net_param.flags = 0;

        ret = sceNetInit(&net_param);

        if (ret < 0)
        {
            free(net_memory);
            net_memory = NULL;

            JS_ThrowInternalError(
                ctx,
                "sceNetInit failed: 0x%08X",
                (unsigned int)ret
            );

            return -1;
        }

        net_initialized = 1;
    }

    if (!netctl_initialized)
    {
        ret = sceNetCtlInit();

        if (ret < 0)
        {
            JS_ThrowInternalError(
                ctx,
                "sceNetCtlInit failed: 0x%08X",
                (unsigned int)ret
            );

            return -1;
        }

        netctl_initialized = 1;
    }

    if (!http_module_loaded)
    {
        ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

        if (ret < 0)
        {
            JS_ThrowInternalError(
                ctx,
                "sceSysmoduleLoadModule(HTTP) failed: 0x%08X",
                (unsigned int)ret
            );

            return -1;
        }

        http_module_loaded = 1;
    }

    if (!http_initialized)
    {
        ret = sceHttpInit(VITAJS_HTTP_POOL_SIZE);

        if (
            ret < 0 &&
            ret != (int)SCE_HTTP_ERROR_ALREADY_INITED
        )
        {
            JS_ThrowInternalError(
                ctx,
                "sceHttpInit failed: 0x%08X",
                (unsigned int)ret
            );

            return -1;
        }

        http_initialized = 1;
    }

    return 0;
}

/*
 * Executes entirely on a native PS Vita worker thread.
 *
 * IMPORTANT:
 * This function MUST NOT call any QuickJS function.
 */
static int net_fetch_worker(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    int tpl = -1;
    int conn = -1;
    int req = -1;

    int status = 0;
    int error_code = 0;

    char error_operation[96];
    error_operation[0] = '\0';

    char *response_body = NULL;
    size_t response_length = 0;
    size_t response_capacity = 0;

    char *url = NULL;

    lock_request();

    if (request.url)
        url = strdup(request.url);

    unlock_request();

    if (!url)
    {
        error_code = 0;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "Unable to copy request URL"
        );

        goto finish;
    }

    tpl = sceHttpCreateTemplate(
        "VitaJS/1.0",
        SCE_HTTP_VERSION_1_1,
        1
    );

    if (tpl < 0)
    {
        error_code = tpl;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpCreateTemplate"
        );

        goto finish;
    }

    conn = sceHttpCreateConnectionWithURL(
        tpl,
        url,
        0
    );

    if (conn < 0)
    {
        error_code = conn;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpCreateConnectionWithURL"
        );

        goto finish;
    }

    req = sceHttpCreateRequestWithURL(
        conn,
        SCE_HTTP_METHOD_GET,
        url,
        0
    );

    if (req < 0)
    {
        error_code = req;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpCreateRequestWithURL"
        );

        goto finish;
    }

    int ret = sceHttpSetAutoRedirect(
        req,
        SCE_HTTP_ENABLE
    );

    if (ret < 0)
    {
        error_code = ret;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpSetAutoRedirect"
        );

        goto finish;
    }

    ret = sceHttpAddRequestHeader(
        req,
        "Accept",
        "*/*",
        SCE_HTTP_HEADER_OVERWRITE
    );

    if (ret < 0)
    {
        error_code = ret;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpAddRequestHeader"
        );

        goto finish;
    }

    ret = sceHttpSendRequest(
        req,
        NULL,
        0
    );

    if (ret < 0)
    {
        error_code = ret;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpSendRequest"
        );

        goto finish;
    }

    ret = sceHttpGetStatusCode(
        req,
        &status
    );

    if (ret < 0)
    {
        error_code = ret;
        snprintf(
            error_operation,
            sizeof(error_operation),
            "sceHttpGetStatusCode"
        );

        goto finish;
    }

    unsigned char chunk[VITAJS_HTTP_READ_CHUNK];

    for (;;)
    {
        int bytes_read = sceHttpReadData(
            req,
            chunk,
            sizeof(chunk)
        );

        if (bytes_read < 0)
        {
            error_code = bytes_read;
            snprintf(
                error_operation,
                sizeof(error_operation),
                "sceHttpReadData"
            );

            goto finish;
        }

        if (bytes_read == 0)
            break;

        size_t required =
            response_length +
            (size_t)bytes_read +
            1;

        if (required > VITAJS_HTTP_MAX_RESPONSE + 1)
        {
            error_code = 0;

            snprintf(
                error_operation,
                sizeof(error_operation),
                "HTTP response exceeds VitaJS 8 MB limit"
            );

            goto finish;
        }

        if (required > response_capacity)
        {
            size_t new_capacity =
                response_capacity == 0
                    ? VITAJS_HTTP_READ_CHUNK
                    : response_capacity * 2;

            while (new_capacity < required)
                new_capacity *= 2;

            if (
                new_capacity >
                VITAJS_HTTP_MAX_RESPONSE + 1
            )
            {
                new_capacity =
                    VITAJS_HTTP_MAX_RESPONSE + 1;
            }

            char *new_body =
                realloc(
                    response_body,
                    new_capacity
                );

            if (!new_body)
            {
                error_code = 0;

                snprintf(
                    error_operation,
                    sizeof(error_operation),
                    "Out of memory while reading HTTP response"
                );

                goto finish;
            }

            response_body = new_body;
            response_capacity = new_capacity;
        }

        memcpy(
            response_body + response_length,
            chunk,
            (size_t)bytes_read
        );

        response_length +=
            (size_t)bytes_read;
    }

    if (response_body)
        response_body[response_length] = '\0';

finish:
    if (req >= 0)
        sceHttpDeleteRequest(req);

    if (conn >= 0)
        sceHttpDeleteConnection(conn);

    if (tpl >= 0)
        sceHttpDeleteTemplate(tpl);

    if (url)
        free(url);

    lock_request();

    if (error_operation[0] != '\0')
    {
        if (response_body)
        {
            free(response_body);
            response_body = NULL;
        }

        request.error_code = error_code;

        snprintf(
            request.error_operation,
            sizeof(request.error_operation),
            "%s",
            error_operation
        );

        request.state = NET_REQUEST_ERROR;
    }
    else
    {
        request.status = status;
        request.response_body = response_body;
        request.response_length = response_length;

        request.state = NET_REQUEST_DONE;
    }

    unlock_request();

    /*
     * Self-delete after publishing result.
     */
    sceKernelExitDeleteThread(0);

    return 0;
}

static JSValue reject_promise_immediately(
    JSContext *ctx,
    JSValue promise,
    JSValue resolve,
    JSValue reject,
    const char *operation,
    int code
)
{
    JSValue error =
        make_error(
            ctx,
            operation,
            code
        );

    if (!JS_IsException(error))
    {
        JSValue call_result =
            JS_Call(
                ctx,
                reject,
                JS_UNDEFINED,
                1,
                (JSValueConst *)&error
            );

        JS_FreeValue(
            ctx,
            call_result
        );

        JS_FreeValue(
            ctx,
            error
        );
    }

    JS_FreeValue(ctx, resolve);
    JS_FreeValue(ctx, reject);

    return promise;
}

/*
 * Net.fetch(url) -> Promise<{ status, ok, body }>
 *
 * MVP: one active request at a time.
 */
static JSValue vitajs_net_fetch(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    (void)this_val;

    if (argc != 1)
    {
        return JS_ThrowSyntaxError(
            ctx,
            "fetch(url)"
        );
    }

    const char *url =
        JS_ToCString(
            ctx,
            argv[0]
        );

    if (!url)
        return JS_EXCEPTION;

    if (net_ensure_initialized(ctx) < 0)
    {
        JS_FreeCString(
            ctx,
            url
        );

        return JS_EXCEPTION;
    }

    JSValue resolving_funcs[2];

    JSValue promise =
        JS_NewPromiseCapability(
            ctx,
            resolving_funcs
        );

    if (JS_IsException(promise))
    {
        JS_FreeCString(
            ctx,
            url
        );

        return promise;
    }

    lock_request();

    if (request.state != NET_REQUEST_IDLE)
    {
        unlock_request();

        JS_FreeCString(
            ctx,
            url
        );

        return reject_promise_immediately(
            ctx,
            promise,
            resolving_funcs[0],
            resolving_funcs[1],
            "Net.fetch: another request is already running",
            0
        );
    }

    request.url = strdup(url);

    JS_FreeCString(
        ctx,
        url
    );

    if (!request.url)
    {
        unlock_request();

        return reject_promise_immediately(
            ctx,
            promise,
            resolving_funcs[0],
            resolving_funcs[1],
            "Net.fetch: out of memory",
            0
        );
    }

    request.status = 0;
    request.response_body = NULL;
    request.response_length = 0;
    request.error_code = 0;
    request.error_operation[0] = '\0';

    request.resolve =
        resolving_funcs[0];

    request.reject =
        resolving_funcs[1];

    request.state =
        NET_REQUEST_RUNNING;

    unlock_request();

    SceUID thread =
        sceKernelCreateThread(
            "vitajs_net_fetch",
            net_fetch_worker,
            0x10000100,
            0x10000,
            0,
            0,
            NULL
        );

    if (thread < 0)
    {
        JSValue resolve;
        JSValue reject;

        lock_request();

        resolve = request.resolve;
        reject = request.reject;

        request.resolve = JS_UNDEFINED;
        request.reject = JS_UNDEFINED;

        if (request.url)
        {
            free(request.url);
            request.url = NULL;
        }

        request.state =
            NET_REQUEST_IDLE;

        unlock_request();

        return reject_promise_immediately(
            ctx,
            promise,
            resolve,
            reject,
            "sceKernelCreateThread",
            thread
        );
    }

    int ret =
        sceKernelStartThread(
            thread,
            0,
            NULL
        );

    if (ret < 0)
    {
        sceKernelDeleteThread(thread);

        JSValue resolve;
        JSValue reject;

        lock_request();

        resolve = request.resolve;
        reject = request.reject;

        request.resolve = JS_UNDEFINED;
        request.reject = JS_UNDEFINED;

        if (request.url)
        {
            free(request.url);
            request.url = NULL;
        }

        request.state =
            NET_REQUEST_IDLE;

        unlock_request();

        return reject_promise_immediately(
            ctx,
            promise,
            resolve,
            reject,
            "sceKernelStartThread",
            ret
        );
    }

    return promise;
}

/*
 * Called ONLY from the JS/main thread.
 *
 * Returns:
 *   true  -> a request is still pending
 *   false -> no request is pending
 *
 * If the worker has finished, this function resolves/rejects the Promise.
 */
static JSValue vitajs_net_pump(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
    {
        return JS_ThrowSyntaxError(
            ctx,
            "__pump()"
        );
    }

    NetRequestState state;

    int status = 0;
    char *body = NULL;
    size_t body_length = 0;

    int error_code = 0;
    char error_operation[96];
    error_operation[0] = '\0';

    JSValue resolve = JS_UNDEFINED;
    JSValue reject = JS_UNDEFINED;

    lock_request();

    state = request.state;

    if (
        state == NET_REQUEST_DONE ||
        state == NET_REQUEST_ERROR
    )
    {
        status =
            request.status;

        body =
            request.response_body;

        body_length =
            request.response_length;

        error_code =
            request.error_code;

        snprintf(
            error_operation,
            sizeof(error_operation),
            "%s",
            request.error_operation
        );

        resolve =
            request.resolve;

        reject =
            request.reject;

        request.resolve =
            JS_UNDEFINED;

        request.reject =
            JS_UNDEFINED;

        request.response_body =
            NULL;

        request.response_length =
            0;

        if (request.url)
        {
            free(request.url);
            request.url = NULL;
        }

        request.state =
            NET_REQUEST_IDLE;
    }

    unlock_request();

    if (state == NET_REQUEST_RUNNING)
        return JS_TRUE;

    if (state == NET_REQUEST_DONE)
    {
        JSValue response =
            make_response(
                ctx,
                status,
                body,
                body_length
            );

        if (body)
            free(body);

        if (JS_IsException(response))
        {
            JSValue exception =
                JS_GetException(ctx);

            JSValue call_result =
                JS_Call(
                    ctx,
                    reject,
                    JS_UNDEFINED,
                    1,
                    (JSValueConst *)&exception
                );

            JS_FreeValue(
                ctx,
                call_result
            );

            JS_FreeValue(
                ctx,
                exception
            );
        }
        else
        {
            JSValue call_result =
                JS_Call(
                    ctx,
                    resolve,
                    JS_UNDEFINED,
                    1,
                    (JSValueConst *)&response
                );

            JS_FreeValue(
                ctx,
                call_result
            );

            JS_FreeValue(
                ctx,
                response
            );
        }

        JS_FreeValue(ctx, resolve);
        JS_FreeValue(ctx, reject);

        return JS_FALSE;
    }

    if (state == NET_REQUEST_ERROR)
    {
        if (body)
            free(body);

        JSValue error =
            make_error(
                ctx,
                error_operation,
                error_code
            );

        if (!JS_IsException(error))
        {
            JSValue call_result =
                JS_Call(
                    ctx,
                    reject,
                    JS_UNDEFINED,
                    1,
                    (JSValueConst *)&error
                );

            JS_FreeValue(
                ctx,
                call_result
            );

            JS_FreeValue(
                ctx,
                error
            );
        }

        JS_FreeValue(ctx, resolve);
        JS_FreeValue(ctx, reject);

        return JS_FALSE;
    }

    return JS_FALSE;
}

static JSValue vitajs_net_is_connected(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "is_connected()");

    if (net_ensure_initialized(ctx) < 0)
        return JS_EXCEPTION;

    int state = 0;

    int ret =
        sceNetCtlInetGetState(
            &state
        );

    if (ret < 0)
    {
        return JS_ThrowInternalError(
            ctx,
            "sceNetCtlInetGetState failed: 0x%08X",
            (unsigned int)ret
        );
    }

    return JS_NewBool(
        ctx,
        state == SCE_NETCTL_STATE_CONNECTED
    );
}

static JSValue vitajs_net_get_ip(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "get_ip()");

    if (net_ensure_initialized(ctx) < 0)
        return JS_EXCEPTION;

    SceNetCtlInfo info;
    memset(
        &info,
        0,
        sizeof(info)
    );

    int ret =
        sceNetCtlInetGetInfo(
            SCE_NETCTL_INFO_GET_IP_ADDRESS,
            &info
        );

    if (ret < 0)
    {
        return JS_ThrowInternalError(
            ctx,
            "sceNetCtlInetGetInfo failed: 0x%08X",
            (unsigned int)ret
        );
    }

    return JS_NewString(
        ctx,
        info.ip_address
    );
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("fetch", 1, vitajs_net_fetch),
    JS_CFUNC_DEF("__pump", 0, vitajs_net_pump),
    JS_CFUNC_DEF("is_connected", 0, vitajs_net_is_connected),
    JS_CFUNC_DEF("get_ip", 0, vitajs_net_get_ip),
};

static int module_init(
    JSContext *ctx,
    JSModuleDef *m
)
{
    return JS_SetModuleExportList(
        ctx,
        m,
        module_funcs,
        countof(module_funcs)
    );
}

JSModuleDef *vitajs_net_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        module_init,
        module_funcs,
        countof(module_funcs),
        "Net"
    );
}
