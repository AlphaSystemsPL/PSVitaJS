/**
 * VitaJS globals for VS Code / JavaScript IntelliSense.
 *
 * Put this file in the project root. It describes globals injected by VitaJS.
 * It does not change runtime behavior.
 */

type VitaTextureId = number;
type VitaTimer = object;
type VitaErrno = number;
type VitaRGBA = number;

interface VitaOS {
    /** Platform string exposed by bundled QuickJS. */
    readonly platform: string;

    /** Schedule a one-shot callback after `delayMs`. */
    setTimeout(callback: () => void, delayMs: number): VitaTimer;

    /** Schedule a repeating callback every `delayMs`. */
    setInterval(callback: () => void, delayMs: number): VitaTimer;

    /** Schedule a callback as soon as possible. */
    setImmediate(callback: () => void): VitaTimer;

    clearTimeout(timer: VitaTimer): void;
    clearInterval(timer: VitaTimer): void;
    clearImmediate(timer: VitaTimer): void;

    /** Block the current thread for `delayMs`. Returns an errno-style code. */
    sleep(delayMs: number): VitaErrno;

    /** Returns [currentDirectory, errno]. */
    getcwd(): [string, VitaErrno];
    chdir(path: string): VitaErrno;
    mkdir(path: string, mode?: number): VitaErrno;

    /** Returns [directoryEntries, errno]. */
    readdir(path: string): [string[], VitaErrno];

    /** Returns [absolutePath, errno]. */
    realpath(path: string): [string, VitaErrno];

    open(path: string, flags: number, mode?: number): number;
    close(fd: number): VitaErrno;
    seek(fd: number, offset: number | bigint, whence: number): number | bigint;
    read(fd: number, buffer: ArrayBuffer, offset: number, length: number): number;
    write(fd: number, buffer: ArrayBuffer, offset: number, length: number): number;
    remove(path: string): VitaErrno;
    rename(oldPath: string, newPath: string): VitaErrno;

    isatty(fd: number): boolean | number;
    ttyGetWinSize(fd: number): unknown;
    ttySetRaw(fd: number): unknown;
    setReadHandler(fd: number, callback: (() => void) | null): void;
    setWriteHandler(fd: number, callback: (() => void) | null): void;
    signal(signal: number, callback: (() => void) | null): void;

    /** @deprecated Currently stubbed in the bundled QuickJS source. */
    stat(path: string): undefined;
    /** @deprecated Currently stubbed in the bundled QuickJS source. */
    lstat(path: string): undefined;

    /** @experimental */
    symlink(target: string, linkPath: string): unknown;
    /** @experimental */
    readlink(path: string): unknown;
    /** @experimental */
    exec(options: unknown): unknown;
    /** @experimental */
    waitpid(pid: number, options: number): unknown;
    /** @experimental */
    pipe(): unknown;
    /** @experimental */
    kill(pid: number, signal: number): unknown;
    /** @experimental */
    dup(fd: number): number;
    /** @experimental */
    dup2(oldFd: number, newFd: number): number;

    readonly O_RDONLY: number;
    readonly O_WRONLY: number;
    readonly O_RDWR: number;
    readonly O_APPEND: number;
    readonly O_CREAT: number;
    readonly O_EXCL: number;
    readonly O_TRUNC: number;

    readonly S_IFMT: number;
    readonly S_IFIFO: number;
    readonly S_IFCHR: number;
    readonly S_IFDIR: number;
    readonly S_IFBLK: number;
    readonly S_IFREG: number;

    readonly SIGINT: number;
    readonly SIGABRT: number;
    readonly SIGFPE: number;
    readonly SIGILL: number;
    readonly SIGSEGV: number;
    readonly SIGTERM: number;
}

interface VitaScreen {
    /** Clear screen using RGBA components in the 0..255 range. */
    clear(r: number, g: number, b: number, a: number): void;
    start_drawing(): void;

    /** @deprecated Present upstream but not implemented. */
    start_drawing_advanced(target: VitaTextureId, flags: number): never;

    end_drawing(): void;
    common_dialog_update(): void;
    swap_buffers(): void;
    wait_rendering_done(): void;
    wait_vblank(): void;

    /** @deprecated Broken/incomplete upstream; use Font for text. */
    print(...args: unknown[]): void;

