/* ==========================================================================
 * Shared types
 * ========================================================================== */

type VitaTextureId = number;
type VitaRGBA = number;

type VitaAudioClipId = number;
type VitaAudioVoiceId = number;

type VitaFileDescriptor = number;
type VitaTimerHandle = unknown;

interface VitaTextDimensions {
    width: number;
    height: number;
}

/** Opaque native font object returned by Font.load_font_file(). */
interface VitaFont {
    readonly __vitaFontBrand?: never;
}

interface VitaNetResponse {
    /** HTTP status code, e.g. 200, 404, 500. */
    status: number;

    /** true when status is in the 200-299 range. */
    ok: boolean;

    /** Full response body as text. */
    body: string;
}

interface VitaPadState {
    /** Raw button bitmask. */
    btns: number;

    /** Left analog X. */
    lx: number;

    /** Left analog Y. */
    ly: number;

    /** Right analog X. */
    rx: number;

    /** Right analog Y. */
    ry: number;
}

/* ==========================================================================
 * System
 * ========================================================================== */

interface VitaSystemAPI {
    openFile(path: string, type: number): VitaFileDescriptor;

    readFile(fd: VitaFileDescriptor, size: number): string;

    writeFile(
        fd: VitaFileDescriptor,
        data: ArrayBuffer,
        size: number
    ): void;

    closeFile(fd: VitaFileDescriptor): void;

    seekFile(
        fd: VitaFileDescriptor,
        position: number,
        type: number
    ): void;

    sizeFile(fd: VitaFileDescriptor): number;

    doesFileExist(path: string): boolean;

    /** Get current directory. */
    currentDir(): string;

    /** Set current directory. */
    currentDir(path: string): void;

    listDir(path?: string): unknown[];

    createDirectory(path: string): void;
    removeDirectory(path: string): void;

    moveFile(source: string, destination: string): void;
    copyFile(source: string, destination: string): void;
    removeFile(path: string): void;
    rename(source: string, destination: string): void;

    /** Blocking delay in milliseconds. */
    delay(milliseconds: number): void;

    get_free_memory(): number;
    get_used_memory(): number;
    get_free_vram(): number;
    get_used_vram(): number;

    /**
     * Present in upstream VitaJS but may be incomplete.
     * @deprecated
     */
    exit(): void;

    /**
     * Upstream implementation is incomplete.
     * @deprecated
     */
    loadELF(path: string, args?: string[]): never;

    readonly boot_path: string;

    readonly FREAD: number;
    readonly FWRITE: number;
    readonly FCREATE: number;
    readonly FRDWR: number;

    readonly SET: number;
    readonly END: number;
    readonly CUR: number;

    readonly READ_ONLY: number;
    readonly SELECT: number;
}

/* ==========================================================================
 * Pads
 * ========================================================================== */

interface VitaPadsAPI {
    /** Read current buttons and analog state. */
    analog(port?: number): VitaPadState;

    /** Check whether a button mask is pressed. */
    check(button: number): boolean;

    /**
     * Upstream implementation may be incomplete.
     * @deprecated
     */
    rumble(small: number, large: number, port?: number): void;

    /**
     * Upstream implementation may be incomplete.
     * @deprecated
     */
    battery_info(port?: number): number;

    readonly SELECT: number;
    readonly START: number;

    readonly UP: number;
    readonly RIGHT: number;
    readonly DOWN: number;
    readonly LEFT: number;

    readonly TRIANGLE: number;
    readonly CIRCLE: number;
    readonly CROSS: number;
    readonly SQUARE: number;

    readonly L1: number;
    readonly L2: number;
    readonly L3: number;
    readonly R1: number;
    readonly R2: number;
    readonly R3: number;

    readonly POWER: number;

    readonly MODE_DIGITAL: number;
    readonly MODE_ANALOG: number;
    readonly MODE_ANALOG_WIDE: number;

    readonly TYPE_UNPAIRED: number;
    readonly TYPE_PSVITA: number;
    readonly TYPE_PSTV: number;
    readonly TYPE_DUALSHOCK3: number;
    readonly TYPE_DUALSHOCK4: number;
}

/* ==========================================================================
 * Screen
 * ========================================================================== */

interface VitaScreenAPI {
    /** Clear screen using RGBA components, usually 0..255. */
    clear(r: number, g: number, b: number, a: number): void;

    start_drawing(): void;

    /**
     * Incomplete upstream implementation.
     * @deprecated
     */
    start_drawing_advanced(
        target: VitaTextureId,
        flags: number
    ): void;

    end_drawing(): void;
    common_dialog_update(): void;
    swap_buffers(): void;
    wait_rendering_done(): void;
    wait_vblank(): void;

