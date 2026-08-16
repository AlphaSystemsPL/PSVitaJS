#include <stdint.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <psp2common/ctrl.h>
#include "../env.h"

static JSValue throw_sce_error(JSContext *ctx, const char *name, int error)
{
	return JS_ThrowInternalError(ctx, "%s failed: 0x%08X", name, (unsigned int)error);
}

static int read_pad(int port, SceCtrlData *out)
{
	memset(out, 0, sizeof(*out));
	if (port == 0)
		return sceCtrlPeekBufferPositiveExt(0, out, 1);
	return sceCtrlPeekBufferPositiveExt2(port, out, 1);
}

static int parse_port(JSContext *ctx, int argc, JSValueConst *argv, int index, int default_port, int min_port)
{
	int32_t port = default_port;
	if (argc > index && JS_ToInt32(ctx, &port, argv[index]))
		return -1;
	if (port < min_port || port > 5)
	{
		JS_ThrowRangeError(ctx, "controller port must be between %d and 5", min_port);
		return -1;
	}
	return port;
}

static JSValue make_pad_object(JSContext *ctx, int port, const SceCtrlData *pad)
{
	JSValue obj = JS_NewObject(ctx);
	if (JS_IsException(obj))
		return obj;
	JS_SetPropertyStr(ctx, obj, "port", JS_NewInt32(ctx, port));
	JS_SetPropertyStr(ctx, obj, "btns", JS_NewUint32(ctx, pad->buttons));
	JS_SetPropertyStr(ctx, obj, "buttons", JS_NewUint32(ctx, pad->buttons));
	JS_SetPropertyStr(ctx, obj, "lx", JS_NewUint32(ctx, pad->lx));
	JS_SetPropertyStr(ctx, obj, "ly", JS_NewUint32(ctx, pad->ly));
	JS_SetPropertyStr(ctx, obj, "rx", JS_NewUint32(ctx, pad->rx));
	JS_SetPropertyStr(ctx, obj, "ry", JS_NewUint32(ctx, pad->ry));
	JS_SetPropertyStr(ctx, obj, "timestamp", JS_NewInt64(ctx, (int64_t)pad->timeStamp));
	return obj;
}

static JSValue vitajs_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc > 1)
		return JS_ThrowSyntaxError(ctx, "read([port]) expects zero or one argument");
	int port = parse_port(ctx, argc, argv, 0, 0, 0);
	if (port < 0)
		return JS_EXCEPTION;
	SceCtrlData pad;
	int ret = read_pad(port, &pad);
	if (ret < 0)
		return throw_sce_error(ctx, port == 0 ? "sceCtrlPeekBufferPositiveExt" : "sceCtrlPeekBufferPositiveExt2", ret);
	return make_pad_object(ctx, port, &pad);
}

static JSValue vitajs_analog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return vitajs_read(ctx, this_val, argc, argv);
}

static JSValue vitajs_check(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc < 1 || argc > 2)
		return JS_ThrowSyntaxError(ctx, "check(button[, port]) expects one or two arguments");
	uint32_t button;
	if (JS_ToUint32(ctx, &button, argv[0]))
		return JS_EXCEPTION;
	int port = parse_port(ctx, argc, argv, 1, 0, 0);
	if (port < 0)
		return JS_EXCEPTION;
	SceCtrlData pad;
	int ret = read_pad(port, &pad);
	if (ret < 0)
		return throw_sce_error(ctx, "sceCtrlPeekBufferPositiveExt/Ext2", ret);
	return JS_NewBool(ctx, (pad.buttons & button) != 0);
}

static JSValue vitajs_rumble(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc < 2 || argc > 3)
		return JS_ThrowSyntaxError(ctx, "rumble(small, large[, port]) expects 2-3 arguments");
	int32_t small, large;
	if (JS_ToInt32(ctx, &small, argv[0]) || JS_ToInt32(ctx, &large, argv[1]))
		return JS_EXCEPTION;
	if (small < 0 || small > 255 || large < 0 || large > 255)
		return JS_ThrowRangeError(ctx, "small and large must be 0..255");
	int port = parse_port(ctx, argc, argv, 2, 1, 1);
	if (port < 0)
		return JS_EXCEPTION;
	SceCtrlActuator actuator = {(uint8_t)small, (uint8_t)large};
	int ret = sceCtrlSetActuator(port, &actuator);
	if (ret < 0)
		return throw_sce_error(ctx, "sceCtrlSetActuator", ret);
	return JS_UNDEFINED;
}

