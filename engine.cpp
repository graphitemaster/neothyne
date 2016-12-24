#include <stdarg.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "texture.h"
#include "engine.h"

#include "c_console.h"
#include "c_complete.h"
#include "c_config.h"

#include "r_common.h"
#include "r_model.h"
#include "r_world.h"

#include "u_file.h"
#include "u_misc.h"
#include "u_set.h"
#include "u_log.h"

#include "s_runtime.h"
#include "s_memory.h"
#include "s_parser.h"
#include "s_object.h"
#include "s_util.h"
#include "s_gc.h"
#include "s_vm.h"

#include "m_vec.h"

///! Query the operating system name
static char gOperatingSystem[1024];
#if !defined(_WIN32) && !defined(_WIN64)
#  include <sys/utsname.h>
#else
#  define _WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif
static struct queryOperatingSystem {
    queryOperatingSystem() {
        // Operating system is unknown until further checking
        strcpy(gOperatingSystem, "Unknown");
#if !defined(_WIN32) && !defined(_WIN64)
        struct utsname n;
        if (uname(&n) == 0)
            snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s %s",
                n.sysname, n.release, n.machine);
#else
        static const size_t kRegQuerySize = 255;
        static char name[kRegQuerySize];
        static char version[kRegQuerySize];
        static char architecture[kRegQuerySize] = { '\0' };
        ULONG type = REG_SZ;
        ULONG size = kRegQuerySize;
        HKEY key = nullptr;
        bool inWine = false;
        // Find the CPU architecture using the registry
        LONG n = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            TEXT("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"),
            0, KEY_QUERY_VALUE, &key);
        if (n != ERROR_SUCCESS)
            goto failedArchitecture;
        if (RegQueryValueEx(key, "PROCESSOR_ARCHITECTURE", nullptr, &type,
            (LPBYTE)&architecture[0], &size) != ERROR_SUCCESS) {
            RegCloseKey(key);
            goto failedArchitecture;
        }
        RegCloseKey(key);
failedArchitecture:
        type = REG_SZ;
        size = kRegQuerySize;
        // Find the version using the registry
        n = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
            0, KEY_QUERY_VALUE, &key);
        if (n != ERROR_SUCCESS)
            goto failedProductName;
        if (RegQueryValueEx(key, "ProductName", nullptr, &type,
            (LPBYTE)&name[0], &size) != ERROR_SUCCESS) {
            RegCloseKey(key);
            goto failedProductName;
        }
        if (RegQueryValueEx(key, "CSDVersion", nullptr, &type,
            (LPBYTE)&version[0], &size) != ERROR_SUCCESS) {
            RegCloseKey(key);
            goto failedCSDVersion;
        }
        RegCloseKey(key);
        static constexpr const char kStripBuf[] = "Microsoft ";
        static constexpr size_t kStripLen = sizeof kStripBuf - 1;
        if (char *strip = strstr(name, kStripBuf))
            u::moveMemory(name, strip + kStripLen, 1 + strlen(strip + kStripLen));
        // Check if we're running in Wine
        n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wine"), 0, KEY_QUERY_VALUE, &key);
        if (n == ERROR_SUCCESS) {
            inWine = true;
            RegCloseKey(key);
        }
#if defined(_WIN32) && !defined(_WIN64)
        if (!strcmp(architecture, "AMD64")) {
            snprintf(gOperatingSystem, sizeof gOperatingSystem,
                "%s %s %s (32-bit binary on 64-bit CPU)", name, version,
                (inWine ? "(in Wine)" : ""));
        } else {
            snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s (32-bit)",
                name, version);
        }
#elif defined(_WIN64)
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s %s (64-bit)",
            name, version, (inWine ? "(in Wine)" : ""));
#else
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s %s",
            name, version, (inWine ? "(in Wine)" : "");
#endif
        return;
failedCSDVersion:
#if defined(_WIN32) && !defined(_WIN64)
        if (!strcmp(architecture, "AMD64")) {
            snprintf(gOperatingSystem, sizeof gOperatingSystem,
                "%s %s (32-bit binary on 64-bit CPU)", name,
                (inWine ? "(in Wine)" : ""));
        } else {
            snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s (32-bit)",
                name, (inWine ? "(in Wine)" : ""));
        }
#elif defined(_WIN64)
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s (64-bit)",
            name, (inWine ? "(in Wine)" : ""));
#else
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s",
            name, (inWine ? "(in Wine)" : ""));
