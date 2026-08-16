#include <stdint.h>
#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/kernel/processmgr.h>

#include "../env.h"

static JSValue throw_sce_error(JSContext *ctx, const char *name, int error)
{
    return JS_ThrowInternalError(
        ctx,
        "%s failed: 0x%08X",
        name,
        (unsigned int)error);
}

static JSValue vitajs_app_exit(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc > 1)
        return JS_ThrowSyntaxError(ctx, "exit([code]) expects zero or one argument");

    int32_t code = 0;
    if (argc == 1 && JS_ToInt32(ctx, &code, argv[0]))
        return JS_EXCEPTION;

    int result = sceKernelExitProcess(code);
    if (result < 0)
        return throw_sce_error(ctx, "sceKernelExitProcess", result);

    return JS_UNDEFINED;
}

static JSValue vitajs_app_get_launch_params(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "getLaunchParams() expects no arguments");

    char params[1024];
    memset(params, 0, sizeof(params));

    int result = sceAppMgrGetAppParam(params);
    if (result < 0)
        return throw_sce_error(ctx, "sceAppMgrGetAppParam", result);

    params[sizeof(params) - 1] = '\0';
    return JS_NewString(ctx, params);
}

static const char *system_event_name(int event)
{
    switch (event)
    {
        case SCE_APPMGR_SYSTEMEVENT_ON_RESUME:
            return "resume";
        case SCE_APPMGR_SYSTEMEVENT_ON_STORE_PURCHASE:
            return "storePurchase";
        case SCE_APPMGR_SYSTEMEVENT_ON_NP_MESSAGE_ARRIVED:
            return "npMessageArrived";
        case SCE_APPMGR_SYSTEMEVENT_ON_STORE_REDEMPTION:
            return "storeRedemption";
        default:
            return "unknown";
    }
}

static JSValue vitajs_app_poll_event(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "pollEvent() expects no arguments");

    SceAppMgrSystemEvent event;
    memset(&event, 0, sizeof(event));

    int result = sceAppMgrReceiveSystemEvent(&event);

    /*
     * AppMgr returns a negative value when there is no event available.
     * Polling code should therefore be able to call this every frame without
     * wrapping it in try/catch.
     */
    if (result < 0)
        return JS_NULL;

    JSValue object = JS_NewObject(ctx);
    if (JS_IsException(object))
        return object;

    if (JS_SetPropertyStr(
            ctx,
            object,
            "type",
            JS_NewString(ctx, system_event_name(event.systemEvent))) < 0)
    {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }

    if (JS_SetPropertyStr(
            ctx,
            object,
            "rawType",
            JS_NewInt32(ctx, event.systemEvent)) < 0)
    {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }

    return object;
}

static JSValue vitajs_app_set_infobar(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1 || !JS_IsObject(argv[0]))
        return JS_ThrowSyntaxError(ctx, "setInfoBar(options) expects one object argument");

    int visibility = SCE_APPMGR_INFOBAR_VISIBILITY_VISIBLE;
    int color = SCE_APPMGR_INFOBAR_COLOR_BLACK;
    int transparency = SCE_APPMGR_INFOBAR_TRANSPARENCY_OPAQUE;

    JSValue value = JS_GetPropertyStr(ctx, argv[0], "visible");
    if (!JS_IsUndefined(value))
    {
        int visible = JS_ToBool(ctx, value);
        JS_FreeValue(ctx, value);
        if (visible < 0)
            return JS_EXCEPTION;
        visibility = visible
            ? SCE_APPMGR_INFOBAR_VISIBILITY_VISIBLE
            : SCE_APPMGR_INFOBAR_VISIBILITY_INVISIBLE;
    }
    else
    {
        JS_FreeValue(ctx, value);
    }

    value = JS_GetPropertyStr(ctx, argv[0], "color");
    if (!JS_IsUndefined(value))
    {
        const char *color_string = JS_ToCString(ctx, value);
        JS_FreeValue(ctx, value);
        if (!color_string)
            return JS_EXCEPTION;

        if (strcmp(color_string, "black") == 0)
            color = SCE_APPMGR_INFOBAR_COLOR_BLACK;
        else if (strcmp(color_string, "white") == 0)
            color = SCE_APPMGR_INFOBAR_COLOR_WHITE;
        else
        {
            JS_FreeCString(ctx, color_string);
            return JS_ThrowRangeError(ctx, "color must be 'black' or 'white'");
        }

        JS_FreeCString(ctx, color_string);
    }
    else
    {
        JS_FreeValue(ctx, value);
    }

    value = JS_GetPropertyStr(ctx, argv[0], "translucent");
    if (!JS_IsUndefined(value))
    {
        int translucent = JS_ToBool(ctx, value);
        JS_FreeValue(ctx, value);
        if (translucent < 0)
            return JS_EXCEPTION;
        transparency = translucent
            ? SCE_APPMGR_INFOBAR_TRANSPARENCY_TRANSLUCENT
            : SCE_APPMGR_INFOBAR_TRANSPARENCY_OPAQUE;
    }
    else
    {
        JS_FreeValue(ctx, value);
    }

    int result = sceAppMgrSetInfobarState(
        (SceAppMgrInfoBarVisibility)visibility,
        (SceAppMgrInfoBarColor)color,
        (SceAppMgrInfoBarTransparency)transparency);

    if (result < 0)
        return throw_sce_error(ctx, "sceAppMgrSetInfobarState", result);

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("exit", 1, vitajs_app_exit),
    JS_CFUNC_DEF("getLaunchParams", 0, vitajs_app_get_launch_params),
    JS_CFUNC_DEF("pollEvent", 0, vitajs_app_poll_event),
    JS_CFUNC_DEF("setInfoBar", 1, vitajs_app_set_infobar),

    JS_PROP_INT32_DEF(
        "EVENT_RESUME",
        SCE_APPMGR_SYSTEMEVENT_ON_RESUME,
        JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF(
        "EVENT_STORE_PURCHASE",
        SCE_APPMGR_SYSTEMEVENT_ON_STORE_PURCHASE,
        JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF(
        "EVENT_NP_MESSAGE_ARRIVED",
        SCE_APPMGR_SYSTEMEVENT_ON_NP_MESSAGE_ARRIVED,
        JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF(
        "EVENT_STORE_REDEMPTION",
        SCE_APPMGR_SYSTEMEVENT_ON_STORE_REDEMPTION,
        JS_PROP_CONFIGURABLE),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(
        ctx,
        m,
        module_funcs,
        countof(module_funcs));
}

JSModuleDef *vitajs_app_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        module_init,
        module_funcs,
        countof(module_funcs),
        "App");
}