static JSValue vitajs_battery_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc > 1)
		return JS_ThrowSyntaxError(ctx, "battery_info([port]) expects zero or one argument");
	int port = parse_port(ctx, argc, argv, 0, 1, 1);
	if (port < 0)
		return JS_EXCEPTION;
	uint8_t level = 0xFF;
	int ret = sceCtrlGetBatteryInfo(port, &level);
	if (ret < 0)
		return throw_sce_error(ctx, "sceCtrlGetBatteryInfo", ret);
	return JS_NewUint32(ctx, level);
}

static JSValue vitajs_set_sampling_mode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "setSamplingMode(mode) expects one argument");
	int32_t mode;
	if (JS_ToInt32(ctx, &mode, argv[0]))
		return JS_EXCEPTION;
	int ret = sceCtrlSetSamplingMode((SceCtrlPadInputMode)mode);
	if (ret < 0)
		return throw_sce_error(ctx, "sceCtrlSetSamplingMode", ret);
	return JS_NewInt32(ctx, ret);
}

static JSValue vitajs_get_sampling_mode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	(void)this_val;
	(void)argv;
	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "getSamplingMode() expects no arguments");
	SceCtrlPadInputMode mode;
	int ret = sceCtrlGetSamplingMode(&mode);
	if (ret < 0)
		return throw_sce_error(ctx, "sceCtrlGetSamplingMode", ret);
	return JS_NewInt32(ctx, (int)mode);
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("read", 1, vitajs_read),
	JS_CFUNC_DEF("analog", 1, vitajs_analog),
	JS_CFUNC_DEF("check", 2, vitajs_check),
	JS_CFUNC_DEF("rumble", 3, vitajs_rumble),
	JS_CFUNC_DEF("battery_info", 1, vitajs_battery_info),
	JS_CFUNC_DEF("setSamplingMode", 1, vitajs_set_sampling_mode),
	JS_CFUNC_DEF("getSamplingMode", 0, vitajs_get_sampling_mode),
	JS_PROP_INT32_DEF("SELECT", SCE_CTRL_SELECT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("START", SCE_CTRL_START, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("UP", SCE_CTRL_UP, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("RIGHT", SCE_CTRL_RIGHT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DOWN", SCE_CTRL_DOWN, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("LEFT", SCE_CTRL_LEFT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TRIANGLE", SCE_CTRL_TRIANGLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CIRCLE", SCE_CTRL_CIRCLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CROSS", SCE_CTRL_CROSS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SQUARE", SCE_CTRL_SQUARE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("L1", SCE_CTRL_L1, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("L2", SCE_CTRL_L2, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("L3", SCE_CTRL_L3, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("R1", SCE_CTRL_R1, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("R2", SCE_CTRL_R2, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("R3", SCE_CTRL_R3, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("POWER", SCE_CTRL_POWER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("MODE_DIGITAL", SCE_CTRL_MODE_DIGITAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("MODE_ANALOG", SCE_CTRL_MODE_ANALOG, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("MODE_ANALOG_WIDE", SCE_CTRL_MODE_ANALOG_WIDE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_UNPAIRED", SCE_CTRL_TYPE_UNPAIRED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_PSVITA", SCE_CTRL_TYPE_PHY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_PSTV", SCE_CTRL_TYPE_VIRT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_DUALSHOCK3", SCE_CTRL_TYPE_DS3, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_DUALSHOCK4", SCE_CTRL_TYPE_DS4, JS_PROP_CONFIGURABLE),
};
static int module_init(JSContext *ctx, JSModuleDef *m) { return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs)); }
JSModuleDef *vitajs_pads_init(JSContext *ctx) { return vitajs_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Pads"); }
