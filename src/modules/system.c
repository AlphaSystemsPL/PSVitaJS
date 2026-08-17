#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <psp2/sysmodule.h>
#include "../env.h"
#include "../system.h"
#include "../memory.h"
#define MAX_DIR_FILES 1024
#define SFO_PATH "app0:/sce_sys/param.sfo"
static char modulePath[256];
static char boot_path[255];

typedef struct __attribute__((packed))
{
	uint32_t magic, version, key_off, data_off, count;
} SfoHeader;
typedef struct __attribute__((packed))
{
	uint16_t key_off, fmt;
	uint32_t len, max_len, data_off;
} SfoEntry;

static uint8_t *read_entire_file(const char *path, size_t *out_size)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0)
	{
		fclose(f);
		return NULL;
	}
	long sz = ftell(f);
	if (sz <= 0 || fseek(f, 0, SEEK_SET) != 0)
	{
		fclose(f);
		return NULL;
	}
	uint8_t *b = malloc((size_t)sz);
	if (!b)
	{
		fclose(f);
		return NULL;
	}
	if (fread(b, 1, (size_t)sz, f) != (size_t)sz)
	{
		free(b);
		fclose(f);
		return NULL;
	}
	fclose(f);
	*out_size = (size_t)sz;
	return b;
}
static JSValue parse_manifest(JSContext *ctx)
{
	size_t size = 0;
	uint8_t *b = read_entire_file(SFO_PATH, &size);
	if (!b)
		return JS_ThrowInternalError(ctx, "Unable to read %s", SFO_PATH);
	if (size < sizeof(SfoHeader))
	{
		free(b);
		return JS_ThrowInternalError(ctx, "Invalid param.sfo");
	}
	SfoHeader *h = (SfoHeader *)b;
	if (h->magic != 0x46535000u || h->count > 1024)
	{
		free(b);
		return JS_ThrowInternalError(ctx, "Invalid param.sfo header");
	}
	size_t entries_end = sizeof(SfoHeader) + (size_t)h->count * sizeof(SfoEntry);
	if (entries_end > size || h->key_off >= size || h->data_off >= size)
	{
		free(b);
		return JS_ThrowInternalError(ctx, "Invalid param.sfo offsets");
	}
	JSValue o = JS_NewObject(ctx);
	SfoEntry *e = (SfoEntry *)(b + sizeof(SfoHeader));
	for (uint32_t n = 0; n < h->count; n++)
	{
		size_t kp = (size_t)h->key_off + e[n].key_off, dp = (size_t)h->data_off + e[n].data_off;
		if (kp >= size || dp >= size || e[n].len > size - dp)
			continue;
		const char *key = (const char *)(b + kp);
		size_t kmax = size - kp;
		if (!memchr(key, 0, kmax))
			continue;
		JSValue val;
		if (e[n].fmt == 0x0404 && e[n].len >= 4)
		{
			uint32_t x;
			memcpy(&x, b + dp, 4);
			val = JS_NewUint32(ctx, x);
		}
		else if (e[n].fmt == 0x0204)
		{
			size_t len = e[n].len;
			while (len && b[dp + len - 1] == 0)
				len--;
			val = JS_NewStringLen(ctx, (const char *)b + dp, len);
		}
		else
		{
			val = JS_NewArrayBufferCopy(ctx, b + dp, e[n].len);
		}
		JS_SetPropertyStr(ctx, o, key, val);
	}
	free(b);
	return o;
}
static JSValue manifest_get(JSContext *ctx, JSValueConst this_val)
{
	(void)this_val;
	return parse_manifest(ctx);
}
static JSValue manifest_field(JSContext *ctx, const char *key)
{
	JSValue o = parse_manifest(ctx);
	if (JS_IsException(o))
		return o;
	JSValue v = JS_GetPropertyStr(ctx, o, key);
	JS_FreeValue(ctx, o);
	return v;
}
#define FIELD_GETTER(name, key)                         \
	static JSValue name(JSContext *ctx, JSValueConst t) \
	{                                                   \
		(void)t;                                        \
		return manifest_field(ctx, key);                \
	}
