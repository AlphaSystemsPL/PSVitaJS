#include <stdint.h>
#include <string.h>
#include <psp2/touch.h>
#include "../env.h"

static JSValue throw_sce_error(JSContext *ctx, const char *name, int error)
{
	return JS_ThrowInternalError(ctx, "%s failed: 0x%08X", name, (unsigned int)error);
}
static int parse_port(JSContext *ctx, JSValueConst value, int *port)
{
	int32_t p;
	if (JS_ToInt32(ctx, &p, value))
		return -1;
	if (p != SCE_TOUCH_PORT_FRONT && p != SCE_TOUCH_PORT_BACK)
	{
		JS_ThrowRangeError(ctx, "touch port must be Touch.FRONT or Touch.BACK");
		return -1;
	}
	*port = p;
	return 0;
}
static JSValue make_touch_data(JSContext *ctx, int port, const SceTouchData *data)
{
	JSValue obj = JS_NewObject(ctx);
	JSValue arr = JS_NewArray(ctx);
	if (JS_IsException(obj) || JS_IsException(arr))
	{
		JS_FreeValue(ctx, obj);
		JS_FreeValue(ctx, arr);
		return JS_EXCEPTION;
	}
	JS_SetPropertyStr(ctx, obj, "port", JS_NewInt32(ctx, port));
	JS_SetPropertyStr(ctx, obj, "timestamp", JS_NewInt64(ctx, (int64_t)data->timeStamp));
	JS_SetPropertyStr(ctx, obj, "status", JS_NewUint32(ctx, data->status));
	JS_SetPropertyStr(ctx, obj, "count", JS_NewUint32(ctx, data->reportNum));
	uint32_t n = data->reportNum > SCE_TOUCH_MAX_REPORT ? SCE_TOUCH_MAX_REPORT : data->reportNum;
	for (uint32_t i = 0; i < n; i++)
	{
		const SceTouchReport *r = &data->report[i];
		JSValue t = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, t, "id", JS_NewUint32(ctx, r->id));
		JS_SetPropertyStr(ctx, t, "x", JS_NewInt32(ctx, r->x));
		JS_SetPropertyStr(ctx, t, "y", JS_NewInt32(ctx, r->y));
		JS_SetPropertyStr(ctx, t, "force", JS_NewUint32(ctx, r->force));
		JS_SetPropertyStr(ctx, t, "info", JS_NewUint32(ctx, r->info));
		JS_SetPropertyUint32(ctx, arr, i, t);
	}
	JS_SetPropertyStr(ctx, obj, "touches", arr);
	return obj;
}
static JSValue do_read(JSContext *ctx, int argc, JSValueConst *argv, int blocking)
{
	int port = SCE_TOUCH_PORT_FRONT;
	if (argc > 1)
		return JS_ThrowSyntaxError(ctx, blocking ? "read([port]) expects 0-1 arguments" : "peek([port]) expects 0-1 arguments");
	if (argc == 1 && parse_port(ctx, argv[0], &port) < 0)
		return JS_EXCEPTION;
	SceTouchData data;
	memset(&data, 0, sizeof(data));
	int ret = blocking ? sceTouchRead(port, &data, 1) : sceTouchPeek(port, &data, 1);
	if (ret < 0)
		return throw_sce_error(ctx, blocking ? "sceTouchRead" : "sceTouchPeek", ret);
	return make_touch_data(ctx, port, &data);
}
static JSValue vitajs_peek(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return do_read(ctx, a, v, 0);
}
static JSValue vitajs_read(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return do_read(ctx, a, v, 1);
}
static JSValue vitajs_front(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	(void)v;
	if (a)
		return JS_ThrowSyntaxError(ctx, "front() expects no arguments");
	SceTouchData d;
	memset(&d, 0, sizeof(d));
	int r = sceTouchPeek(SCE_TOUCH_PORT_FRONT, &d, 1);
	if (r < 0)
		return throw_sce_error(ctx, "sceTouchPeek", r);
	return make_touch_data(ctx, SCE_TOUCH_PORT_FRONT, &d);
}
static JSValue vitajs_back(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	(void)v;
	if (a)
		return JS_ThrowSyntaxError(ctx, "back() expects no arguments");
	SceTouchData d;
	memset(&d, 0, sizeof(d));
	int r = sceTouchPeek(SCE_TOUCH_PORT_BACK, &d, 1);
	if (r < 0)
		return throw_sce_error(ctx, "sceTouchPeek", r);
	return make_touch_data(ctx, SCE_TOUCH_PORT_BACK, &d);
}
static JSValue vitajs_panel(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(ctx, "getPanelInfo(port) expects one argument");
	int p;
	if (parse_port(ctx, v[0], &p) < 0)
		return JS_EXCEPTION;
	SceTouchPanelInfo i;
	memset(&i, 0, sizeof(i));
	int r = sceTouchGetPanelInfo(p, &i);
	if (r < 0)
		return throw_sce_error(ctx, "sceTouchGetPanelInfo", r);
	JSValue o = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, o, "minAaX", JS_NewInt32(ctx, i.minAaX));
	JS_SetPropertyStr(ctx, o, "minAaY", JS_NewInt32(ctx, i.minAaY));
	JS_SetPropertyStr(ctx, o, "maxAaX", JS_NewInt32(ctx, i.maxAaX));
	JS_SetPropertyStr(ctx, o, "maxAaY", JS_NewInt32(ctx, i.maxAaY));
	JS_SetPropertyStr(ctx, o, "minDispX", JS_NewInt32(ctx, i.minDispX));
	JS_SetPropertyStr(ctx, o, "minDispY", JS_NewInt32(ctx, i.minDispY));
	JS_SetPropertyStr(ctx, o, "maxDispX", JS_NewInt32(ctx, i.maxDispX));
	JS_SetPropertyStr(ctx, o, "maxDispY", JS_NewInt32(ctx, i.maxDispY));
	JS_SetPropertyStr(ctx, o, "minForce", JS_NewUint32(ctx, i.minForce));
	JS_SetPropertyStr(ctx, o, "maxForce", JS_NewUint32(ctx, i.maxForce));
	return o;
}
static JSValue vitajs_set_sampling(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(ctx, "setSampling(port, enabled) expects two arguments");
	int p;
	if (parse_port(ctx, v[0], &p) < 0)
		return JS_EXCEPTION;
	int b = JS_ToBool(ctx, v[1]);
	if (b < 0)
		return JS_EXCEPTION;
	int r = sceTouchSetSamplingState(p, b ? SCE_TOUCH_SAMPLING_STATE_START : SCE_TOUCH_SAMPLING_STATE_STOP);
	if (r < 0)
		return throw_sce_error(ctx, "sceTouchSetSamplingState", r);
	return JS_UNDEFINED;
}
static JSValue vitajs_get_sampling(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(ctx, "getSampling(port) expects one argument");
	int p;
	if (parse_port(ctx, v[0], &p) < 0)
		return JS_EXCEPTION;
	SceTouchSamplingState s;
	int r = sceTouchGetSamplingState(p, &s);
	if (r < 0)
		return throw_sce_error(ctx, "sceTouchGetSamplingState", r);
	return JS_NewBool(ctx, s == SCE_TOUCH_SAMPLING_STATE_START);
}
static JSValue vitajs_force(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(ctx, "setForce(port, enabled) expects two arguments");
	int p;
	if (parse_port(ctx, v[0], &p) < 0)
		return JS_EXCEPTION;
	int b = JS_ToBool(ctx, v[1]);
	if (b < 0)
		return JS_EXCEPTION;
	int r = b ? sceTouchEnableTouchForce(p) : sceTouchDisableTouchForce(p);
	if (r < 0)
		return throw_sce_error(ctx, b ? "sceTouchEnableTouchForce" : "sceTouchDisableTouchForce", r);
	return JS_UNDEFINED;
}
static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("peek", 1, vitajs_peek), JS_CFUNC_DEF("read", 1, vitajs_read), JS_CFUNC_DEF("front", 0, vitajs_front), JS_CFUNC_DEF("back", 0, vitajs_back), JS_CFUNC_DEF("getPanelInfo", 1, vitajs_panel), JS_CFUNC_DEF("setSampling", 2, vitajs_set_sampling), JS_CFUNC_DEF("getSampling", 1, vitajs_get_sampling), JS_CFUNC_DEF("setForce", 2, vitajs_force),
	JS_PROP_INT32_DEF("FRONT", SCE_TOUCH_PORT_FRONT, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("BACK", SCE_TOUCH_PORT_BACK, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("INFO_HIDE_UPPER_LAYER", SCE_TOUCH_REPORT_INFO_HIDE_UPPER_LAYER, JS_PROP_CONFIGURABLE)};
static int module_init(JSContext *ctx, JSModuleDef *m) { return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs)); }
JSModuleDef *vitajs_touch_init(JSContext *ctx) { return vitajs_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Touch"); }
