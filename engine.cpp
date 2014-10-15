#include <stdarg.h>

#include <SDL2/SDL_stdinc.h>

#include "engine.h"
#include "r_common.h"
#include "c_var.h"

// Engine globals
static u::map<int, int> gKeyMap;
static size_t gScreenWidth = 0;
static size_t gScreenHeight = 0;
static size_t gRefreshRate = 0;
static SDL_Window *gScreen = nullptr;
static frameTimer gTimer;

// maximum resolution is 15360x8640 (8640p) (16:9)
// minimum resolution is 640x480 (VGA) (4:3)
// default resolution is 1024x768 (XGA) (4:3)
static constexpr int kDefaultScreenWidth = 1024;
static constexpr int kDefaultScreenHeight = 768;
static constexpr size_t kRefreshRate = 60;

static c::var<int> vid_vsync("vid_vsync", "vertical syncronization", -1, kSyncRefresh, kSyncNone);
static c::var<int> vid_fullscreen("vid_fullscreen", "toggle fullscreen", 0, 1, 1);
static c::var<int> vid_width("vid_width", "resolution width", 480, 15360, 0);
static c::var<int> vid_height("vid_height", "resolution height", 240, 8640, 0);
static c::var<int> vid_maxfps("vid_maxfps", "cap framerate", 24, 3600, 0);

// An accurate frame rate timer and capper
frameTimer::frameTimer() :
    m_deltaTime(0.0f),
    m_lastFrameTicks(0),
    m_currentTicks(0),
    m_frameAverage(0.0f),
    m_framesPerSecond(0),
    m_lock(false)
{
    reset();
    cap(kMaxFPS);
}

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

u::map<int, int> &neoKeyState(int key, bool keyDown, bool keyUp) {
    if (keyDown)
        gKeyMap[key]++;
    if (keyUp)
        gKeyMap[key] = 0;
    return gKeyMap;
}

void neoMouseDelta(int *deltaX, int *deltaY) {
    SDL_GetRelativeMouseState(deltaX, deltaY);
}

void neoSwap() {
    SDL_GL_SwapWindow(gScreen);
    gTimer.update();
}

size_t neoWidth() {
    return gScreenWidth;
}

size_t neoHeight() {
    return gScreenHeight;
}

void neoToggleRelativeMouseMode() {
    SDL_SetRelativeMouseMode(
        SDL_GetRelativeMouseMode() == SDL_TRUE
            ? SDL_FALSE : SDL_TRUE);
}

void neoSetWindowTitle(const char *title) {
    SDL_SetWindowTitle(gScreen, title);
}

void neoResize(size_t width, size_t height) {
    SDL_SetWindowSize(gScreen, width, height);
    gScreenWidth = width;
    gScreenHeight = height;
    gl::Viewport(0, 0, width, height);
}

void neoSetVSyncOption(vSyncOption option) {
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

static SDL_Window *getContext() {
    SDL_Init(SDL_INIT_VIDEO);

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
        gScreenWidth = static_cast<size_t>(vid_width.get());
        gScreenHeight = static_cast<size_t>(vid_height.get());
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

    SDL_SetRelativeMouseMode(SDL_TRUE);

    return window;
}

void neoFatal(const char *fmt, ...) {
    char buffer[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Neothyne: Fatal error", buffer, nullptr);
    SDL_DestroyWindow(gScreen);
    SDL_Quit();
    abort();
}

u::string neoPath() {
    u::string path;
    char *get = SDL_GetPrefPath("Neothyne", "");
    path = get;
    SDL_free(get);
    return path;
}

// So we don't need to depend on SDL_main we provide our own
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
    ()hInst;
    ()hPrev;
    ()szCmdLine;
    ()sw;

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
    int status = SDL_main(args.size() - 1, &args[0]);
    delete[] buf;
    exit(status);
    return 0;
}
#endif

///
/// On Window the entry point is entered as such:
///     WinMain -> SDL_main -> main -> neoMain
/// For everywhere else:
///     main -> neoMain
///
extern int neoMain(frameTimer &timer, int argc, char **argv);
int main(int argc, char **argv) {
    c::readConfig();

    gScreen = getContext();

    neoSetVSyncOption(static_cast<vSyncOption>(vid_vsync.get()));
    gTimer.cap(vid_maxfps.get());

    gl::init();
    gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // back face culling
    gl::FrontFace(GL_CW);
    gl::CullFace(GL_BACK);
    gl::Enable(GL_CULL_FACE);

    const char *vendor = (const char *)gl::GetString(GL_VENDOR);
    const char *renderer = (const char *)gl::GetString(GL_RENDERER);
    const char *version = (const char *)gl::GetString(GL_VERSION);
    const char *shader = (const char *)gl::GetString(GL_SHADING_LANGUAGE_VERSION);

    printf("Vendor: %s\nRenderer: %s\nDriver: %s\nShading: %s\n",
        vendor, renderer, version, shader);

    return neoMain(gTimer, argc, argv);
}