FIELD_GETTER(get_title_id, "TITLE_ID")
FIELD_GETTER(get_title, "TITLE")
FIELD_GETTER(get_short_title, "STITLE") FIELD_GETTER(get_version, "APP_VER") FIELD_GETTER(get_category, "CATEGORY") FIELD_GETTER(get_content_id, "CONTENT_ID") static const char *language_name(int l)
{
	static const char *n[] = {"ja-JP", "en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "nl-NL", "pt-PT", "ru-RU", "ko-KR", "zh-TW", "zh-CN", "fi-FI", "sv-SE", "da-DK", "no-NO", "pl-PL", "pt-BR", "en-GB", "tr-TR"};
	return (l >= 0 && l < (int)(sizeof(n) / sizeof(n[0]))) ? n[l] : "unknown";
}
static int read_language(int *out)
{
	int r = sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, out);
	if (r >= 0)
		return r;
	if (r != SCE_APPUTIL_ERROR_NOT_INITIALIZED)
		return r;

	int loaded_here = 0;
	int initialized_here = 0;

	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_APPUTIL) < 0)
	{
		r = sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);
		if (r < 0)
			return r;
		loaded_here = 1;
	}

	SceAppUtilInitParam init_param;
	SceAppUtilBootParam boot_param;
	memset(&init_param, 0, sizeof(init_param));
	memset(&boot_param, 0, sizeof(boot_param));

	r = sceAppUtilInit(&init_param, &boot_param);
	if (r >= 0)
	{
		initialized_here = 1;
	}
	else if (r != SCE_APPUTIL_ERROR_BUSY)
	{
		if (loaded_here)
			sceSysmoduleUnloadModule(SCE_SYSMODULE_APPUTIL);
		return r;
	}

	r = sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, out);

	if (initialized_here)
	{
		sceAppUtilShutdown();
		if (loaded_here)
			sceSysmoduleUnloadModule(SCE_SYSMODULE_APPUTIL);
	}

	return r;
}
static JSValue get_language(JSContext *ctx, JSValueConst t)
{
	(void)t;
	int l = 0, r = read_language(&l);
	if (r < 0)
		return JS_ThrowInternalError(ctx, "Unable to read system language: 0x%08X", (unsigned)r);
	return JS_NewString(ctx, language_name(l));
}
static JSValue get_language_id(JSContext *ctx, JSValueConst t)
{
	(void)t;
	int l = 0, r = read_language(&l);
	if (r < 0)
		return JS_ThrowInternalError(ctx, "Unable to read system language: 0x%08X", (unsigned)r);
	return JS_NewInt32(ctx, l);
}
static JSValue get_model_id(JSContext *ctx, JSValueConst t)
{
	(void)t;
	return JS_NewInt32(ctx, sceKernelGetModel());
}
static JSValue get_model(JSContext *ctx, JSValueConst t)
{
	(void)t;
	int m = sceKernelGetModel();
	return JS_NewString(ctx, m == SCE_KERNEL_MODEL_VITATV ? "PS Vita TV" : "PS Vita");
}
static JSValue get_is_pstv(JSContext *ctx, JSValueConst t)
{
	(void)t;
	return JS_NewBool(ctx, sceKernelGetModel() == SCE_KERNEL_MODEL_VITATV);
}

