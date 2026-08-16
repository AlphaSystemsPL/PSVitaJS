#include <stdint.h>
#include <stdlib.h>

#include "../graphics.h"
#include "../env.h"

typedef struct FontData
{
    vita2d_font *font;
} FontData;

static JSClassID js_font_class_id = 0;

static void vitajs_font_finalizer(JSRuntime *rt, JSValue val)
{
    FontData *data = JS_GetOpaque(val, js_font_class_id);

    if (!data)
        return;

    if (data->font)
    {
        vita2d_free_font(data->font);
        data->font = NULL;
    }

    js_free_rt(rt, data);
}

static JSClassDef js_font_class = {
    .class_name = "VitaJSFont",
    .finalizer = vitajs_font_finalizer,
};

static FontData *vitajs_get_font_data(JSContext *ctx, JSValueConst value)
{
    FontData *data = JS_GetOpaque2(ctx, value, js_font_class_id);

    if (!data)
        return NULL;

    if (!data->font)
    {
        JS_ThrowTypeError(ctx, "Font has already been freed");
        return NULL;
    }

    return data;
}

static JSValue vitajs_load_font_file(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "load_font_file(filename: string) expects exactly one argument");

    const char *filename = JS_ToCString(ctx, argv[0]);

    if (!filename)
        return JS_EXCEPTION;

    vita2d_font *font = vita2d_load_font_file(filename);

    JS_FreeCString(ctx, filename);

    if (!font)
        return JS_ThrowInternalError(ctx, "Unable to load font file");

    JSValue obj = JS_NewObjectClass(ctx, js_font_class_id);

    if (JS_IsException(obj))
    {
        vita2d_free_font(font);
        return obj;
    }

    FontData *data = js_mallocz(ctx, sizeof(*data));

    if (!data)
    {
        vita2d_free_font(font);
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    data->font = font;
    JS_SetOpaque(obj, data);

    return obj;
}

static JSValue vitajs_free_font(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "free_font(font) expects exactly one argument");

    FontData *data = JS_GetOpaque2(ctx, argv[0], js_font_class_id);

    if (!data)
        return JS_EXCEPTION;

    if (data->font)
    {
        vita2d_free_font(data->font);
        data->font = NULL;
    }

    /*
     * Do NOT JS_FreeValue(argv[0]) here.
     * argv values are borrowed from QuickJS.
     * The object remains valid but represents an explicitly freed font.
     */
    return JS_UNDEFINED;
}

static JSValue vitajs_font_draw_text(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 6)
        return JS_ThrowSyntaxError(
            ctx,
            "font_draw_text(font, x, y, color, size, text) expects six arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    int32_t x;
    int32_t y;
    uint32_t color;
    uint32_t size;

    if (JS_ToInt32(ctx, &x, argv[1]))
        return JS_EXCEPTION;

    if (JS_ToInt32(ctx, &y, argv[2]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &color, argv[3]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &size, argv[4]))
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[5]);

    if (!text)
        return JS_EXCEPTION;

    vita2d_font_draw_text(
        data->font,
        x,
        y,
        color,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_draw_text_ls(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 7)
        return JS_ThrowSyntaxError(
            ctx,
            "font_draw_text_ls(font, x, y, lineSpace, color, size, text) expects seven arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    int32_t x;
    int32_t y;
    float line_space;
    uint32_t color;
    uint32_t size;

    if (JS_ToInt32(ctx, &x, argv[1]))
        return JS_EXCEPTION;

    if (JS_ToInt32(ctx, &y, argv[2]))
        return JS_EXCEPTION;

    if (JS_ToFloat32(ctx, &line_space, argv[3]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &color, argv[4]))
        return JS_EXCEPTION;

    if (JS_ToUint32(ctx, &size, argv[5]))
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[6]);

    if (!text)
        return JS_EXCEPTION;

    vita2d_font_draw_text_ls(
        data->font,
        x,
        y,
        line_space,
        color,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_UNDEFINED;
}

static JSValue vitajs_font_text_dimensions(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 3)
        return JS_ThrowSyntaxError(
            ctx,
            "font_text_dimensions(font, size, text) expects three arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    uint32_t size;

    if (JS_ToUint32(ctx, &size, argv[1]))
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[2]);

    if (!text)
        return JS_EXCEPTION;

    int width = 0;
    int height = 0;

    vita2d_font_text_dimensions(
        data->font,
        size,
        text,
        &width,
        &height);

    JS_FreeCString(ctx, text);

    JSValue result = JS_NewObject(ctx);

    if (JS_IsException(result))
        return result;

    if (JS_SetPropertyStr(
            ctx,
            result,
            "width",
            JS_NewInt32(ctx, width)) < 0)
    {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }

    if (JS_SetPropertyStr(
            ctx,
            result,
            "height",
            JS_NewInt32(ctx, height)) < 0)
    {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }

    return result;
}

static JSValue vitajs_font_text_width(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 3)
        return JS_ThrowSyntaxError(
            ctx,
            "font_text_width(font, size, text) expects three arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    uint32_t size;

    if (JS_ToUint32(ctx, &size, argv[1]))
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[2]);

    if (!text)
        return JS_EXCEPTION;

    int width = vita2d_font_text_width(
        data->font,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, width);
}

static JSValue vitajs_font_text_height(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 3)
        return JS_ThrowSyntaxError(
            ctx,
            "font_text_height(font, size, text) expects three arguments");

    FontData *data = vitajs_get_font_data(ctx, argv[0]);

    if (!data)
        return JS_EXCEPTION;

    uint32_t size;

    if (JS_ToUint32(ctx, &size, argv[1]))
        return JS_EXCEPTION;

    const char *text = JS_ToCString(ctx, argv[2]);

    if (!text)
        return JS_EXCEPTION;

    int height = vita2d_font_text_height(
        data->font,
        size,
        text);

    JS_FreeCString(ctx, text);

    return JS_NewInt32(ctx, height);
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("load_font_file", 1, vitajs_load_font_file),
    JS_CFUNC_DEF("free_font", 1, vitajs_free_font),
    JS_CFUNC_DEF("font_draw_text", 6, vitajs_font_draw_text),
    JS_CFUNC_DEF("font_draw_text_ls", 7, vitajs_font_draw_text_ls),
    JS_CFUNC_DEF("font_text_dimensions", 3, vitajs_font_text_dimensions),
    JS_CFUNC_DEF("font_text_width", 3, vitajs_font_text_width),
    JS_CFUNC_DEF("font_text_height", 3, vitajs_font_text_height),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    if (js_font_class_id == 0)
        JS_NewClassID(&js_font_class_id);

    /*
     * Class IDs are global, class registrations are per JSRuntime.
     * This also makes the module safe if a second context is created
     * in the same runtime.
     */
    if (!JS_IsRegisteredClass(rt, js_font_class_id))
    {
        if (JS_NewClass(rt, js_font_class_id, &js_font_class) < 0)
            return -1;
    }

    JSValue proto = JS_NewObject(ctx);

    if (JS_IsException(proto))
        return -1;

    JS_SetClassProto(ctx, js_font_class_id, proto);

    return JS_SetModuleExportList(
        ctx,
        m,
        module_funcs,
        countof(module_funcs));
}

JSModuleDef *vitajs_font_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        module_init,
        module_funcs,
        countof(module_funcs),
        "Font");
}
