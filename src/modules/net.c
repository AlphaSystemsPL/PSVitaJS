#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/net/http.h>

#include "../env.h"

#define VITAJS_NET_POOL_SIZE      (1024 * 1024)
#define VITAJS_HTTP_POOL_SIZE     (1024 * 1024)
#define VITAJS_HTTP_READ_CHUNK    (16 * 1024)
#define VITAJS_HTTP_MAX_RESPONSE  (8 * 1024 * 1024)

static void *net_memory = NULL;
static int net_initialized = 0;
static int netctl_initialized = 0;
static int http_initialized = 0;
static int net_module_loaded = 0;
static int http_module_loaded = 0;

static JSValue throw_sce_error(JSContext *ctx, const char *operation, int code)
{
    return JS_ThrowInternalError(
        ctx,
        "%s failed: 0x%08X",
        operation,
        (unsigned int)code
    );
}

static int net_ensure_initialized(JSContext *ctx)
{
    int ret;

    if (http_initialized)
        return 0;

    if (!net_module_loaded)
    {
        ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
        if (ret < 0)
        {
            throw_sce_error(ctx, "sceSysmoduleLoadModule(SCE_SYSMODULE_NET)", ret);
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

            throw_sce_error(ctx, "sceNetInit", ret);
            return -1;
        }

        net_initialized = 1;
    }

    if (!netctl_initialized)
    {
        ret = sceNetCtlInit();

        if (ret < 0)
        {
            throw_sce_error(ctx, "sceNetCtlInit", ret);
            return -1;
        }

        netctl_initialized = 1;
    }

    if (!http_module_loaded)
    {
        ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

        if (ret < 0)
        {
            throw_sce_error(ctx, "sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP)", ret);
            return -1;
        }

        http_module_loaded = 1;
    }

    if (!http_initialized)
    {
        ret = sceHttpInit(VITAJS_HTTP_POOL_SIZE);

        if (ret < 0 && ret != (int)SCE_HTTP_ERROR_ALREADY_INITED)
        {
            throw_sce_error(ctx, "sceHttpInit", ret);
            return -1;
        }

        http_initialized = 1;
    }

    return 0;
}

static void net_shutdown(void)
{
    if (http_initialized)
    {
        sceHttpTerm();
        http_initialized = 0;
    }

    if (http_module_loaded)
    {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
        http_module_loaded = 0;
    }

    if (netctl_initialized)
    {
        sceNetCtlTerm();
        netctl_initialized = 0;
    }

    if (net_initialized)
    {
        sceNetTerm();
        net_initialized = 0;
    }

    if (net_memory)
    {
        free(net_memory);
        net_memory = NULL;
    }

    if (net_module_loaded)
    {
        sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
        net_module_loaded = 0;
    }
}

static int method_from_string(const char *method)
{
    if (!strcmp(method, "GET"))
        return SCE_HTTP_METHOD_GET;

    if (!strcmp(method, "POST"))
        return SCE_HTTP_METHOD_POST;

    if (!strcmp(method, "PUT"))
        return SCE_HTTP_METHOD_PUT;

    if (!strcmp(method, "DELETE"))
        return SCE_HTTP_METHOD_DELETE;

    if (!strcmp(method, "HEAD"))
        return SCE_HTTP_METHOD_HEAD;

    if (!strcmp(method, "OPTIONS"))
        return SCE_HTTP_METHOD_OPTIONS;

    return -1;
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

    if (JS_SetPropertyStr(
            ctx,
            response,
            "status",
            JS_NewInt32(ctx, status)) < 0)
    {
        JS_FreeValue(ctx, response);
        return JS_EXCEPTION;
    }

    if (JS_SetPropertyStr(
            ctx,
            response,
            "ok",
            JS_NewBool(ctx, status >= 200 && status < 300)) < 0)
    {
        JS_FreeValue(ctx, response);
        return JS_EXCEPTION;
    }

    if (JS_SetPropertyStr(
            ctx,
            response,
            "body",
            JS_NewStringLen(ctx, body ? body : "", body_length)) < 0)
    {
        JS_FreeValue(ctx, response);
        return JS_EXCEPTION;
    }

    return response;
}