static JSValue vitajs_getCurrentDirectory(JSContext *ctx)
{
	char path[256];
	getcwd(path, sizeof(path));
	return JS_NewString(ctx, path);
}
static JSValue vitajs_setCurrentDirectory(JSContext *ctx, JSValueConst *v)
{
	const char *p = JS_ToCString(ctx, v[0]);
	if (!p)
		return JS_EXCEPTION;
	int r = chdir(p);
	JS_FreeCString(ctx, p);
	if (r < 0)
		return JS_ThrowInternalError(ctx, "chdir failed");
	return JS_UNDEFINED;
}
static JSValue vitajs_curdir(JSContext *ctx, JSValueConst t, int argc, JSValueConst *argv)
{
	(void)t;
	if (argc == 0)
		return vitajs_getCurrentDirectory(ctx);
	if (argc == 1)
		return vitajs_setCurrentDirectory(ctx, argv);
	return JS_ThrowSyntaxError(ctx, "currentDir([path]) expects 0-1 arguments");
}
static JSValue vitajs_dir(JSContext *ctx, JSValueConst t, int argc, JSValueConst *argv)
{
	(void)t;
	if (argc > 1)
		return JS_ThrowSyntaxError(ctx, "listDir([path]) expects 0-1 arguments");
	char path[256];
	if (argc)
	{
		const char *p = JS_ToCString(ctx, argv[0]);
		if (!p)
			return JS_EXCEPTION;
		snprintf(path, sizeof(path), "%s", p);
		JS_FreeCString(ctx, p);
	}
	else
		getcwd(path, sizeof(path));
	DIR *d = opendir(path);
	JSValue a = JS_NewArray(ctx);
	if (!d)
		return a;
	struct dirent *e;
	uint32_t i = 0;
	while ((e = readdir(d)) && i < MAX_DIR_FILES)
	{
		JSValue o = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, o, "name", JS_NewString(ctx, e->d_name));
		JS_SetPropertyStr(ctx, o, "size", JS_NewUint32(ctx, e->d_stat.st_size));
		JS_SetPropertyStr(ctx, o, "dir", JS_NewBool(ctx, S_ISDIR(e->d_stat.st_mode)));
		JS_SetPropertyUint32(ctx, a, i++, o);
	}
	closedir(d);
	return a;
}
static JSValue one_path(JSContext *ctx, int argc, JSValueConst *argv, int op)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "one path expected");
	const char *p = JS_ToCString(ctx, argv[0]);
	if (!p)
		return JS_EXCEPTION;
	int r = 0;
	if (op == 0)
		r = mkdir(p, 0777);
	else if (op == 1)
		r = rmdir(p);
	else
		r = remove(p);
	JS_FreeCString(ctx, p);
	if (r < 0)
		return JS_ThrowInternalError(ctx, "filesystem operation failed");
	return JS_UNDEFINED;
}
static JSValue vitajs_createDir(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return one_path(c, a, v, 0);
}
static JSValue vitajs_removeDir(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return one_path(c, a, v, 1);
}
static JSValue vitajs_removeFile(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return one_path(c, a, v, 2);
}
static JSValue two_path(JSContext *ctx, int argc, JSValueConst *argv, int move)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "source and destination expected");
	const char *s = JS_ToCString(ctx, argv[0]), *d = JS_ToCString(ctx, argv[1]);
	if (!s || !d)
	{
		if (s)
			JS_FreeCString(ctx, s);
		if (d)
			JS_FreeCString(ctx, d);
		return JS_EXCEPTION;
	}
	int ret = 0;
	if (move == 2)
		ret = sceIoRename(s, d);
	else
	{
		int in = sceIoOpen(s, O_RDONLY, 0), out = -1;
		if (in < 0)
		{
			ret = in;
		}
		else
		{
			out = sceIoOpen(d, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (out < 0)
				ret = out;
			else
			{
				char b[16384];
				int n;
				while ((n = sceIoRead(in, b, sizeof(b))) > 0)
				{
					if (sceIoWrite(out, b, n) != n)
					{
						ret = -1;
						break;
					}
				}
				if (n < 0)
					ret = n;
			}
		}
		if (in >= 0)
			sceIoClose(in);
		if (out >= 0)
			sceIoClose(out);
		if (move == 1 && ret == 0)
			ret = sceIoRemove(s);
	}
	JS_FreeCString(ctx, s);
	JS_FreeCString(ctx, d);
	if (ret < 0)
		return JS_ThrowInternalError(ctx, "filesystem operation failed");
	return JS_UNDEFINED;
}
static JSValue vitajs_copyfile(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return two_path(c, a, v, 0);
}
static JSValue vitajs_movefile(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return two_path(c, a, v, 1);
}
static JSValue vitajs_rename(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return two_path(c, a, v, 2);
}
static JSValue vitajs_delay(JSContext *ctx, JSValueConst t, int argc, JSValueConst *argv)
{
	(void)t;
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "delay(milliseconds) expects one argument");
	int32_t ms;
	if (JS_ToInt32(ctx, &ms, argv[0]))
		return JS_EXCEPTION;
	delayMiliseconds(ms);
	return JS_UNDEFINED;
}
#define MEM_FN(name, fn)                                                        \
	static JSValue name(JSContext *ctx, JSValueConst t, int a, JSValueConst *v) \
	{                                                                           \
		(void)t;                                                                \
		(void)v;                                                                \
		if (a)                                                                  \
			return JS_ThrowSyntaxError(ctx, #fn " expects no arguments");       \
		return JS_NewUint32(ctx, (uint32_t)fn());                               \
	}
MEM_FN(vitajs_getFreeMemory, get_free_memory)
MEM_FN(vitajs_getUsedMemory, get_used_memory)
MEM_FN(vitajs_getFreeVRam, get_free_vram) MEM_FN(vitajs_getUsedVRam, get_used_vram) static JSValue vitajs_openfile(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(ctx, "openFile(path,type) expects two arguments");
	const char *p = JS_ToCString(ctx, v[0]);
	int32_t type;
	if (!p || JS_ToInt32(ctx, &type, v[1]))
	{
		if (p)
			JS_FreeCString(ctx, p);
		return JS_EXCEPTION;
	}
	int fd = open(p, type, 0777);
	JS_FreeCString(ctx, p);
	if (fd < 0)
		return JS_ThrowInternalError(ctx, "cannot open requested file");
	return JS_NewInt32(ctx, fd);
}
static JSValue vitajs_readfile(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(ctx, "readFile(fd,size) expects two arguments");
	int32_t fd;
	uint32_t size;
	if (JS_ToInt32(ctx, &fd, v[0]) || JS_ToUint32(ctx, &size, v[1]))
		return JS_EXCEPTION;
	char *b = malloc(size + 1);
	if (!b)
		return JS_ThrowOutOfMemory(ctx);
	int n = read(fd, b, size);
	if (n < 0)
	{
		free(b);
		return JS_ThrowInternalError(ctx, "read failed");
	}
	b[n] = 0;
	JSValue r = JS_NewStringLen(ctx, b, n);
	free(b);
	return r;
}
static JSValue vitajs_writefile(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 3)
		return JS_ThrowSyntaxError(ctx, "writeFile(fd,data,size) expects three arguments");
	int32_t fd;
	uint32_t size;
	size_t len;
	if (JS_ToInt32(ctx, &fd, v[0]) || JS_ToUint32(ctx, &size, v[2]))
		return JS_EXCEPTION;
	uint8_t *d = JS_GetArrayBuffer(ctx, &len, v[1]);
	if (!d)
		return JS_ThrowTypeError(ctx, "data must be ArrayBuffer");
	if (size > len)
		size = (uint32_t)len;
	if (write(fd, d, size) < 0)
		return JS_ThrowInternalError(ctx, "write failed");
	return JS_UNDEFINED;
}
static JSValue vitajs_closefile(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(ctx, "closeFile(fd) expects one argument");
	int32_t fd;
	if (JS_ToInt32(ctx, &fd, v[0]))
		return JS_EXCEPTION;
	if (close(fd) < 0)
		return JS_ThrowInternalError(ctx, "close failed");
	return JS_UNDEFINED;
}
static JSValue vitajs_seekfile(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 3)
		return JS_ThrowSyntaxError(ctx, "seekFile(fd,pos,type) expects three arguments");
	int32_t fd, pos, type;
	if (JS_ToInt32(ctx, &fd, v[0]) || JS_ToInt32(ctx, &pos, v[1]) || JS_ToInt32(ctx, &type, v[2]))
		return JS_EXCEPTION;
	if (lseek(fd, pos, type) < 0)
		return JS_ThrowInternalError(ctx, "seek failed");
	return JS_UNDEFINED;
}
static JSValue vitajs_sizefile(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(ctx, "sizeFile(fd) expects one argument");
	int32_t fd;
	if (JS_ToInt32(ctx, &fd, v[0]))
		return JS_EXCEPTION;
	off_t cur = lseek(fd, 0, SEEK_CUR), sz = lseek(fd, 0, SEEK_END);
	lseek(fd, cur, SEEK_SET);
	return JS_NewInt64(ctx, sz);
}
static JSValue vitajs_checkexist(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(ctx, "doesFileExist(path) expects one argument");
	const char *p = JS_ToCString(ctx, v[0]);
	if (!p)
		return JS_EXCEPTION;
	int fd = open(p, O_RDONLY, 0);
	JS_FreeCString(ctx, p);
	if (fd < 0)
		return JS_FALSE;
	close(fd);
	return JS_TRUE;
}
static JSValue vitajs_exit(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)ctx;
	(void)t;
	(void)a;
	(void)v;
	return JS_UNDEFINED;
}
static JSValue vitajs_loadELF(JSContext *ctx, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	(void)a;
	(void)v;
	return JS_ThrowInternalError(ctx, "LoadELFFromFile not implemented yet");
}
static void setModulePath(void)
{
	getcwd(modulePath, sizeof(modulePath));
	snprintf(boot_path, sizeof(boot_path), "%s", modulePath);
}
static const JSCFunctionListEntry system_funcs[] = {
	JS_CFUNC_DEF("openFile", 2, vitajs_openfile), JS_CFUNC_DEF("readFile", 2, vitajs_readfile), JS_CFUNC_DEF("writeFile", 3, vitajs_writefile), JS_CFUNC_DEF("closeFile", 1, vitajs_closefile), JS_CFUNC_DEF("seekFile", 3, vitajs_seekfile), JS_CFUNC_DEF("sizeFile", 1, vitajs_sizefile), JS_CFUNC_DEF("doesFileExist", 1, vitajs_checkexist), JS_CFUNC_DEF("currentDir", 1, vitajs_curdir), JS_CFUNC_DEF("listDir", 1, vitajs_dir), JS_CFUNC_DEF("createDirectory", 1, vitajs_createDir), JS_CFUNC_DEF("removeDirectory", 1, vitajs_removeDir), JS_CFUNC_DEF("moveFile", 2, vitajs_movefile), JS_CFUNC_DEF("copyFile", 2, vitajs_copyfile), JS_CFUNC_DEF("removeFile", 1, vitajs_removeFile), JS_CFUNC_DEF("rename", 2, vitajs_rename), JS_CFUNC_DEF("delay", 1, vitajs_delay), JS_CFUNC_DEF("get_free_memory", 0, vitajs_getFreeMemory), JS_CFUNC_DEF("get_used_memory", 0, vitajs_getUsedMemory), JS_CFUNC_DEF("get_free_vram", 0, vitajs_getFreeVRam), JS_CFUNC_DEF("get_used_vram", 0, vitajs_getUsedVRam), JS_CFUNC_DEF("exit", 0, vitajs_exit), JS_CFUNC_DEF("loadELF", 2, vitajs_loadELF),
	JS_PROP_STRING_DEF("boot_path", boot_path, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("FREAD", O_RDONLY, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("FWRITE", O_WRONLY, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("FCREATE", O_CREAT | O_WRONLY, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("FRDWR", O_RDWR, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("SET", SEEK_SET, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("END", SEEK_END, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("CUR", SEEK_CUR, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("READ_ONLY", 1, JS_PROP_CONFIGURABLE), JS_PROP_INT32_DEF("SELECT", 2, JS_PROP_CONFIGURABLE)};
static const char *system_info_exports[] = {
	"manifest",
	"titleId",
	"title",
	"shortTitle",
	"version",
	"category",
	"contentId",
	"language",
	"languageId",
	"model",
	"modelId",
	"isPSTV"};

static JSValue manifest_property_or_undefined(
	JSContext *ctx,
	JSValueConst manifest,
	const char *key)
{
	if (!JS_IsObject(manifest))
		return JS_UNDEFINED;

	return JS_GetPropertyStr(ctx, manifest, key);
}

static int set_system_export(
	JSContext *ctx,
	JSModuleDef *m,
	const char *name,
	JSValue value)
{
	/*
	 * JS_SetModuleExport takes ownership of value.
	 * Never pass JS_EXCEPTION into the module namespace.
	 */
	if (JS_IsException(value))
	{
		JSValue exception = JS_GetException(ctx);
		JS_FreeValue(ctx, exception);
		value = JS_UNDEFINED;
	}

	return JS_SetModuleExport(ctx, m, name, value);
}

static int system_init(JSContext *ctx, JSModuleDef *m)
{
	int ret = JS_SetModuleExportList(
		ctx,
		m,
		system_funcs,
		countof(system_funcs));

	if (ret < 0)
		return ret;

	/*
	 * QuickJS JS_SetModuleExportList() only supports C functions and
	 * simple constant/object entries. JS_CGETSET_DEF is NOT supported
	 * for C module exports and causes QuickJS to abort().
	 *
	 * System metadata is effectively immutable for the lifetime of the
	 * process, so expose it as ordinary module values initialized once.
	 */
	JSValue manifest = parse_manifest(ctx);

	if (JS_IsException(manifest))
	{
		JSValue exception = JS_GetException(ctx);
		JS_FreeValue(ctx, exception);
		manifest = JS_NewObject(ctx);
	}

	JSValue title_id = manifest_property_or_undefined(ctx, manifest, "TITLE_ID");
	JSValue title = manifest_property_or_undefined(ctx, manifest, "TITLE");
	JSValue short_title = manifest_property_or_undefined(ctx, manifest, "STITLE");
	JSValue version = manifest_property_or_undefined(ctx, manifest, "APP_VER");
	JSValue category = manifest_property_or_undefined(ctx, manifest, "CATEGORY");
	JSValue content_id = manifest_property_or_undefined(ctx, manifest, "CONTENT_ID");

	int language_id = -1;
	const char *language = "unknown";

	if (read_language(&language_id) >= 0)
		language = language_name(language_id);

	int model_id = sceKernelGetModel();
	int is_pstv = (model_id == SCE_KERNEL_MODEL_VITATV);

	if (set_system_export(ctx, m, "manifest", manifest) < 0 ||
		set_system_export(ctx, m, "titleId", title_id) < 0 ||
		set_system_export(ctx, m, "title", title) < 0 ||
		set_system_export(ctx, m, "shortTitle", short_title) < 0 ||
		set_system_export(ctx, m, "version", version) < 0 ||
		set_system_export(ctx, m, "category", category) < 0 ||
		set_system_export(ctx, m, "contentId", content_id) < 0 ||
		set_system_export(ctx, m, "language", JS_NewString(ctx, language)) < 0 ||
		set_system_export(ctx, m, "languageId", JS_NewInt32(ctx, language_id)) < 0 ||
		set_system_export(ctx, m, "model",
						  JS_NewString(ctx, is_pstv ? "PS Vita TV" : "PS Vita")) < 0 ||
		set_system_export(ctx, m, "modelId", JS_NewInt32(ctx, model_id)) < 0 ||
		set_system_export(ctx, m, "isPSTV", JS_NewBool(ctx, is_pstv)) < 0)
		return -1;

	return 0;
}

JSModuleDef *vitajs_system_init(JSContext *ctx)
{
	setModulePath();

	JSModuleDef *m = vitajs_push_module(
		ctx,
		system_init,
		system_funcs,
		countof(system_funcs),
		"System");

	if (!m)
		return NULL;

	for (size_t i = 0;
		 i < sizeof(system_info_exports) / sizeof(system_info_exports[0]);
		 i++)
	{
		if (JS_AddModuleExport(ctx, m, system_info_exports[i]) < 0)
			return NULL;
	}

	return m;
}