    /**
     * Broken/incomplete upstream. Use Font.font_draw_text().
     * @deprecated
     */
    print(...args: unknown[]): void;

    gmx_get_context(): void;

    set_region_clip(
        mode: number,
        xMin: number,
        yMin: number,
        xMax: number,
        yMax: number
    ): void;

    disable_clipping(): void;

    get_clipping_enabled(): void;

    set_clip_rectangle(
        xMin: number,
        yMin: number,
        xMax: number,
        yMax: number
    ): void;

    /**
     * Incomplete upstream implementation.
     * @deprecated
     */
    get_clip_rectangle(
        xMin: number,
        yMin: number,
        xMax: number,
        yMax: number
    ): void;

    set_blend_mode_add(enable: boolean | number): void;

    draw_pixel(
        x: number,
        y: number,
        color: VitaRGBA
    ): void;

    draw_line(
        x0: number,
        y0: number,
        x1: number,
        y1: number,
        color: VitaRGBA
    ): void;

    draw_rectangle(
        x: number,
        y: number,
        width: number,
        height: number,
        color: VitaRGBA
    ): void;

    /**
     * VitaJS currently uses argument order:
     * radius, x, y, color.
     */
    draw_fill_circle(
        radius: number,
        x: number,
        y: number,
        color: VitaRGBA
    ): void;

    /**
     * Incomplete/unsafe upstream.
     * @deprecated
     */
    draw_array(
        mode: number,
        x: number,
        y: number,
        z: number,
        color: VitaRGBA,
        count: number
    ): void;

    /**
     * Incomplete/unsafe upstream.
     * @deprecated
     */
    draw_array2(
        mode: number,
        vertices: unknown,
        count: number
    ): void;

    create_empty_texture(
        width: number,
        height: number
    ): VitaTextureId;

    create_empty_texture_format(
        width: number,
        height: number,
        format: number
    ): VitaTextureId;

    create_empty_texture_rendertarget(
        width: number,
        height: number,
        format: number
    ): VitaTextureId;

    free_texture(textureId: VitaTextureId): boolean;

    texture_get_width(textureId: VitaTextureId): number;
    texture_get_height(textureId: VitaTextureId): number;
    texture_get_stride(textureId: VitaTextureId): number;
    texture_get_format(textureId: VitaTextureId): number;

    texture_get_datap(
        textureId: VitaTextureId,
        isShared: boolean
    ): ArrayBuffer;

    texture_get_palette(
        textureId: VitaTextureId,
        isShared: boolean
    ): ArrayBuffer;

    texture_get_min_filter(textureId: VitaTextureId): number;
    texture_get_mag_filter(textureId: VitaTextureId): number;

    texture_set_filters(
        textureId: VitaTextureId,
        minFilter: number,
        magFilter: number
    ): void;

    draw_texture(
        textureId: VitaTextureId,
        x: number,
        y: number
    ): void;

    draw_texture_rotate(
        textureId: VitaTextureId,
        x: number,
        y: number,
        radians: number
    ): void;

    draw_texture_tint_rotate_hotspot(
        textureId: VitaTextureId,
        x: number,
        y: number,
        radians: number,
        centerX: number,
        centerY: number,
        color: VitaRGBA
    ): void;

    draw_texture_tint_scale(
        textureId: VitaTextureId,
        x: number,
        y: number,
        scaleX: number,
        scaleY: number,
        color: VitaRGBA
    ): void;

    draw_texture_tint_part(
        textureId: VitaTextureId,
        x: number,
        y: number,
        texX: number,
        texY: number,
        texWidth: number,
        texHeight: number,
        color: VitaRGBA
    ): void;

    draw_texture_tint_part_scale(
        textureId: VitaTextureId,
        x: number,
        y: number,
        texX: number,
        texY: number,
        texWidth: number,
        texHeight: number,
        scaleX: number,
        scaleY: number,
        color: VitaRGBA
    ): void;

    draw_texture_tint_scale_rotate_hotspot(
        textureId: VitaTextureId,
        x: number,
        y: number,
        scaleX: number,
        scaleY: number,
        radians: number,
        centerX: number,
        centerY: number,
        color: VitaRGBA
    ): void;

    draw_texture_tint_scale_rotate(
        textureId: VitaTextureId,
        x: number,
        y: number,
        scaleX: number,
        scaleY: number,
        radians: number,
        color: VitaRGBA
    ): void;

    draw_texture_part_tint_scale_rotate(
        textureId: VitaTextureId,
        x: number,
        y: number,
        texX: number,
        texY: number,
        texWidth: number,
        texHeight: number,
        scaleX: number,
        scaleY: number,
        radians: number,
        color: VitaRGBA
    ): void;