#endif
        return;
failedProductName:
#if defined(_WIN32) && !defined(_WIN64)
        if (!strcmp(architecture, "AMD64")) {
            snprintf(gOperatingSystem, sizeof gOperatingSystem,
                "Windows %s (32-bit binary on 64-bit CPU)",
                (inWine ? "(in Wine)" : ""));
        } else {
            snprintf(gOperatingSystem, sizeof gOperatingSystem,
                "Windows %s (32-bit)", (inWine ? "(in Wine)" : ""));
        }
#elif defined(_WIN64)
        snprintf(gOperatingSystem, sizeof gOperatingSystem,
            "Windows %s (64-bit)", (inWine ? "(in Wine)" : ""));
#else
        snprintf(gOperatingSystem, sizeof gOperatingSystem,
            "Windows %s", (inWine ? "(in Wine)" : ""));
#endif
#endif
    }
} gQueryOperatingSystem;

static volatile bool gShutdown = false;

static void neoSignalHandler(int) {
    c::Config::write(neoUserPath());
    gShutdown = true;
}

// maximum resolution is 15360x8640 (8640p) (16:9)
// default resolution is 1024x768 (XGA) (4:3)
static constexpr int kDefaultScreenWidth = 1024;
static constexpr int kDefaultScreenHeight = 768;
static constexpr size_t kRefreshRate = 60;

VAR(int, vid_vsync, "vertical syncronization (-1 = late swap tearing, 0 = disabled, 1 = enabled, 2 = cap to refresh rate)", -1, kSyncRefresh, kSyncNone);
VAR(int, vid_fullscreen, "toggle fullscreen (0 = windowed mode, 1 = fullscreen)", 0, 1, 1);
VAR(int, vid_width, "resolution width (resuolution or if 0 and vid_height is also 0 engine uses desktop resolution)", 0, 15360, 0);
VAR(int, vid_height, "resolution height (resolution or if 0 and vid_weight is also 0 engine uses desktop resolution)", 0, 8640, 0);
VAR(int, vid_maxfps, "cap framerate (maximum fps allowed, 0 = unlimited)", 0, 3600, 0);
VAR(u::string, vid_driver, "video driver");
VAR(u::string, vid_display, "name of the display to use for video");

VAR(int, scr_info, "embed engine info in screenshot", 0, 1, 1);
VAR(int, scr_format, "screenshot file format (0 = BMP, 1 = TGA, 2 = PNG, 3 = JPG)", 0, 3, 3);
VAR(float, scr_quality, "screenshot quality", 0.0f, 1.0f, 1.0f);

/// pimpl context
struct Context {
    struct Controller {
        Controller();
        Controller(int id);

        const char *name() const;
        int instance() const;

        void update(float delta, const m::vec3 &limits);

    protected:
        void close();

    private:
        friend struct Engine;
        friend struct Context;

        SDL_GameController *m_gamePad;
        SDL_Joystick *m_joyStick;
        const char *m_name;
        int m_instance;
        m::vec3 m_position[2]; // L, R
        m::vec3 m_velocity[4]; // L, R, Rest L, Rest R
    };
    ~Context();
    Context();

    void addController(int id);
    void delController(int id);
    Controller *getController(int id);
    void updateController(int id, unsigned char axis, int16_t value);

    void begTextInput();
    void endTextInput();

private:
    friend struct Engine;
    u::map<int, Controller> m_controllers;
    SDL_GLContext m_gl;
    SDL_Window *m_window;
    u::string m_textString;
    TextState m_textState;
};

#define CTX(X) ((Context*)(X))

///! Context
inline Context::Context()
    : m_window(nullptr)
    , m_textState(TextState::kInactive)
{
    memset(&m_gl, 0, sizeof m_gl);
}

