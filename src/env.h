#include <stdbool.h>
#include <psp2common/ctrl.h>
#include "../quickjs/quickjs.h"
#include "../quickjs/quickjs-libc.h"
#include "../quickjs/cutils.h"

/*
 * PSVitaJS compatibility helpers.
 *
 * Older PSVitaJS QuickJS fork exposed JS_ToFloat32 / JS_NewFloat32.
 * Upstream QuickJS uses Number (double), so convert at the C API boundary.
 */
static inline int JS_ToFloat32(
    JSContext *ctx,
    float *pres,
    JSValueConst val)
{
    double value;

    int ret = JS_ToFloat64(
        ctx,
        &value,
        val
    );

    if (ret < 0)
        return ret;

    *pres = (float)value;

    return 0;
}

static inline JSValue JS_NewFloat32(
    JSContext *ctx,
    float value)
{
    return JS_NewFloat64(
        ctx,
        (double)value
    );
}

static SceCtrlData ctrl;

unsigned int isButtonPressed();

static JSContext *JS_NewCustomContext(JSRuntime *rt);
JSModuleDef *vitajs_push_module(JSContext *ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char *module_name);

/*
 * Single source of truth for native VitaJS modules.
 * Adding a module to modules.def declares its init function here as well.
 */
#define VITAJS_MODULE(js_name, init_fn) JSModuleDef *init_fn(JSContext *ctx);
#include "modules/modules.def"
#undef VITAJS_MODULE

/* Kept for compatibility with existing code outside the module registry. */
JSModuleDef *vitajs_render_init(JSContext *ctx);

const char *runScript(const char *script);
static int qjs_handle_file(JSContext *ctx, const char *filename);
static int qjs_handle_fh(JSContext *ctx, FILE *f, const char *filename);
static int qjs_eval_buf(JSContext *ctx, const void *buf, size_t buf_len, const char *filename, int eval_flags);

unsigned int get_used_memory();
unsigned int get_free_memory();
unsigned int get_used_vram();
unsigned int get_free_vram();

void delay(int timer);
void delayMiliseconds(int timer);