static JSValue net_request_native(
    JSContext *ctx,
    const char *method_string,
    const char *url,
    const char *body,
    const char *content_type
)
{
    int method = method_from_string(method_string);

    if (method < 0)
        return JS_ThrowTypeError(ctx, "Unsupported HTTP method: %s", method_string);

    if (net_ensure_initialized(ctx) < 0)
        return JS_EXCEPTION;

    int tpl = -1;
    int conn = -1;
    int req = -1;
    int ret = 0;
    int status = 0;

    char *response_body = NULL;
    size_t response_length = 0;
    size_t response_capacity = 0;

    const char *failed_operation = NULL;

    size_t request_body_length = body ? strlen(body) : 0;

    tpl = sceHttpCreateTemplate(
        "VitaJS/1.0",
        SCE_HTTP_VERSION_1_1,
        1
    );

    if (tpl < 0)
    {
        ret = tpl;
        failed_operation = "sceHttpCreateTemplate";
        goto fail;
    }

    conn = sceHttpCreateConnectionWithURL(tpl, url, 0);

    if (conn < 0)
    {
        ret = conn;
        failed_operation = "sceHttpCreateConnectionWithURL";
        goto fail;
    }

    req = sceHttpCreateRequestWithURL(
        conn,
        method,
        url,
        (unsigned long long)request_body_length
    );

    if (req < 0)
    {
        ret = req;
        failed_operation = "sceHttpCreateRequestWithURL";
        goto fail;
    }

    ret = sceHttpSetAutoRedirect(req, SCE_HTTP_ENABLE);

    if (ret < 0)
    {
        failed_operation = "sceHttpSetAutoRedirect";
        goto fail;
    }

    ret = sceHttpAddRequestHeader(
        req,
        "Accept",
        "*/*",
        SCE_HTTP_HEADER_OVERWRITE
    );

    if (ret < 0)
    {
        failed_operation = "sceHttpAddRequestHeader(Accept)";
        goto fail;
    }

    if (body && request_body_length > 0)
    {
        ret = sceHttpAddRequestHeader(
            req,
            "Content-Type",
            content_type ? content_type : "text/plain; charset=utf-8",
            SCE_HTTP_HEADER_OVERWRITE
        );

        if (ret < 0)
        {
            failed_operation = "sceHttpAddRequestHeader(Content-Type)";
            goto fail;
        }
    }

    ret = sceHttpSendRequest(
        req,
        body,
        (unsigned int)request_body_length
    );

    if (ret < 0)
    {
        failed_operation = "sceHttpSendRequest";
        goto fail;
    }

    ret = sceHttpGetStatusCode(req, &status);

    if (ret < 0)
    {
        failed_operation = "sceHttpGetStatusCode";
        goto fail;
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
            ret = bytes_read;
            failed_operation = "sceHttpReadData";
            goto fail;
        }

        if (bytes_read == 0)
            break;

        size_t required =
            response_length + (size_t)bytes_read + 1;

        if (required > VITAJS_HTTP_MAX_RESPONSE + 1)
        {
            ret = -1;
            failed_operation = "HTTP response exceeds VitaJS 8 MB limit";
            goto fail;
        }

        if (required > response_capacity)
        {
            size_t new_capacity =
                response_capacity == 0
                    ? VITAJS_HTTP_READ_CHUNK
                    : response_capacity * 2;

            while (new_capacity < required)
                new_capacity *= 2;

            if (new_capacity > VITAJS_HTTP_MAX_RESPONSE + 1)
                new_capacity = VITAJS_HTTP_MAX_RESPONSE + 1;

            char *new_body =
                realloc(response_body, new_capacity);

            if (!new_body)
            {
                if (response_body)
                    free(response_body);

                if (req >= 0)
                    sceHttpDeleteRequest(req);

                if (conn >= 0)
                    sceHttpDeleteConnection(conn);

                if (tpl >= 0)
                    sceHttpDeleteTemplate(tpl);

                return JS_ThrowOutOfMemory(ctx);
            }

            response_body = new_body;
            response_capacity = new_capacity;
        }

        memcpy(
            response_body + response_length,
            chunk,
            (size_t)bytes_read
        );

        response_length += (size_t)bytes_read;
    }

    if (response_body)
        response_body[response_length] = '\0';

    JSValue response = make_response(
        ctx,
        status,
        response_body,
        response_length
    );

    if (response_body)
        free(response_body);

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);

    return response;

fail:
    if (response_body)
        free(response_body);

    if (req >= 0)
        sceHttpDeleteRequest(req);

    if (conn >= 0)
        sceHttpDeleteConnection(conn);

    if (tpl >= 0)
        sceHttpDeleteTemplate(tpl);

    if (ret == -1)
        return JS_ThrowInternalError(ctx, "%s", failed_operation);

    return throw_sce_error(ctx, failed_operation, ret);
}