inline Context::~Context() {
    if (m_window) {
        SDL_GL_DeleteContext(m_gl);
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

inline void Context::addController(int id) {
    // Add the new game controller
    Controller cntrl(id);
    m_controllers.insert({ cntrl.instance(), cntrl });

    u::Log::out("[input] => gamepad %d (%s) connected\n", cntrl.m_instance, cntrl.name());
}

inline void Context::delController(int instance) {
    auto cntrl = getController(instance);
    if (!cntrl) return;
    u::Log::out("[input] => gamepad %d (%s) disconnected\n", instance, cntrl->name());
    cntrl->close();
}

inline Context::Controller *Context::getController(int id) {
    if (m_controllers.find(id) == m_controllers.end())
        return nullptr;
    return &m_controllers[id];
}

inline void Context::updateController(int instance, unsigned char axis, int16_t value) {
    auto cntrl = getController(instance);
    if (!cntrl) return;
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        cntrl->m_velocity[0].x = value / 32767.0f;
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        cntrl->m_velocity[0].y = value / 32767.0f;
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        cntrl->m_velocity[1].x = value / 32767.0f;
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        cntrl->m_velocity[1].y = value / 32767.0f;
        break;
    }
}

inline void Context::begTextInput() {
    m_textState = TextState::kInputting;
    m_textString = "";
}

inline void Context::endTextInput() {
    if (m_textState != TextState::kInputting)
        return;
    m_textState = TextState::kFinished;
}


///! Context::Controller
inline Context::Controller::Controller()
    : m_gamePad(nullptr)
    , m_joyStick(nullptr)
    , m_name(nullptr)
    , m_instance(0)
{
}

inline Context::Controller::Controller(int id)
    : m_gamePad(SDL_GameControllerOpen(id))
    , m_joyStick(SDL_GameControllerGetJoystick(m_gamePad))
    , m_name(SDL_GameControllerNameForIndex(id))
    , m_instance(SDL_JoystickInstanceID(m_joyStick))
{
    // Get idle positions
    const auto lx = SDL_GameControllerGetAxis(m_gamePad, SDL_CONTROLLER_AXIS_LEFTX);
    const auto ly = SDL_GameControllerGetAxis(m_gamePad, SDL_CONTROLLER_AXIS_LEFTY);
    const auto rx = SDL_GameControllerGetAxis(m_gamePad, SDL_CONTROLLER_AXIS_RIGHTX);
    const auto ry = SDL_GameControllerGetAxis(m_gamePad, SDL_CONTROLLER_AXIS_RIGHTY);

    m_velocity[2] = m::vec3(lx / 32767.0f, ly / 32767.0f, 0.0f);
    m_velocity[3] = m::vec3(rx / 32767.0f, ry / 32767.0f, 0.0f);
}

inline const char *Context::Controller::name() const {
    return m_name;
}

inline int Context::Controller::instance() const {
    return m_instance;
}

inline void Context::Controller::update(float delta, const m::vec3 &limits) {
    // If the velocity is close to the idle position kill it
    if (m::abs(m_velocity[2].x - m_velocity[0].x) < 0.1f) m_velocity[0].x = 0.0f;
    if (m::abs(m_velocity[2].y - m_velocity[0].y) < 0.1f) m_velocity[0].y = 0.0f;
    if (m::abs(m_velocity[3].x - m_velocity[1].x) < 0.1f) m_velocity[1].x = 0.0f;
    if (m::abs(m_velocity[3].y - m_velocity[1].y) < 0.1f) m_velocity[1].y = 0.0f;

    m_position[0] = m::clamp(m_position[0] + m_velocity[0] * delta, m::vec3::origin, limits);
    m_position[1] = m::clamp(m_position[1] + m_velocity[1] * delta, m::vec3::origin, limits);
}

inline void Context::Controller::close() {
    SDL_GameControllerClose(m_gamePad);
}

/// engine
static Engine gEngine;

inline Engine::Engine()
    : m_textInputHistoryCursor(0)
    , m_autoCompleteCursor(0)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_refreshRate(0)
    , m_context(nullptr)
{
}

inline Engine::~Engine() {
    delete CTX(m_context);
}

bool Engine::init(int &argc, char **argv) {
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

bool Engine::initContext() {
    const u::string &videoDriver = vid_driver.get();
    if (videoDriver.size()) {
        if (SDL_GL_LoadLibrary(videoDriver.c_str()) != 0) {
            u::Log::err("[video] => failed to load video driver: %s\n", SDL_GetError());
            return false;
        } else {
            u::Log::out("[video] => loaded video driver: %s\n", videoDriver);
        }
    }

    // search for a suitable display
    const u::string &displayName = vid_display.get();
    int display = -1;
    int displays = SDL_GetNumVideoDisplays();
    u::Log::out("[video] => found %d displays\n", displays);
    if (displayName.size())
        u::Log::out("[video] => searching for display `%s'\n", displayName);
    for (int i = 0; i < displays; i++) {
        const char *name = SDL_GetDisplayName(i);
        u::Log::out("[video] => found %s display `%s' ",
            name == displayName ? "matching" : "a", name);
        if (name == displayName)
            display = i;
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds))
            u::Log::out("\n");
        else
            u::Log::out("(%d x %d)\n", bounds.w, bounds.h);
    }
    if (display == -1) {
        // by default SDL uses display 0
        const char *name = SDL_GetDisplayName(0);
        if (name)
            vid_display.set(name);
        display = 0;
    }
    const char *selectedDisplayName = SDL_GetDisplayName(display);
    if (selectedDisplayName)
        u::Log::out("[video] => using display `%s'\n", selectedDisplayName);

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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    u::unique_ptr<Context> ctx(new Context);

    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (vid_fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

#if defined(_MSC_VER)
    if (IsDebuggerPresent())
    {
        flags &= ~SDL_WINDOW_FULLSCREEN;
        m_screenWidth = 800;
        m_screenHeight = 600;
    }
#endif

    char name[1024];
    snprintf(name, sizeof name, "Neothyne [%s]", gOperatingSystem);
    ctx->m_window = SDL_CreateWindow(
        name,
        display == -1 ? SDL_WINDOWPOS_UNDEFINED : SDL_WINDOWPOS_CENTERED_DISPLAY(display),
        display == -1 ? SDL_WINDOWPOS_UNDEFINED : SDL_WINDOWPOS_CENTERED_DISPLAY(display),
        m_screenWidth,
        m_screenHeight,
        flags
    );

    if (!ctx->m_window || !(ctx->m_gl = SDL_GL_CreateContext(ctx->m_window))) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Neothyne: Initialization error",
            "OpenGL 3.0 or higher is required",
            nullptr
        );
        return false;
    }

    // Hide the cursor for the window
    SDL_ShowCursor(0);

    // Application icon
    Texture icon;
    if (icon.load("icon")) {
        SDL_Surface *iconSurface = nullptr;
        if (icon.bpp() == 4) {
            iconSurface = SDL_CreateRGBSurfaceFrom((void *)icon.data(),
                icon.width(), icon.height(), icon.bpp()*8, icon.pitch(),
                0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        } else if (icon.bpp() == 3) {
            iconSurface = SDL_CreateRGBSurfaceFrom((void *)icon.data(),
                icon.width(), icon.height(), icon.bpp()*8, icon.pitch(),
                0x0000FF, 0x00FF00, 0xFF0000, 0);
        } else {
            neoFatal("Application icon must be 3bpp or 4bpp");
            return false;
        }
        SDL_SetWindowIcon(ctx->m_window, iconSurface);
        SDL_FreeSurface(iconSurface);
    }

    // Find all gamepads:
    // We ignore events since we need to calculate the idle state of the joysticks
    // on the devices. These idles states are used to know when the joysticks are
    // in resting position.
    //
    // This is not accurate. If the engine is launched with the joystick buttons
    // in a non-resting position the engine will take on that value as the new
    // resting position.
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

bool Engine::initTimers() {
    m_frameTimer.reset();
    m_frameTimer.cap(FrameTimer::kMaxFPS);
    return true;
}

void Engine::deleteConfigRecursive(const u::string &pathName) {
    if (u::dir::isFile(pathName)) {
        u::remove(pathName, u::kFile);
        return;
    }
    u::dir directory(pathName);
    for (const auto &it : directory)
        deleteConfigRecursive(u::fixPath(u::format("%s/%s", pathName, it)));
    u::remove(pathName, u::kDirectory);
}

void Engine::deleteConfig() {
    deleteConfigRecursive(m_userPath);
}

bool Engine::initData(int &argc, char **argv) {
    // Get a path for the game, can come from command line as well
    const char *directory = nullptr;
    for (int i = 1; i < argc - 1; i++) {
        if (argv[i] && argv[i + 1] && !strcmp(argv[i], "-gamedir")) {
            directory = argv[i + 1];
            argc -= 2;
            // Found a game directory, remove it and the directory from argv
            u::moveMemory(argv+i, argv+i+2, (argc-i) * sizeof *argv);
            break;
        }
    }

    // Check if the game directory even exists. But fix it to the platforms
    // path separator rules before verifying.
    // Engine paths
    static const char *kPaths[] = {
        "screenshots", "cache", "tmp"
    };
    u::string fixedDirectory;
    if (directory) {
        fixedDirectory = u::fixPath(directory);
        if (!u::exists(fixedDirectory, u::kDirectory)) {
            u::Log::out("Game directory `%s' doesn't exist (falling back to %s)\n",
                fixedDirectory, u::fixPath("./game/"));
        }
    }

    m_gamePath = fixedDirectory.empty() ? u::fixPath("./game") : fixedDirectory;

    // Add trailing path separator
    if (m_gamePath.end()[-1] != u::kPathSep)
        m_gamePath += u::kPathSep;

    // Verify that path even exists
    if (!u::exists(m_gamePath, u::kDirectory)) {
        u::Log::err("Game directory `%s' doesn't exist", u::fixPath(m_gamePath));
        return false;
    }

    // Get a path for the user
    const auto get = SDL_GetPrefPath("Neothyne", "");
    m_userPath = get;
    m_userPath.pop_back(); // Remove additional path separator
    m_userPath = u::fixPath(m_userPath); // Fix path separator (w.r.t platform)
    SDL_free(get);

    // Verify all the paths exist for the user directory. If they don't exist
    // create them.
    for (auto &it : kPaths) {
        u::string path = m_userPath + it;
        if (u::exists(path, u::kDirectory))
            continue;
        if (u::mkdir(path))
            continue;
    }

    // Established game and user data paths, now load the config
    c::Config::read(m_userPath);
    return true;
}

u::map<u::string, int> &Engine::keyState(const u::string &key, bool keyDown, bool keyUp) {
    if (keyDown)
        m_keyMap[key]++;
    if (keyUp)
        m_keyMap[key] = 0;
    return m_keyMap;
}

void Engine::mouseDelta(int *deltaX, int *deltaY) {
    if (SDL_GetRelativeMouseMode() == SDL_TRUE)
        SDL_GetRelativeMouseState(deltaX, deltaY);
}

MouseState Engine::mouse() const {
    return m_mouseState;
}

void Engine::bindSet(const u::string &what, void (*handler)()) {
    m_binds[what] = handler;
}

void Engine::swap() {
    SDL_GL_SwapWindow(CTX(m_context)->m_window);
    m_frameTimer.update();

    auto callBind = [this](const char *what) {
        if (m_binds.find(what) != m_binds.end())
            m_binds[what]();
    };

    m_mouseState.wheel = 0;

    char format[1024];
    const char *keyName;
    for (SDL_Event e; SDL_PollEvent(&e); ) {
        switch (e.type) {
        case SDL_QUIT:
            gShutdown = true;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            CTX(m_context)->addController(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            CTX(m_context)->delController(e.cdevice.which);
            break;
        case SDL_CONTROLLERAXISMOTION:
            CTX(m_context)->updateController(e.caxis.which, e.caxis.axis, e.caxis.value);
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                resize(e.window.data1, e.window.data2);
                break;
            }
            break;
        case SDL_KEYDOWN:
            if (CTX(m_context)->m_textState == TextState::kInputting) {
                if (e.key.keysym.sym != SDLK_TAB) {
                    m_autoComplete.destroy();
                    m_autoCompleteCursor = 0;
                }

                if (e.key.keysym.sym == SDLK_RETURN) {
                    CTX(m_context)->endTextInput();
                    // Keep input history if not empty-string
                    if (CTX(m_context)->m_textString.size()) {
                        if (m_textInputHistory.full()) {
                            m_textInputHistory.pop_back();
                            m_textInputHistoryCursor--;
                        }
                        m_textInputHistory.push_back(CTX(m_context)->m_textString);
                        m_textInputHistoryCursor++;
                    }
                } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    u::string &input = CTX(m_context)->m_textString;
                    if (input.size())
                        input.pop_back();
                } else if (e.key.keysym.sym == SDLK_TAB) {
                    if (m_autoComplete.empty())
                        m_autoComplete = c::Console::suggestions(CTX(m_context)->m_textString);
                    if (m_autoComplete.size()) {
                        if (m_autoComplete.size() <= m_autoCompleteCursor)
                            m_autoCompleteCursor = 0;
                        CTX(m_context)->m_textString = m_autoComplete[m_autoCompleteCursor++];
                    }
                } else if (e.key.keysym.sym == SDLK_UP && m_textInputHistory.size()) {
                    m_textInputHistoryCursor--;
                    if (m_textInputHistoryCursor == size_t(-1))
                        m_textInputHistoryCursor = m_textInputHistory.size() - 1;
                    CTX(m_context)->m_textString =
                        m_textInputHistory[m_textInputHistoryCursor];
                } else if (e.key.keysym.sym == SDLK_DOWN && m_textInputHistory.size()) {
                    m_textInputHistoryCursor++;
                    if (m_textInputHistoryCursor >= m_textInputHistory.size())
                        m_textInputHistoryCursor = 0;
                    CTX(m_context)->m_textString =
                        m_textInputHistory[m_textInputHistoryCursor];
                }
                break;
            } else if (e.key.keysym.sym == SDLK_SLASH) {
                CTX(m_context)->begTextInput();
                m_textInputHistoryCursor = m_textInputHistory.size();
                break;
            } else {
                keyName = SDL_GetKeyName(e.key.keysym.sym);
                snprintf(format, sizeof format, "%sDn", keyName);
                callBind(format);
                keyState(keyName, true);
                break;
            }
            break;
        case SDL_KEYUP:
            keyName = SDL_GetKeyName(e.key.keysym.sym);
            snprintf(format, sizeof format, "%sUp", keyName);
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
                m_mouseState.button |= MouseState::kMouseButtonLeft;
                break;
            case SDL_BUTTON_RIGHT:
                callBind("MouseDnR");
                m_mouseState.button |= MouseState::kMouseButtonRight;
                break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                callBind("MouseUpL");
                m_mouseState.button &= ~MouseState::kMouseButtonLeft;
                break;
            case SDL_BUTTON_RIGHT:
                callBind("MouseUpR");
                m_mouseState.button &= ~MouseState::kMouseButtonRight;
                break;
            }
            break;
        case SDL_TEXTINPUT:
            // Ignore text input if we're not inputting
            if (CTX(m_context)->m_textState != TextState::kInputting)
                break;
            // Ignore the first "/"
            if (CTX(m_context)->m_textString.empty() && !strcmp(e.text.text, "/"))
                break;
            CTX(m_context)->m_textString += e.text.text;
            break;
        }
    }

    for (auto &cntrl : CTX(m_context)->m_controllers)
        cntrl.second.update(m_frameTimer.delta(), { float(m_screenWidth), float(m_screenHeight), 0.0f });
}