    /**
     * Experimental upstream API.
     * @experimental
     */
    draw_array_textured(
        textureId: VitaTextureId,
        mode: number,
        color: VitaRGBA,
        vertices: ArrayBuffer
    ): void;

    load_png_file(filename: string): VitaTextureId;
    load_jpg_file(filename: string): VitaTextureId;
    load_bmp_file(filename: string): VitaTextureId;
}

/* ==========================================================================
 * Font - patched module
 * ========================================================================== */

interface VitaFontAPI {
    /**
     * Load a TTF/OTF font from Vita filesystem.
     *
     * @example
     * const font = Font.load_font_file("app0:/assets/font.ttf");
     */
    load_font_file(path: string): VitaFont;

    /** Explicitly release a font. */
    free_font(font: VitaFont): void;

    /**
     * Draw text.
     * `y` acts as the text baseline.
     */
    font_draw_text(
        font: VitaFont,
        x: number,
        y: number,
        color: VitaRGBA,
        size: number,
        text: string
    ): void;

    font_draw_text_ls(
        font: VitaFont,
        x: number,
        y: number,
        lineSpace: number,
        color: VitaRGBA,
        size: number,
        text: string
    ): void;

    font_text_dimensions(
        font: VitaFont,
        size: number,
        text: string
    ): VitaTextDimensions;

    font_text_width(
        font: VitaFont,
        size: number,
        text: string
    ): number;

    font_text_height(
        font: VitaFont,
        size: number,
        text: string
    ): number;
}

/* ==========================================================================
 * Audio - custom module
 * ========================================================================== */

interface VitaAudioAPI {
    /**
     * Load RIFF/WAVE PCM 16-bit mono or stereo.
     * Audio is resampled internally to 48 kHz stereo.
     *
     * @returns clip handle
     */
    load_wav(path: string): VitaAudioClipId;

    /**
     * Free a loaded audio clip.
     * Any playing voices using it are stopped.
     */
    free(clipId: VitaAudioClipId): void;

    /**
     * Play a loaded audio clip.
     *
     * @param clipId clip returned by Audio.load_wav()
     * @param volume volume 0.0..1.0, default 1.0
     * @param loop whether to loop continuously
     *
     * @returns voice handle, or -1 when all voices are occupied
     */
    play(
        clipId: VitaAudioClipId,
        volume?: number,
        loop?: boolean
    ): VitaAudioVoiceId;

    stop(voiceId: VitaAudioVoiceId): void;

    stop_all(): void;

    is_playing(
        voiceId: VitaAudioVoiceId
    ): boolean;

    set_voice_volume(
        voiceId: VitaAudioVoiceId,
        volume: number
    ): void;

    set_master_volume(
        volume: number
    ): void;

    get_master_volume(): number;

    /** Shut down audio thread and free all clips. */
    term(): void;
}

/* ==========================================================================
 * Net - synchronous module
 * ========================================================================== */

interface VitaNetResponse {
    /** HTTP status code, e.g. 200, 404, 500. */
    status: number;

    /** true when status is in the 200-299 range. */
    ok: boolean;

    /** Full response body as text. */
    body: string;
}

type VitaHttpMethod =
    | "GET"
    | "POST"
    | "PUT"
    | "DELETE"
    | "HEAD"
    | "OPTIONS";

interface VitaNetAPI {
    /**
     * Perform a synchronous HTTP request.
     *
     * NOTE: this blocks the JS/game loop until the request completes.
     *
     * @param method HTTP method
     * @param url Request URL
     * @param body Optional request body
     * @param contentType Optional Content-Type header
     */
    request(
        method: VitaHttpMethod,
        url: string,
        body?: string | null,
        contentType?: string | null
    ): VitaNetResponse;

    /**
     * Perform a synchronous HTTP GET request.
     *
     * NOTE: this blocks the JS/game loop until the request completes.
     */
    get(url: string): VitaNetResponse;

    /**
     * Perform a synchronous HTTP POST request.
     *
     * Content-Type defaults to application/json.
     *
     * NOTE: this blocks the JS/game loop until the request completes.
     */
    post(
        url: string,
        body: string,
        contentType?: string
    ): VitaNetResponse;

    /** Check whether Vita currently has an active network connection. */
    is_connected(): boolean;

    /** Return current Vita IPv4 address. */
    get_ip(): string;

    /** Shut down SceHttp/SceNetCtl/SceNet resources. */
    term(): void;
}

/* ==========================================================================
 * QuickJS os
 * ========================================================================== */

interface VitaOSAPI {
    readonly platform: string;

    setTimeout(
        callback: () => void,
        delayMs: number
    ): VitaTimerHandle;

