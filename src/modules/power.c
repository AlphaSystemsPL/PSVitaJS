#include <stdint.h>

#include <psp2/power.h>

#include "../env.h"

static JSValue throw_sce_error(JSContext *ctx, const char *name, int error)
{
    return JS_ThrowInternalError(
        ctx,
        "%s failed: 0x%08X",
        name,
        (unsigned int)error);
}

static int set_property(JSContext *ctx, JSValue object, const char *name, JSValue value)
{
    if (JS_SetPropertyStr(ctx, object, name, value) < 0)
        return -1;

    return 0;
}

static JSValue vitajs_power_get_battery_info(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "getBatteryInfo() expects no arguments");

    JSValue object = JS_NewObject(ctx);
    if (JS_IsException(object))
        return object;

    int percent = scePowerGetBatteryLifePercent();
    int remaining_minutes = scePowerGetBatteryLifeTime();
    int remaining_capacity = scePowerGetBatteryRemainCapacity();
    int full_capacity = scePowerGetBatteryFullCapacity();
    int temperature = scePowerGetBatteryTemp();
    int voltage = scePowerGetBatteryVolt();
    int health = scePowerGetBatterySOH();
    int cycles = scePowerGetBatteryCycleCount();

    if (set_property(ctx, object, "percent", JS_NewInt32(ctx, percent)) < 0 ||
        set_property(ctx, object, "charging", JS_NewBool(ctx, scePowerIsBatteryCharging())) < 0 ||
        set_property(ctx, object, "pluggedIn", JS_NewBool(ctx, scePowerIsPowerOnline())) < 0 ||
        set_property(ctx, object, "low", JS_NewBool(ctx, scePowerIsLowBattery())) < 0 ||
        set_property(ctx, object, "remainingMinutes", JS_NewInt32(ctx, remaining_minutes)) < 0 ||
        set_property(ctx, object, "remainingCapacity", JS_NewInt32(ctx, remaining_capacity)) < 0 ||
        set_property(ctx, object, "fullCapacity", JS_NewInt32(ctx, full_capacity)) < 0 ||
        set_property(ctx, object, "temperature", JS_NewFloat64(ctx, (double)temperature / 100.0)) < 0 ||
        set_property(ctx, object, "voltage", JS_NewInt32(ctx, voltage)) < 0 ||
        set_property(ctx, object, "health", JS_NewInt32(ctx, health)) < 0 ||
        set_property(ctx, object, "cycles", JS_NewInt32(ctx, cycles)) < 0)
    {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }

    return object;
}

static JSValue vitajs_power_get_clocks(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "getClocks() expects no arguments");

    JSValue object = JS_NewObject(ctx);
    if (JS_IsException(object))
        return object;

    if (set_property(ctx, object, "cpu", JS_NewInt32(ctx, scePowerGetArmClockFrequency())) < 0 ||
        set_property(ctx, object, "bus", JS_NewInt32(ctx, scePowerGetBusClockFrequency())) < 0 ||
        set_property(ctx, object, "gpu", JS_NewInt32(ctx, scePowerGetGpuClockFrequency())) < 0 ||
        set_property(ctx, object, "gpuXbar", JS_NewInt32(ctx, scePowerGetGpuXbarClockFrequency())) < 0)
    {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }

    return object;
}

static JSValue vitajs_power_get_battery_percent(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "getBatteryPercent() expects no arguments");

    return JS_NewInt32(ctx, scePowerGetBatteryLifePercent());
}

static JSValue vitajs_power_is_charging(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "isCharging() expects no arguments");

    return JS_NewBool(ctx, scePowerIsBatteryCharging());
}

static JSValue vitajs_power_is_plugged_in(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "isPluggedIn() expects no arguments");

    return JS_NewBool(ctx, scePowerIsPowerOnline());
}

static JSValue vitajs_power_is_low_battery(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "isLowBattery() expects no arguments");

    return JS_NewBool(ctx, scePowerIsLowBattery());
}

static JSValue vitajs_power_is_suspend_required(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "isSuspendRequired() expects no arguments");

    return JS_NewBool(ctx, scePowerIsSuspendRequired());
}

static int get_optional_clock(
    JSContext *ctx,
    JSValue object,
    const char *name,
    int *present,
    int32_t *frequency)
{
    JSValue value = JS_GetPropertyStr(ctx, object, name);

    if (JS_IsUndefined(value) || JS_IsNull(value))
    {
        JS_FreeValue(ctx, value);
        *present = 0;
        return 0;
    }

    if (JS_ToInt32(ctx, frequency, value))
    {
        JS_FreeValue(ctx, value);
        return -1;
    }

    JS_FreeValue(ctx, value);
    *present = 1;
    return 0;
}

