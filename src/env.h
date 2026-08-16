#include <stdbool.h>
#include <psp2common/ctrl.h>
#include "../quickjs/quickjs.h"
#include "../quickjs/quickjs-libc.h"
#include "../quickjs/cutils.h"

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
static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len, const char *filename, int eval_flags);

unsigned int get_used_memory();
unsigned int get_free_memory();
unsigned int get_used_vram();
unsigned int get_free_vram();

void delay(int timer);
void delayMiliseconds(int timer);
