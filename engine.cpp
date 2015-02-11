#include <stdarg.h>

#include <SDL2/SDL.h>

#include "engine.h"
#include "cvar.h"

#include "r_common.h"

#include "u_file.h"
#include "u_misc.h"

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

/// pimpl context
struct context {
    struct controller {
        controller() = default;
    private:
        friend struct engine;
        friend struct context;
        SDL_GameController *gamePad;
        SDL_Joystick *joyStick;
        const char *name;
    };
    ~context();
    context();

    void addController(int id);
    void delController(int id);
    controller *getController(int id);

    void begTextInput();
    void endTextInput();

private:
    friend struct engine;
    u::map<int, controller> m_controllers;
    SDL_Window *m_window;
    u::string m_textString;
    textState m_textState;
};

#define CTX(X) ((context*)(X))

inline context::context()
    : m_window(nullptr)
    , m_textState(textState::kInactive)
{
}

inline context::~context() {
    if (m_window)
        SDL_DestroyWindow(m_window);
    SDL_Quit();
}

inline void context::addController(int id) {
    // Add the new game controller
    controller cntrl;
    cntrl.gamePad = SDL_GameControllerOpen(id);
    cntrl.joyStick = SDL_GameControllerGetJoystick(cntrl.gamePad);
    cntrl.name = SDL_GameControllerNameForIndex(id);
    int instance = SDL_JoystickInstanceID(cntrl.joyStick);
    m_controllers.insert({ instance, cntrl });

    u::print("[input] => gamepad %d (%s) connected\n", instance, cntrl.name);
}

inline void context::delController(int instance) {
    auto cntrl = getController(instance);
    if (cntrl) {
        u::print("[input] => gamepad %d (%s) disconnected\n", instance, cntrl->name);
        SDL_GameControllerClose(cntrl->gamePad);
    }
}

inline context::controller *context::getController(int id) {
    if (m_controllers.find(id) == m_controllers.end())
        return nullptr;
    return &m_controllers[id];
}

inline void context::begTextInput() {
    m_textState = textState::kInputting;
    m_textString = "";
}

inline void context::endTextInput() {
    if (m_textState != textState::kInputting)
        return;
    m_textState = textState::kFinished;
}

/// engine
static engine gEngine;

inline engine::engine()
    : m_screenWidth(0)
    , m_screenHeight(0)
    , m_refreshRate(0)
    , m_context(nullptr)
{
}

inline engine::~engine() {
    delete CTX(m_context);
}

bool engine::init(int &argc, char **argv) {
    // Establish local timers for the engine
    if (!initTimers())
        return false;
    // Establish game and user directories + configuration
    if (!initData(argc, argv))
        return false;
    // Launch the context
    if (!initContext())
        return false;

    setVSyncOption(vid_vsync);
    m_frameTimer.cap(vid_maxfps);

    return true;
}

bool engine::initContext() {
    const u::string &videoDriver = vid_driver.get();
    if (videoDriver.size()) {
        if (SDL_GL_LoadLibrary(videoDriver.c_str()) != 0) {
            u::print("Failed to load video driver: %s\n", SDL_GetError());
            return false;
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
            m_screenWidth = kDefaultScreenWidth;
            m_screenHeight = kDefaultScreenHeight;
        } else {
            // Try all the display devices until we find one that actually contains
            // display information
            for (int i = 0; i < displays; i++) {
                if (SDL_GetCurrentDisplayMode(i, &mode) != 0)
                    continue;
                // Found a display mode
                m_screenWidth = mode.w;
                m_screenHeight = mode.h;
            }
        }
    } else {
        // Use the desktop display mode
        m_screenWidth = mode.w;
        m_screenHeight = mode.h;
    }

    if (m_screenWidth <= 0 || m_screenHeight <= 0) {
        // In this event we fall back to the default resolution
        m_screenWidth = kDefaultScreenWidth;
        m_screenHeight = kDefaultScreenHeight;
    }

    m_refreshRate = (mode.refresh_rate == 0)
        ? kRefreshRate : mode.refresh_rate;

    // Coming from a config
    if (vid_width != 0 && vid_height != 0) {
        m_screenWidth = size_t(vid_width.get());
        m_screenHeight = size_t(vid_height.get());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE | SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    u::unique_ptr<context> ctx(new context);

    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (vid_fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    ctx->m_window = SDL_CreateWindow("Neothyne",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        m_screenWidth,
        m_screenHeight,
        flags
    );

    if (!ctx->m_window || !SDL_GL_CreateContext(ctx->m_window)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Neothyne: Initialization error",
            "OpenGL 3.3 or higher is required",
            nullptr
        );
        return false;
    }

    // Hide the cursor for the window
    SDL_ShowCursor(0);

    // Find all controllers (that may be plugged in)
    SDL_GameControllerEventState(SDL_IGNORE);
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i))
            continue;
        ctx->addController(i);
    }
    SDL_GameControllerEventState(SDL_ENABLE);

    // Own the context now
    m_context = (void*)ctx.release();

    return true;
}

