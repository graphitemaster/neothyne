#include <stdarg.h>

#include <SDL2/SDL.h>

#include "engine.h"
#include "cvar.h"

#include "r_common.h"

#include "u_file.h"
#include "u_misc.h"

// Engine globals
static u::map<u::string, int> gKeyMap;
static u::string gUserPath;
static u::string gGamePath;
static size_t gScreenWidth = 0;
static size_t gScreenHeight = 0;
static size_t gRefreshRate = 0;
static SDL_Window *gScreen = nullptr;
static frameTimer gTimer;
static u::map<u::string, void (*)()> gBinds;
static u::string gTextInput;
static mouseState gMouseState;

// maximum resolution is 15360x8640 (8640p) (16:9)
// default resolution is 1024x768 (XGA) (4:3)
static constexpr int kDefaultScreenWidth = 1024;
static constexpr int kDefaultScreenHeight = 768;
static constexpr size_t kRefreshRate = 60;

VAR(int, vid_vsync, "vertical syncronization", -1, kSyncRefresh, kSyncNone);
VAR(int, vid_fullscreen, "toggle fullscreen", 0, 1, 1);
VAR(int, vid_width, "resolution width", 0, 15360, 0);
VAR(int, vid_height, "resolution height", 0, 8640, 0);
VAR(int, vid_maxfps, "cap framerate", 0, 3600, 0);
VAR(u::string, vid_driver, "video driver");

static SDL_Window *getContext() {
    SDL_Init(SDL_INIT_VIDEO);

    const u::string &videoDriver = vid_driver.get();
    if (videoDriver.size()) {
        if (SDL_GL_LoadLibrary(videoDriver.c_str()) != 0) {
            u::print("Failed to load video driver: %s\n", SDL_GetError());
            return nullptr;
        } else {
            u::print("Loaded video driver: %s\n", videoDriver);
        }
    }

    // Get the display mode resolution
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
        // Failed to get the desktop mode
        int displays = SDL_GetNumVideoDisplays();
        if (displays < 1) {
            // Failed to even get a display, worst case we'll assume a default
            gScreenWidth = kDefaultScreenWidth;
            gScreenHeight = kDefaultScreenHeight;
        } else {
            // Try all the display devices until we find one that actually contains
            // display information
            for (int i = 0; i < displays; i++) {
                if (SDL_GetCurrentDisplayMode(i, &mode) != 0)
                    continue;
                // Found a display mode
                gScreenWidth = mode.w;
                gScreenHeight = mode.h;
            }
        }
    } else {
        // Use the desktop display mode
        gScreenWidth = mode.w;
        gScreenHeight = mode.h;
    }

    if (gScreenWidth <= 0 || gScreenHeight <= 0) {
        // In this event we fall back to the default resolution
        gScreenWidth = kDefaultScreenWidth;
        gScreenHeight = kDefaultScreenHeight;
    }

    gRefreshRate = (mode.refresh_rate == 0)
        ? kRefreshRate : mode.refresh_rate;

    // Coming from a config
    if (vid_width != 0 && vid_height != 0) {
        gScreenWidth = size_t(vid_width.get());
        gScreenHeight = size_t(vid_height.get());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE | SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (vid_fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *window = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        gScreenWidth,
        gScreenHeight,
        flags
    );

    if (!window || !SDL_GL_CreateContext(window)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Neothyne: Initialization error",
            "OpenGL 3.3 or higher is required",
            nullptr);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }

    return window;
}

// An accurate frame rate timer and capper
frameTimer::frameTimer()
    : m_maxFrameTicks(0.0f)
    , m_lastSecondTicks(0)
    , m_frameCount(0)
    , m_minTicks(0)
    , m_maxTicks(0)
    , m_averageTicks(0.0f)
    , m_deltaTime(0.0f)
    , m_lastFrameTicks(0)
    , m_currentTicks(0)
    , m_targetTicks(0)
    , m_frameMin(0)
    , m_frameMax(0)
    , m_frameAverage(0.0f)
    , m_framesPerSecond(0)
    , m_lock(false)
{
}

    float m_maxFrameTicks;
    uint32_t m_lastSecondTicks;
    int m_frameCount;
    uint32_t m_minTicks;
    uint32_t m_maxTicks;
    float m_averageTicks;
    float m_deltaTime;
    uint32_t m_lastFrameTicks;
    uint32_t m_currentTicks;
    uint32_t m_targetTicks;
    uint32_t m_frameMin;
    uint32_t m_frameMax;
    float m_frameAverage;
    int m_framesPerSecond;
    bool m_lock;

void frameTimer::lock() {
    m_lock = true;
}

void frameTimer::unlock() {
    m_lock = false;
}

void frameTimer::cap(float maxFps) {
    if (m_lock)
        return;
    m_maxFrameTicks = maxFps <= 0.0f
        ? -1 : (1000.0f / maxFps) - kDampenEpsilon;
}