static JSValue vitajs_power_set_clocks(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1 || !JS_IsObject(argv[0]))
        return JS_ThrowSyntaxError(ctx, "setClocks(options) expects one object argument");

    int has_cpu = 0;
    int has_bus = 0;
    int has_gpu = 0;
    int has_gpu_xbar = 0;

    int32_t cpu = 0;
    int32_t bus = 0;
    int32_t gpu = 0;
    int32_t gpu_xbar = 0;

    if (get_optional_clock(ctx, argv[0], "cpu", &has_cpu, &cpu) < 0 ||
        get_optional_clock(ctx, argv[0], "bus", &has_bus, &bus) < 0 ||
        get_optional_clock(ctx, argv[0], "gpu", &has_gpu, &gpu) < 0 ||
        get_optional_clock(ctx, argv[0], "gpuXbar", &has_gpu_xbar, &gpu_xbar) < 0)
    {
        return JS_EXCEPTION;
    }

    int result;

    if (has_cpu)
    {
        result = scePowerSetArmClockFrequency(cpu);
        if (result < 0)
            return throw_sce_error(ctx, "scePowerSetArmClockFrequency", result);
    }

    if (has_bus)
    {
        result = scePowerSetBusClockFrequency(bus);
        if (result < 0)
            return throw_sce_error(ctx, "scePowerSetBusClockFrequency", result);
    }

    if (has_gpu)
    {
        result = scePowerSetGpuClockFrequency(gpu);
        if (result < 0)
            return throw_sce_error(ctx, "scePowerSetGpuClockFrequency", result);
    }

    if (has_gpu_xbar)
    {
        result = scePowerSetGpuXbarClockFrequency(gpu_xbar);
        if (result < 0)
            return throw_sce_error(ctx, "scePowerSetGpuXbarClockFrequency", result);
    }

    return vitajs_power_get_clocks(ctx, JS_UNDEFINED, 0, NULL);
}

static JSValue request_power_action(
    JSContext *ctx,
    const char *name,
    int (*action)(void))
{
    int result = action();
    if (result < 0)
        return throw_sce_error(ctx, name, result);

    return JS_UNDEFINED;
}

static JSValue vitajs_power_suspend(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "suspend() expects no arguments");
    return request_power_action(ctx, "scePowerRequestSuspend", scePowerRequestSuspend);
}

static JSValue vitajs_power_standby(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "standby() expects no arguments");
    return request_power_action(ctx, "scePowerRequestStandby", scePowerRequestStandby);
}

static JSValue vitajs_power_display_on(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "requestDisplayOn() expects no arguments");
    return request_power_action(ctx, "scePowerRequestDisplayOn", scePowerRequestDisplayOn);
}

static JSValue vitajs_power_display_off(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;
    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "requestDisplayOff() expects no arguments");
    return request_power_action(ctx, "scePowerRequestDisplayOff", scePowerRequestDisplayOff);
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("getBatteryInfo", 0, vitajs_power_get_battery_info),
    JS_CFUNC_DEF("getClocks", 0, vitajs_power_get_clocks),
    JS_CFUNC_DEF("getBatteryPercent", 0, vitajs_power_get_battery_percent),
    JS_CFUNC_DEF("isCharging", 0, vitajs_power_is_charging),
    JS_CFUNC_DEF("isPluggedIn", 0, vitajs_power_is_plugged_in),
    JS_CFUNC_DEF("isLowBattery", 0, vitajs_power_is_low_battery),
    JS_CFUNC_DEF("isSuspendRequired", 0, vitajs_power_is_suspend_required),
    JS_CFUNC_DEF("setClocks", 1, vitajs_power_set_clocks),
    JS_CFUNC_DEF("suspend", 0, vitajs_power_suspend),
    JS_CFUNC_DEF("standby", 0, vitajs_power_standby),
    JS_CFUNC_DEF("requestDisplayOn", 0, vitajs_power_display_on),
    JS_CFUNC_DEF("requestDisplayOff", 0, vitajs_power_display_off),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(
        ctx,
        m,
        module_funcs,
        countof(module_funcs));
}

JSModuleDef *vitajs_power_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        module_init,
        module_funcs,
        countof(module_funcs),
        "Power");
}