bool engine::initTimers() {
    m_frameTimer.reset();
    m_frameTimer.cap(frameTimer::kMaxFPS);
    return true;
}

bool engine::initData(int &argc, char **argv) {
    // Get a path for the game, can come from command line as well
    const char *directory = nullptr;
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "-gamedir") && argv[i + 1]) {
            directory = argv[i + 1];
            argc -= 2;
            // Found a game directory, remove it and the directory from argv
            memmove(argv+i, argv+i+2, (argc-i) * sizeof(char*));
            break;
        }
    }

    // Verify the game directory even exists
    if (directory && !u::exists(directory, u::kDirectory)) {
        u::print("Game directory `%s' doesn't exist (falling back to .%cgame%c)\n",
            directory, u::kPathSep, u::kPathSep);
        directory = nullptr;
    }

    m_gamePath = directory ? directory : u::format(".%cgame%c", u::kPathSep, u::kPathSep);
    if (m_gamePath.find(u::kPathSep) == u::string::npos)
        m_gamePath += u::kPathSep;

    // Get a path for the user
    auto get = SDL_GetPrefPath("Neothyne", "");
    m_userPath = get;
    m_userPath.pop_back(); // Remove additional path separator
    SDL_free(get);

    // Verify all the paths exist for the user directory. If they don't exist
    // create them.
    static const char *paths[] = {
        "screenshots", "cache"
    };

    for (auto &it : paths) {
        u::string path = m_userPath + it;
        if (u::exists(path, u::kDirectory))
            continue;
        if (u::mkdir(path))
            continue;
    }

    // Established game and user data paths, now load the config
    readConfig(m_userPath);
    return true;
}

u::map<u::string, int> &engine::keyState(const u::string &key, bool keyDown, bool keyUp) {
    if (keyDown)
        m_keyMap[key]++;
    if (keyUp)
        m_keyMap[key] = 0;
    return m_keyMap;
}

void engine::mouseDelta(int *deltaX, int *deltaY) {
    if (SDL_GetRelativeMouseMode() == SDL_TRUE)
        SDL_GetRelativeMouseState(deltaX, deltaY);
}

mouseState engine::mouse() const {
    return m_mouseState;
}

void engine::bindSet(const u::string &what, void (*handler)()) {
    m_binds[what] = handler;
}

void engine::swap() {
    SDL_GL_SwapWindow(CTX(m_context)->m_window);
    m_frameTimer.update();

    auto callBind = [this](const char *what) {
        if (m_binds.find(what) != m_binds.end())
            m_binds[what]();
    };

    m_mouseState.wheel = 0;

    char format[1024];
    const char *keyName;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERDEVICEADDED:
            CTX(m_context)->addController(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            CTX(m_context)->delController(e.cdevice.which);
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                resize(e.window.data1, e.window.data2);
                break;
            }
            break;
        case SDL_KEYDOWN:
            if (CTX(m_context)->m_textState == textState::kInputting) {
                if (e.key.keysym.sym == SDLK_RETURN) {
                    CTX(m_context)->endTextInput();
                } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    u::string &input = CTX(m_context)->m_textString;
                    if (input.size())
                        input.pop_back();
                }
                break;
            } else if (e.key.keysym.sym == SDLK_SLASH) {
                CTX(m_context)->begTextInput();
                break;
            }
            keyName = SDL_GetKeyName(e.key.keysym.sym);
            snprintf(format, sizeof(format), "%sDn", keyName);
            callBind(format);
            keyState(keyName, true);
            break;
        case SDL_KEYUP:
            keyName = SDL_GetKeyName(e.key.keysym.sym);
            snprintf(format, sizeof(format), "%sUp", keyName);
            callBind(format);
            keyState(keyName, false, true);
            break;
        case SDL_MOUSEMOTION:
            m_mouseState.x = e.motion.x;
            m_mouseState.y = m_screenHeight - e.motion.y;
            break;
        case SDL_MOUSEWHEEL:
            m_mouseState.wheel = e.wheel.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                callBind("MouseDnL");
                m_mouseState.button |= mouseState::kMouseButtonLeft;
                break;
            case SDL_BUTTON_RIGHT:
                callBind("MouseDnR");
                m_mouseState.button |= mouseState::kMouseButtonRight;
                break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                callBind("MouseUpL");
                m_mouseState.button &= ~mouseState::kMouseButtonLeft;
                break;
            case SDL_BUTTON_RIGHT:
                callBind("MouseUpR");
                m_mouseState.button &= ~mouseState::kMouseButtonRight;
                break;
            }
            break;
        case SDL_TEXTINPUT:
            // Ignore the first "/"
            if (CTX(m_context)->m_textString.empty() && !strcmp(e.text.text, "/"))
                break;
            CTX(m_context)->m_textString += e.text.text;
            break;
        }
    }
}

size_t engine::width() const {
    return m_screenWidth;
}

size_t engine::height() const {
    return m_screenHeight;
}

void engine::relativeMouse(bool state) {
    SDL_SetRelativeMouseMode(state ? SDL_TRUE : SDL_FALSE);
}