void frameTimer::reset() {
    m_frameCount = 0;
    m_minTicks = 1000;
    m_maxTicks = 0;
    m_averageTicks = 0;
    m_lastSecondTicks = SDL_GetTicks();
}

bool frameTimer::update() {
    m_frameCount++;
    m_targetTicks = m_maxFrameTicks != -1 ?
        m_lastSecondTicks + uint32_t(m_frameCount * m_maxFrameTicks) : 0;
    m_currentTicks = SDL_GetTicks();
    m_averageTicks += m_currentTicks - m_lastFrameTicks;
    if (m_currentTicks - m_lastFrameTicks <= m_minTicks)
        m_minTicks = m_currentTicks - m_lastFrameTicks;
    if (m_currentTicks - m_lastFrameTicks >= m_maxTicks)
        m_maxTicks = m_currentTicks - m_lastFrameTicks;
    if (m_targetTicks && m_currentTicks < m_targetTicks) {
        uint32_t beforeDelay = SDL_GetTicks();
        SDL_Delay(m_targetTicks - m_currentTicks);
        m_currentTicks = SDL_GetTicks();
        m_averageTicks += m_currentTicks - beforeDelay;
    }
    m_deltaTime = 0.001f * (m_currentTicks - m_lastFrameTicks);
    m_lastFrameTicks = m_currentTicks;
    if (m_currentTicks - m_lastSecondTicks >= 1000) {
        m_framesPerSecond = m_frameCount;
        m_frameAverage = m_averageTicks / m_frameCount;
        m_frameMin = m_minTicks;
        m_frameMax = m_maxTicks;
        reset();
        return true;
    }
    return false;
}

float frameTimer::mspf() const {
    return m_frameAverage;
}

int frameTimer::fps() const {
    return m_framesPerSecond;
}

float frameTimer::delta() const {
    return m_deltaTime;
}

uint32_t frameTimer::ticks() const {
    return m_currentTicks;
}

u::map<u::string, int> &neoKeyState(const u::string &key, bool keyDown, bool keyUp) {
    if (keyDown)
        gKeyMap[key]++;
    if (keyUp)
        gKeyMap[key] = 0;
    return gKeyMap;
}

void neoMouseDelta(int *deltaX, int *deltaY) {
    if (SDL_GetRelativeMouseMode() == SDL_TRUE)
        SDL_GetRelativeMouseState(deltaX, deltaY);
}

mouseState neoMouseState() {
    return gMouseState;
}

void *neoGetProcAddress(const char *proc) {
    return SDL_GL_GetProcAddress(proc);
}

void neoBindSet(const u::string &what, void (*handler)()) {
    gBinds[what] = handler;
}

void (*neoBindGet(const u::string &what))() {
    if (gBinds.find(what) != gBinds.end())
        return gBinds[what];
    return nullptr;
}

void neoSwap() {
    SDL_GL_SwapWindow(gScreen);
    gTimer.update();

    auto callBind = [](const char *what) {
        if (gBinds.find(what) != gBinds.end())
            gBinds[what]();
    };

    // Event dispatch
    char format[1024];
    const char *keyName;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                neoResize(e.window.data1, e.window.data2);
                break;
            }
            break;
        case SDL_KEYDOWN:
            keyName = SDL_GetKeyName(e.key.keysym.sym);
            snprintf(format, sizeof(format), "%sDn", keyName);
            callBind(format);
            neoKeyState(keyName, true);
            break;
        case SDL_KEYUP:
            keyName = SDL_GetKeyName(e.key.keysym.sym);
            snprintf(format, sizeof(format), "%sUp", keyName);
            callBind(format);
            neoKeyState(keyName, false, true);
            break;
        case SDL_MOUSEMOTION:
            gMouseState.x = e.motion.x;
            gMouseState.y = neoHeight() - e.motion.y;
            break;
        case SDL_MOUSEWHEEL:
            gMouseState.wheel = e.wheel.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                callBind("MouseDnL");
                gMouseState.button |= mouseState::kMouseButtonLeft;
                break;
            case SDL_BUTTON_RIGHT:
                callBind("MouseDnR");
                gMouseState.button |= mouseState::kMouseButtonRight;
                break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                callBind("MouseUpL");
                gMouseState.button &= ~mouseState::kMouseButtonLeft;
                break;
            case SDL_BUTTON_RIGHT:
                callBind("MouseUpR");
                gMouseState.button &= ~mouseState::kMouseButtonRight;
                break;
            }
            break;
        case SDL_TEXTINPUT:
            gTextInput += e.text.text;
            break;
        }
    }
}

size_t neoWidth() {
    return gScreenWidth;
}

size_t neoHeight() {
    return gScreenHeight;
}

void neoRelativeMouse(bool state) {
    SDL_SetRelativeMouseMode(state ? SDL_TRUE : SDL_FALSE);
}