    /** Not tested upstream. */
    gmx_get_context(): void;

    set_region_clip(mode: number, xMin: number, yMin: number, xMax: number, yMax: number): void;
    disable_clipping(): void;

    /** Current wrapper discards the native return value. */
    get_clipping_enabled(): void;

    set_clip_rectangle(xMin: number, yMin: number, xMax: number, yMax: number): void;

    /** @deprecated Current upstream wrapper is incomplete. */
    get_clip_rectangle(xMin: number, yMin: number, xMax: number, yMax: number): void;

    set_blend_mode_add(enable: number | boolean): void;
    draw_pixel(x: number, y: number, color: VitaRGBA): void;
    draw_line(x0: number, y0: number, x1: number, y1: number, color: VitaRGBA): void;
    draw_rectangle(x: number, y: number, width: number, height: number, color: VitaRGBA): void;

    /** NOTE: VitaJS uses argument order radius, x, y, color. */
    draw_fill_circle(radius: number, x: number, y: number, color: VitaRGBA): void;

    /** @deprecated Incomplete/unsafe upstream. */
    draw_array(mode: number, x: number, y: number, z: number, color: VitaRGBA, count: number): void;
    /** @deprecated Incomplete/unsafe upstream. */
    draw_array2(mode: number, vertices: unknown, count: number): void;

    create_empty_texture(width: number, height: number): VitaTextureId;
    create_empty_texture_format(width: number, height: number, format: number): VitaTextureId;
    create_empty_texture_rendertarget(width: number, height: number, format: number): VitaTextureId;
    free_texture(textureId: VitaTextureId): boolean;

    texture_get_width(textureId: VitaTextureId): number;
    texture_get_height(textureId: VitaTextureId): number;
    texture_get_stride(textureId: VitaTextureId): number;
    texture_get_format(textureId: VitaTextureId): number;
    texture_get_datap(textureId: VitaTextureId, isShared: boolean): ArrayBuffer;
    texture_get_palette(textureId: VitaTextureId, isShared: boolean): ArrayBuffer;
    texture_get_min_filter(textureId: VitaTextureId): number;
    texture_get_mag_filter(textureId: VitaTextureId): number;
    texture_set_filters(textureId: VitaTextureId, minFilter: number, magFilter: number): void;

    draw_texture(textureId: VitaTextureId, x: number, y: number): void;
    draw_texture_rotate(textureId: VitaTextureId, x: number, y: number, radians: number): void;
    draw_texture_tint_rotate_hotspot(textureId: VitaTextureId, x: number, y: number, radians: number, centerX: number, centerY: number, color: VitaRGBA): void;
    draw_texture_tint_scale(textureId: VitaTextureId, x: number, y: number, scaleX: number, scaleY: number, color: VitaRGBA): void;
    draw_texture_tint_part(textureId: VitaTextureId, x: number, y: number, texX: number, texY: number, texWidth: number, texHeight: number, color: VitaRGBA): void;
    draw_texture_tint_part_scale(textureId: VitaTextureId, x: number, y: number, texX: number, texY: number, texWidth: number, texHeight: number, scaleX: number, scaleY: number, color: VitaRGBA): void;
    draw_texture_tint_scale_rotate_hotspot(textureId: VitaTextureId, x: number, y: number, scaleX: number, scaleY: number, radians: number, centerX: number, centerY: number, color: VitaRGBA): void;
    draw_texture_tint_scale_rotate(textureId: VitaTextureId, x: number, y: number, scaleX: number, scaleY: number, radians: number, color: VitaRGBA): void;
    draw_texture_part_tint_scale_rotate(textureId: VitaTextureId, x: number, y: number, texX: number, texY: number, texWidth: number, texHeight: number, scaleX: number, scaleY: number, radians: number, color: VitaRGBA): void;

    /** @experimental Incomplete upstream wrapper. */
    draw_array_textured(textureId: VitaTextureId, mode: number, color: VitaRGBA, vertices: ArrayBuffer): void;

    load_png_file(filename: string): VitaTextureId;
    load_jpg_file(filename: string): VitaTextureId;
    load_bmp_file(filename: string): VitaTextureId;
}

declare const os: VitaOS;
declare const Screen: VitaScreen;