static JSValue vitajs_net_request(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    if (argc < 2 || argc > 4)
    {
        return JS_ThrowSyntaxError(
            ctx,
            "request(method, url[, body[, contentType]])"
        );
    }

    const char *method = JS_ToCString(ctx, argv[0]);

    if (!method)
        return JS_EXCEPTION;

    const char *url = JS_ToCString(ctx, argv[1]);

    if (!url)
    {
        JS_FreeCString(ctx, method);
        return JS_EXCEPTION;
    }

    const char *body = NULL;
    const char *content_type = NULL;

    if (argc >= 3 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2]))
    {
        body = JS_ToCString(ctx, argv[2]);

        if (!body)
        {
            JS_FreeCString(ctx, url);
            JS_FreeCString(ctx, method);
            return JS_EXCEPTION;
        }
    }

    if (argc >= 4 && !JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3]))
    {
        content_type = JS_ToCString(ctx, argv[3]);

        if (!content_type)
        {
            if (body)
                JS_FreeCString(ctx, body);

            JS_FreeCString(ctx, url);
            JS_FreeCString(ctx, method);
            return JS_EXCEPTION;
        }
    }

    JSValue result =
        net_request_native(
            ctx,
            method,
            url,
            body,
            content_type
        );

    if (content_type)
        JS_FreeCString(ctx, content_type);

    if (body)
        JS_FreeCString(ctx, body);

    JS_FreeCString(ctx, url);
    JS_FreeCString(ctx, method);

    return result;
}

static JSValue vitajs_net_get(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "get(url)");

    const char *url = JS_ToCString(ctx, argv[0]);

    if (!url)
        return JS_EXCEPTION;

    JSValue result =
        net_request_native(
            ctx,
            "GET",
            url,
            NULL,
            NULL
        );

    JS_FreeCString(ctx, url);

    return result;
}

static JSValue vitajs_net_post(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    if (argc < 2 || argc > 3)
    {
        return JS_ThrowSyntaxError(
            ctx,
            "post(url, body[, contentType])"
        );
    }

    const char *url = JS_ToCString(ctx, argv[0]);

    if (!url)
        return JS_EXCEPTION;

    const char *body = JS_ToCString(ctx, argv[1]);

    if (!body)
    {
        JS_FreeCString(ctx, url);
        return JS_EXCEPTION;
    }

    const char *content_type = "application/json";

    if (argc == 3 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2]))
    {
        content_type = JS_ToCString(ctx, argv[2]);

        if (!content_type)
        {
            JS_FreeCString(ctx, body);
            JS_FreeCString(ctx, url);
            return JS_EXCEPTION;
        }
    }

    JSValue result =
        net_request_native(
            ctx,
            "POST",
            url,
            body,
            content_type
        );

    if (argc == 3 && content_type)
        JS_FreeCString(ctx, content_type);

    JS_FreeCString(ctx, body);
    JS_FreeCString(ctx, url);

    return result;
}

static JSValue vitajs_net_is_connected(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "is_connected()");

    if (net_ensure_initialized(ctx) < 0)
        return JS_EXCEPTION;

    int state = 0;
    int ret = sceNetCtlInetGetState(&state);

    if (ret < 0)
        return throw_sce_error(ctx, "sceNetCtlInetGetState", ret);

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
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "get_ip()");

    if (net_ensure_initialized(ctx) < 0)
        return JS_EXCEPTION;

    SceNetCtlInfo info;
    memset(&info, 0, sizeof(info));

    int ret = sceNetCtlInetGetInfo(
        SCE_NETCTL_INFO_GET_IP_ADDRESS,
        &info
    );

    if (ret < 0)
        return throw_sce_error(ctx, "sceNetCtlInetGetInfo", ret);

    return JS_NewString(ctx, info.ip_address);
}

static JSValue vitajs_net_term(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv
)
{
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "term()");

    net_shutdown();

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("request", 4, vitajs_net_request),
    JS_CFUNC_DEF("get", 1, vitajs_net_get),
    JS_CFUNC_DEF("post", 3, vitajs_net_post),
    JS_CFUNC_DEF("is_connected", 0, vitajs_net_is_connected),
    JS_CFUNC_DEF("get_ip", 0, vitajs_net_get_ip),
    JS_CFUNC_DEF("term", 0, vitajs_net_term),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
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