bool neoRelativeMouse() {
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

void neoCenterMouse() {
    SDL_WarpMouseInWindow(gScreen, gScreenWidth / 2, gScreenHeight / 2);
}

void neoSetWindowTitle(const char *title) {
    SDL_SetWindowTitle(gScreen, title);
}

void neoResize(size_t width, size_t height) {
    SDL_SetWindowSize(gScreen, width, height);
    gScreenWidth = width;
    gScreenHeight = height;
    gl::Viewport(0, 0, width, height);
    vid_width.set(width);
    vid_height.set(height);
}

void neoSetVSyncOption(int option) {
    switch (option) {
        case kSyncTear:
            if (SDL_GL_SetSwapInterval(-1) == -1)
                neoSetVSyncOption(kSyncRefresh);
            break;
        case kSyncNone:
            SDL_GL_SetSwapInterval(0);
            break;
        case kSyncEnabled:
            SDL_GL_SetSwapInterval(1);
            break;
        case kSyncRefresh:
            SDL_GL_SetSwapInterval(0);
            gTimer.unlock();
            gTimer.cap(gRefreshRate);
            gTimer.lock();
            break;
    }
    gTimer.reset();
}

const u::string &neoUserPath() { return gUserPath; }
const u::string &neoGamePath() { return gGamePath; }

void neoFatalError(const char *error) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Neothyne: Fatal error", error, nullptr);
    abort();
}

///
/// On Window the entry point is entered as such:
///     WinMain -> entryPoint -> neoMain
/// For everywhere else:
///     main -> entryPoint -> neoMain
///
static int entryPoint(int argc, char **argv) {
    extern int neoMain(frameTimer &timer, int argc, char **argv);

    // Must come after SDL is loaded
    gTimer.reset();
    gTimer.cap(frameTimer::kMaxFPS);

    // Check for command line '-gamedir'
    --argc;
    const char *directory = nullptr;
    for (int i = 0; i < argc - 1; i++) {
        if (!strcmp(argv[i + 1], "-gamedir") && argv[i + 2]) {
            directory = argv[i + 2];
            break;
        }
    }

    gGamePath = directory ? directory : u::format(".%cgame%c", PATH_SEP, PATH_SEP);
    if (gGamePath.find(PATH_SEP) == u::string::npos)
        gGamePath += PATH_SEP;

    // Get a path for the user
    char *get = SDL_GetPrefPath("Neothyne", "");
    gUserPath = get;
    gUserPath.pop_back(); // Remove additional path separator
    SDL_free(get);

    // Verify all the paths exist for the user directory. If they don't exist
    // create them.
    static const char *paths[] = {
        "screenshots", "cache"
    };

    for (size_t i = 0; i < sizeof(paths)/sizeof(*paths); i++) {
        u::string path = gUserPath + paths[i];
        if (u::exists(path, u::kDirectory))
            continue;
        u::mkdir(path);
    }

    readConfig();

    gScreen = getContext();

    neoSetVSyncOption(vid_vsync);
    gTimer.cap(vid_maxfps.get());

    SDL_ShowCursor(0);

    gl::init();

    // back face culling
    gl::FrontFace(GL_CW);
    gl::CullFace(GL_BACK);
    gl::Enable(GL_CULL_FACE);

    gl::Enable(GL_LINE_SMOOTH);
    gl::Hint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    const char *vendor = (const char *)gl::GetString(GL_VENDOR);
    const char *renderer = (const char *)gl::GetString(GL_RENDERER);
    const char *version = (const char *)gl::GetString(GL_VERSION);
    const char *shader = (const char *)gl::GetString(GL_SHADING_LANGUAGE_VERSION);

    u::print("Vendor: %s\nRenderer: %s\nDriver: %s\nShading: %s\n",
        vendor, renderer, version, shader);
    u::print("Game: %s\nUser: %s\n", neoGamePath(), neoUserPath());

    int status = neoMain(gTimer, argc, argv);
    SDL_Quit();
    return status;
}

// So we don't need to depend on SDL_main we provide our own
#ifdef _WIN32
#include <ctype.h>
#include "u_vector.h"
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
    (void)hInst;
    (void)hPrev;
    (void)szCmdLine;
    (void)sw;

    auto parseCommandLine = [](const char *src, u::vector<char *> &args) {
        char *buf = new char[strlen(src) + 1];
        char *dst = buf;
        for (;;) {
            while (isspace(*src))
                src++;
            if (!*src)
                break;
            args.push_back(dst);
            for (bool quoted = false; *src && (quoted || !isspace(*src)); src++) {
                if (*src != '"')
                    *dst++ = *src;
                else if (dst > buf && src[-1] == '\\')
                    dst[-1] = '"';
                else
                    quoted = !quoted;
            }
            *dst++ = '\0';
        }
        args.push_back(nullptr);
        return buf;
    };
    u::vector<char *> args;
    char *buf = parseCommandLine(GetCommandLine(), args);
    SDL_SetMainReady();
    int status = entryPoint(args.size() - 1, &args[0]);
    delete[] buf;
    exit(status);
    return 0;
}
#else
int main(int argc, char **argv) {
    return entryPoint(argc, argv);
}
#endif