bool engine::relativeMouse() {
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

void engine::centerMouse() {
    SDL_WarpMouseInWindow(CTX(m_context)->m_window, m_screenWidth / 2, m_screenHeight / 2);
}

void engine::setWindowTitle(const char *title) {
    SDL_SetWindowTitle(CTX(m_context)->m_window, title);
}

void engine::resize(size_t width, size_t height) {
    SDL_SetWindowSize(CTX(m_context)->m_window, width, height);
    m_screenWidth = width;
    m_screenHeight = height;
    gl::Viewport(0, 0, width, height);
    vid_width.set(width);
    vid_height.set(height);
}

void engine::setVSyncOption(int option) {
    switch (option) {
        case kSyncTear:
            if (SDL_GL_SetSwapInterval(-1) == -1)
                setVSyncOption(kSyncRefresh);
            break;
        case kSyncNone:
            SDL_GL_SetSwapInterval(0);
            break;
        case kSyncEnabled:
            SDL_GL_SetSwapInterval(1);
            break;
        case kSyncRefresh:
            SDL_GL_SetSwapInterval(0);
            m_frameTimer.unlock();
            m_frameTimer.cap(m_refreshRate);
            m_frameTimer.lock();
            break;
    }
    m_frameTimer.reset();
}

const u::string &engine::userPath() const {
    return m_userPath;
}

const u::string &engine::gamePath() const {
    return m_gamePath;
}

textState engine::textInput(u::string &what) {
    if (CTX(m_context)->m_textState == textState::kInactive)
        return textState::kInactive;
    what = CTX(m_context)->m_textString;
    if (CTX(m_context)->m_textState == textState::kFinished) {
        CTX(m_context)->m_textState = textState::kInactive;
        return textState::kFinished;
    }
    return textState::kInputting;
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

// Global functions
void neoFatalError(const char *error) {
    writeConfig(gEngine.userPath());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Neothyne: Fatal error", error, nullptr);
    abort();
}

void *neoGetProcAddress(const char *proc) {
    return SDL_GL_GetProcAddress(proc);
}

///
/// On Window the entry point is entered as such:
///     WinMain -> entryPoint -> neoMain
/// For everywhere else:
///     main -> entryPoint -> neoMain
///
static int entryPoint(int argc, char **argv) {
    extern int neoMain(frameTimer&, int argc, char **argv);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
        neoFatal("Failed to initialize SDL2");

    if (!gEngine.init(argc, argv))
        neoFatal("Failed to initialize engine");

    // Setup OpenGL
    gl::init();

    gl::FrontFace(GL_CW);
    gl::CullFace(GL_BACK);
    gl::Enable(GL_CULL_FACE);

    gl::Enable(GL_LINE_SMOOTH);
    gl::Hint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    auto vendor = (const char *)gl::GetString(GL_VENDOR);
    auto renderer = (const char *)gl::GetString(GL_RENDERER);
    auto version = (const char *)gl::GetString(GL_VERSION);
    auto shader = (const char *)gl::GetString(GL_SHADING_LANGUAGE_VERSION);

    // Intel online texture compression is slow
    if (strstr(vendor, "Intel"))
        gl::Hint(GL_TEXTURE_COMPRESSION_HINT, GL_FASTEST);

    u::print("Vendor: %s\nRenderer: %s\nDriver: %s\nShading: %s\n",
        vendor, renderer, version, shader);
    u::print("Game: %s\nUser: %s\n", gEngine.gamePath(), gEngine.userPath());

    // Launch the game
    int status = neoMain(gEngine.m_frameTimer, argc, argv);
    writeConfig(gEngine.userPath());
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

// Global wrapper (for now)
u::map<u::string, int> &neoKeyState(const u::string &key, bool keyDown, bool keyUp) {
    return gEngine.keyState(key, keyDown, keyUp);
}

mouseState neoMouseState() {
    return gEngine.mouse();
}

void neoMouseDelta(int *deltaX, int *deltaY) {
    gEngine.mouseDelta(deltaX, deltaY);
}

void neoSwap() {
    gEngine.swap();
}

size_t neoWidth() {
    return gEngine.width();
}

size_t neoHeight() {
    return gEngine.height();
}

void neoRelativeMouse(bool state) {
    gEngine.relativeMouse(state);
}

bool neoRelativeMouse() {
    return gEngine.relativeMouse();
}

void neoCenterMouse() {
    gEngine.centerMouse();
}

void neoSetWindowTitle(const char *title) {
    gEngine.setWindowTitle(title);
}

void neoResize(size_t width, size_t height) {
    gEngine.resize(width, height);
}

const u::string &neoUserPath() {
    return gEngine.userPath();
}

const u::string &neoGamePath() {
    return gEngine.gamePath();
}

textState neoTextState(u::string &what) {
    return gEngine.textInput(what);
}

void neoBindSet(const u::string &what, void (*handler)()) {
    gEngine.bindSet(what, handler);
}

void neoSetVSyncOption(int option) {
    gEngine.setVSyncOption(option);
}