    setInterval(
        callback: () => void,
        delayMs: number
    ): VitaTimerHandle;

    setImmediate(
        callback: () => void
    ): VitaTimerHandle;

    clearTimeout(timer: VitaTimerHandle): void;
    clearInterval(timer: VitaTimerHandle): void;
    clearImmediate(timer: VitaTimerHandle): void;

    /** Blocking sleep. */
    sleep(delayMs: number): number;

    getcwd(): [string, number];
    chdir(path: string): number;
    mkdir(path: string, mode?: number): number;

    readdir(path: string): [string[], number];

    realpath(path: string): [string, number];

    open(
        path: string,
        flags: number,
        mode?: number
    ): VitaFileDescriptor;

    close(
        fd: VitaFileDescriptor
    ): number;

    seek(
        fd: VitaFileDescriptor,
        offset: number | bigint,
        whence: number
    ): number | bigint;

    read(
        fd: VitaFileDescriptor,
        buffer: ArrayBuffer,
        offset: number,
        length: number
    ): number;

    write(
        fd: VitaFileDescriptor,
        buffer: ArrayBuffer,
        offset: number,
        length: number
    ): number;

    remove(path: string): number;
    rename(oldPath: string, newPath: string): number;

    isatty(fd: VitaFileDescriptor): boolean | number;

    setReadHandler(
        fd: VitaFileDescriptor,
        callback: (() => void) | null
    ): void;

    setWriteHandler(
        fd: VitaFileDescriptor,
        callback: (() => void) | null
    ): void;

    signal(
        signal: number,
        callback: (() => void) | null
    ): void;

    symlink(
        target: string,
        linkPath: string
    ): unknown;

    readlink(
        path: string
    ): unknown;

    exec(
        options: unknown
    ): unknown;

    waitpid(
        pid: number,
        options: number
    ): unknown;

    pipe(): unknown;

    kill(
        pid: number,
        signal: number
    ): unknown;

    dup(
        fd: number
    ): number;

    dup2(
        oldFd: number,
        newFd: number
    ): number;

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

/* ==========================================================================
 * QuickJS std
 * ========================================================================== */

interface VitaStdFile {
    close(): void;
    puts(value: string): void;
    printf(format: string, ...args: unknown[]): void;
    flush(): void;

    tell(): number;
    tello(): number | bigint;

    seek(
        offset: number | bigint,
        whence: number
    ): number;

    eof(): boolean;
    fileno(): number;

    error(): number;
    clearerr(): void;

    read(
        buffer: ArrayBuffer,
        offset: number,
        length: number
    ): number;

    write(
        buffer: ArrayBuffer,
        offset: number,
        length: number
    ): number;

    getline(): string | null;

    readAsString(
        maxSize?: number
    ): string;

    getByte(): number;
    putByte(value: number): number;
}

interface VitaStdAPI {
    exit(status?: number): never;

    gc(): void;

    evalScript(
        source: string
    ): unknown;

    loadScript(
        path: string
    ): unknown;

    loadFile(
        path: string
    ): string | null;

    getenv(
        name: string
    ): string | undefined;

    setenv(
        name: string,
        value?: string
    ): void;

    unsetenv(
        name: string
    ): void;

    getenviron(): Record<string, string>;

    strerror(errno: number): string;

    parseExtJSON(
        source: string
    ): unknown;

    open(
        path: string,
        mode: string
    ): VitaStdFile | null;

    popen(
        command: string,
        mode: string
    ): VitaStdFile | null;

    fdopen(
        fd: number,
        mode: string
    ): VitaStdFile | null;

    tmpfile(): VitaStdFile | null;

    puts(
        value: string
    ): void;

    printf(
        format: string,
        ...args: unknown[]
    ): void;

    sprintf(
        format: string,
        ...args: unknown[]
    ): string;

    readonly SEEK_SET: number;
    readonly SEEK_CUR: number;
    readonly SEEK_END: number;

    readonly in: VitaStdFile;
    readonly out: VitaStdFile;
    readonly err: VitaStdFile;
}

/* ==========================================================================
 * Console
 * ========================================================================== */

interface VitaConsoleAPI {
    log(...values: unknown[]): void;
}

/* ==========================================================================
 * Globals injected by VitaJS
 * ========================================================================== */

declare const System: VitaSystemAPI;
declare const Pads: VitaPadsAPI;
declare const Screen: VitaScreenAPI;
declare const Font: VitaFontAPI;
declare const Audio: VitaAudioAPI;
declare const Net: VitaNetAPI;

declare const os: VitaOSAPI;
declare const std: VitaStdAPI;
declare const console: VitaConsoleAPI;