size_t Engine::width() const {
    return m_screenWidth;
}

size_t Engine::height() const {
    return m_screenHeight;
}

void Engine::relativeMouse(bool state) {
#if defined(_MSC_VER)
    if (IsDebuggerPresent())
        SDL_SetRelativeMouseMode(SDL_FALSE);
    else
#endif
    SDL_SetRelativeMouseMode(state ? SDL_TRUE : SDL_FALSE);
}

bool Engine::relativeMouse() {
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

void Engine::centerMouse() {
    SDL_WarpMouseInWindow(CTX(m_context)->m_window, m_screenWidth / 2, m_screenHeight / 2);
}

void Engine::setWindowTitle(const char *title) {
    char name[1024];
    snprintf(name, sizeof name, "%s [%s]", title, gOperatingSystem);
    SDL_SetWindowTitle(CTX(m_context)->m_window, name);
}

void Engine::resize(size_t width, size_t height) {
    SDL_SetWindowSize(CTX(m_context)->m_window, width, height);
    m_screenWidth = width;
    m_screenHeight = height;
    gl::Viewport(0, 0, width, height);
    vid_width.set(width);
    vid_height.set(height);
}

void Engine::setVSyncOption(int option) {
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

void Engine::screenShot() {
    // Generate a unique filename from the time
    const time_t t = time(nullptr);
    const struct tm tm = *localtime(&t);
    const u::string file = u::format("%sscreenshots/%d-%d-%d-%d%d%d",
        neoUserPath(), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Get metrics for reading the final composite from GL
    const int screenWidth = int(neoWidth());
    const int screenHeight = int(neoHeight());
    const int screenSize = screenWidth * screenHeight;
    auto pixels = u::unique_ptr<unsigned char>(new unsigned char[screenSize * 3]);

    // make sure we're reading from the final framebuffer when obtaining the pixels
    // for the screenshot.
    gl::BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl::ReadPixels(0, 0, screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE,
        (GLvoid *)pixels.get());
    gl::PixelStorei(GL_PACK_ALIGNMENT, 8);

    // Construct a texture object from the pixel data
    Texture screenShot(&pixels[0], screenSize*3, screenWidth, screenHeight, false, kTexFormatRGB);
    screenShot.flip();
    if (scr_info) {
        size_t line = 0;
        auto vendor = (const char *)gl::GetString(GL_VENDOR);
        auto renderer = (const char *)gl::GetString(GL_RENDERER);
        auto version = (const char *)gl::GetString(GL_VERSION);
        auto shader = (const char *)gl::GetString(GL_SHADING_LANGUAGE_VERSION);
        screenShot.drawString(line, gOperatingSystem);
        screenShot.drawString(line, u::CPUDesc());
        screenShot.drawString(line, u::RAMDesc());
        screenShot.drawString(line, vendor);
        screenShot.drawString(line, renderer);
        screenShot.drawString(line, version);
        screenShot.drawString(line, shader);
        screenShot.drawString(line, "Extensions");
        for (auto &it : gl::extensions()) {
            char buffer[2048];
            snprintf(buffer, sizeof buffer, " %s", gl::extensionString(it));
            screenShot.drawString(line, buffer);
        }
    }

    auto fixedPath = u::fixPath(file);
    auto format = SaveFormat(scr_format.get());

    if (screenShot.save(file, format, scr_quality))
        u::Log::out("[screenshot] => %s.%s\n", fixedPath, kSaveFormatExtensions[scr_format]);
}

const u::string &Engine::userPath() const {
    return m_userPath;
}

const u::string &Engine::gamePath() const {
    return m_gamePath;
}

TextState Engine::textInput(u::string &what) {
    if (CTX(m_context)->m_textState == TextState::kInactive)
        return TextState::kInactive;
    what = CTX(m_context)->m_textString;
    if (CTX(m_context)->m_textState == TextState::kFinished) {
        CTX(m_context)->m_textState = TextState::kInactive;
        return TextState::kFinished;
    }
    return TextState::kInputting;
}

// An accurate frame rate timer and capper
FrameTimer::FrameTimer()
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

void FrameTimer::lock() {
    m_lock = true;
}

void FrameTimer::unlock() {
    m_lock = false;
}

void FrameTimer::cap(float maxFps) {
    if (m_lock)
        return;
    m_maxFrameTicks = maxFps <= 0.0f
        ? -1 : (1000.0f / maxFps) - kDampenEpsilon;
}

void FrameTimer::reset() {
    m_frameCount = 0;
    m_minTicks = 1000;
    m_maxTicks = 0;
    m_averageTicks = 0;
    m_lastSecondTicks = SDL_GetTicks();
}

bool FrameTimer::update() {
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

float FrameTimer::mspf() const {
    return m_frameAverage;
}

int FrameTimer::fps() const {
    return m_framesPerSecond;
}

float FrameTimer::delta() const {
    return m_deltaTime;
}

uint32_t FrameTimer::ticks() const {
    return m_currentTicks;
}

// Global functions
[[noreturn]] void neoFatalError(const char *error) {
    c::Config::write(gEngine.userPath());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Neothyne: Fatal error", error, nullptr);
    fflush(nullptr);
    abort();
}

void *neoGetProcAddress(const char *proc) {
    return SDL_GL_GetProcAddress(proc);
}

static void exec(const u::string &script) {
    // Allocate memory for Neo
    s::Memory::init();

    s::SourceRange source = s::SourceRange::readFile(script.c_str());
    if (!source.m_begin) {
        s::Memory::destroy();
        return;
    }

    // Allocate Neo state
    s::State state = { };
    state.m_shared = (s::SharedState *)s::Memory::allocate(sizeof *state.m_shared, 1);

    // Initialize garbage collector
    s::GC::init(&state);

    // Create a frame on the VM to execute the root object construction
    s::VM::addFrame(&state, 0, 0);
    s::Object *root = s::createRoot(&state);
    s::VM::delFrame(&state);

    // Create the pinning set for the GC
    s::RootSet set;
    s::GC::addRoots(&state, &root, 1, &set);

    // Specify the source contents
    s::SourceRecord::registerSource(source, script.c_str(), 0, 0);

    // Parse the result into our module
    s::UserFunction *module = nullptr;
    char *text = source.m_begin;
    s::ParseResult result = s::Parser::parseModule(&text, &module);

    //s::UserFunction::dump(module, 0);

    if (result == s::kParseOk) {
        // Execute the result on the VM
        s::VM::callFunction(&state, root, module, nullptr, 0);
        s::VM::run(&state);

        // Export the profile of the code
        s::ProfileState::dump(source, &state.m_shared->m_profileState);

        // Did we error out while running?
        if (state.m_runState == s::kErrored) {
            u::Log::err("[script] => \e[1m\e[31merror:\e[0m \e[1m%s\e[0m\n", state.m_error);
            s::VM::printBacktrace(&state);
        }
    }

    // Tear down the objects represented by this set
    s::GC::delRoots(&state, &set);

    // Reclaim memory
    s::GC::run(&state);

    // Reclaim any leaking memory
    s::Memory::destroy();
}

///
/// On Window the entry point is entered as such:
///     WinMain -> entryPoint -> neoMain
/// For everywhere else:
///     main -> entryPoint -> neoMain
///
static int entryPoint(int argc, char **argv) {
    extern int neoMain(FrameTimer&, a::Audio &, r::World &, int argc, char **argv, bool &shutdown);

    signal(SIGINT, neoSignalHandler);
    signal(SIGTERM, neoSignalHandler);

#if !defined(_WIN32)
    // For muxless setups on Linux with combination discrete and non-discrete
    // graphics, we want to utilize the discrete one. By convention, most people
    // use DRI_PRIME=1 for this. This is what we do as well.
    setenv("DRI_PRIME", "1", 1);
#endif

    c::Console::initialize();

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

    gl::Hint(GL_TEXTURE_COMPRESSION_HINT, GL_FASTEST);
    gl::Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // Intel online texture compression is slow (even with the compression hint.)
    if (strstr(vendor, "Intel")) {
        // Silently fails if not compiled with -DDXT_COMPRESSOR
        auto &dxtCompressor = c::Console::value<int>("r_dxt_compressor");
        dxtCompressor.set(1);
    }

    u::Log::out("[video] => Vendor: %s\n", vendor);
    u::Log::out("[video] => Renderer: %s\n", renderer);
    u::Log::out("[video] => Driver: %s\n", version);
    u::Log::out("[video] => Shading: %s (using %s)\n", shader, gl::glslVersionString());
    u::Log::out("[video] => Extensions:\n");

    for (const auto &it : gl::extensions())
        u::Log::out("            %s\n", gl::extensionString(it));

    u::Log::out("[system] => OS: %s\n", gOperatingSystem);
    u::Log::out("[system] => CPU: %s\n", u::CPUDesc());
    u::Log::out("[system] => RAM: %s\n", u::RAMDesc());
    u::Log::out("[system] => Game: %s\n", gEngine.gamePath());
    u::Log::out("[system] => User: %s\n", gEngine.userPath());

    a::Audio *audio = new a::Audio(a::Audio::kClipRoundOff);
    r::World *world = new r::World();

    // Launch the init.neo
    exec(gEngine.gamePath() + "init.neo");

    // Launch the game
    const int status = neoMain(gEngine.m_frameTimer, *audio, *world, argc, argv, (bool &)gShutdown);
    c::Config::write(gEngine.userPath());

    // Instance must be released before OpenGL context is lost
    r::geomMethods::instance().release();

    delete world;
    delete audio;

    // shut down the console (frees the console variables)
    c::Console::shutdown();
    return status;
}

// So we don't need to depend on SDL_main we provide our own
#if defined(_WIN32)
#include "u_vector.h"
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
    (void)hInst;
    (void)hPrev;
    (void)szCmdLine;
    (void)sw;

    freopen(u::fixPath(gEngine.userPath() + "/stdout.txt").c_str(), "w", stdout);
    freopen(u::fixPath(gEngine.userPath() + "/stderr.txt").c_str(), "w", stderr);

    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    auto parseCommandLine = [](const char *src, u::vector<char *> &args) {
        char *const buf = new char[strlen(src) + 1];
        char *dst = buf;
        for (;;) {
            while (u::isspace(*src))
                src++;
            if (!*src)
                break;
            args.push_back(dst);
            for (bool quoted = false; *src && (quoted || !u::isspace(*src)); src++) {
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
    const char *const buf = parseCommandLine(GetCommandLine(), args);
    SDL_SetMainReady();
    const int status = entryPoint(args.size() - 1, &args[0]);
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

const FrameTimer &neoFrameTimer() {
    return gEngine.m_frameTimer;
}

MouseState neoMouseState() {
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

TextState neoTextState(u::string &what) {
    return gEngine.textInput(what);
}

void neoBindSet(const u::string &what, void (*handler)()) {
    gEngine.bindSet(what, handler);
}

void neoSetVSyncOption(int option) {
    gEngine.setVSyncOption(option);
}

void neoScreenShot() {
    gEngine.screenShot();
}

void neoDeleteConfig() {
    gEngine.deleteConfig();
}
