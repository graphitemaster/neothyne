#include <stdarg.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "texture.h"
#include "engine.h"
#include "cvar.h"

#include "c_complete.h"

#include "r_common.h"
#include "r_model.h"

#include "u_file.h"
#include "u_misc.h"
#include "u_set.h"

#include "m_vec.h"

// To render text into screen shots we use an 8x8 bitmap font
static const uint64_t kFont[128] = {
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x0000000000000000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,0x7E7E7E7E7E7E0000,
    0x0000000000000000,0x0808080800080000,0x2828000000000000,0x00287C287C280000,
    0x081E281C0A3C0800,0x6094681629060000,0x1C20201926190000,0x0808000000000000,
    0x0810202010080000,0x1008040408100000,0x2A1C3E1C2A000000,0x0008083E08080000,
    0x0000000000081000,0x0000003C00000000,0x0000000000080000,0x0204081020400000,
    0x1824424224180000,0x08180808081C0000,0x3C420418207E0000,0x3C420418423C0000,
    0x081828487C080000,0x7E407C02423C0000,0x3C407C42423C0000,0x7E04081020400000,
    0x3C423C42423C0000,0x3C42423E023C0000,0x0000080000080000,0x0000080000081000,
    0x0006186018060000,0x00007E007E000000,0x0060180618600000,0x3844041800100000,
    0x003C449C945C201C,0x1818243C42420000,0x7844784444780000,0x3844808044380000,
    0x7844444444780000,0x7C407840407C0000,0x7C40784040400000,0x3844809C44380000,
    0x42427E4242420000,0x3E080808083E0000,0x1C04040444380000,0x4448507048440000,
    0x40404040407E0000,0x4163554941410000,0x4262524A46420000,0x1C222222221C0000,
    0x7844784040400000,0x1C222222221C0200,0x7844785048440000,0x1C22100C221C0000,
    0x7F08080808080000,0x42424242423C0000,0x8142422424180000,0x4141495563410000,
    0x4224181824420000,0x4122140808080000,0x7E040810207E0000,0x3820202020380000,
    0x4020100804020000,0x3808080808380000,0x1028000000000000,0x00000000007E0000,
    0x1008000000000000,0x003C023E463A0000,0x40407C42625C0000,0x00001C20201C0000,
    0x02023E42463A0000,0x003C427E403C0000,0x0018103810100000,0x0000344C44340438,
    0x2020382424240000,0x0800080808080000,0x0800180808080870,0x20202428302C0000,
    0x1010101010180000,0x0000665A42420000,0x00002E3222220000,0x00003C42423C0000,
    0x00005C62427C4040,0x00003A46423E0202,0x00002C3220200000,0x001C201804380000,
    0x00103C1010180000,0x00002222261A0000,0x0000424224180000,0x000081815A660000,
    0x0000422418660000,0x0000422214081060,0x00003C08103C0000,0x1C103030101C0000,
    0x0808080808080800,0x38080C0C08380000,0x000000324C000000,0x7E7E7E7E7E7E0000
};

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
#if defined(_WIN32)
        if (!strcmp(architecture, "AMD64")) {
            snprintf(gOperatingSystem, sizeof gOperatingSystem,
                "%s %s (32-bit binary on 64-bit CPU)", name, version);
        } else {
            snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s (32-bit)",
                name, version);
        }
#elif defined(_WIN64)
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s (64-bit)",
            name, version);
#else
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s %s",
            name, version);
#endif
        return;
failedCSDVersion:
#if defined(_WIN32)
        if (!strcmp(architecture, "AMD64")) {
            snprintf(gOperatingSystem, sizeof gOperatingSystem,
                "%s (32-bit binary on 64-bit CPU)", name);
        } else {
            snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s (32-bit)",
                name);
        }
#elif defined(_WIN64)
        snprintf(gOperatingSystem, sizeof gOperatingSystem, "%s (64-bit)", name);
#else
        strcpy(gOperatingSystem, name);
#endif
        return;
failedProductName:
#if defined(_WIN32)
        if (!strcmp(architecture, "AMD64"))
            strcpy(gOperatingSystem, "Windows (32-bit binary on 64-bit CPU)");
        else
            strcpy(gOperatingSystem, "Windows (32-bit)");
#elif defined(_WIN64)
        strcpy(gOperatingSystem, "Windows (64-bit)");
#else
        strcpy(gOperatingSystem, "Windows");
#endif
#endif
    }
} gQueryOperatingSystem;

static volatile bool gShutdown = false;

static void neoSignalHandler(int) {
    writeConfig(neoUserPath());
    gShutdown = true;
}

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

VAR(int, scr_info, "embed engine info in screenshot", 0, 1, 1);
VAR(int, scr_format, "screenshot file format", 0, 2, 2);
VAR(float, scr_quality, "screenshot quality", 0.0f, 1.0f, 1.0f);

/// pimpl context
struct context {
    struct controller {
        controller();
        controller(int id);

        const char *name() const;
        int instance() const;

        void update(float delta, const m::vec3 &limits);

    protected:
        void close();

    private:
        friend struct engine;
        friend struct context;

        SDL_GameController *m_gamePad;
        SDL_Joystick *m_joyStick;
        const char *m_name;
        int m_instance;
        m::vec3 m_position[2]; // L, R
        m::vec3 m_velocity[4]; // L, R, Rest L, Rest R
    };
    ~context();
    context();

    void addController(int id);
    void delController(int id);
    controller *getController(int id);
    void updateController(int id, unsigned char axis, int16_t value);

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

///! context
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
    controller cntrl(id);
    m_controllers.insert({ cntrl.instance(), cntrl });

    u::print("[input] => gamepad %d (%s) connected\n", cntrl.m_instance, cntrl.name());
}

inline void context::delController(int instance) {
    auto cntrl = getController(instance);
    if (!cntrl) return;
    u::print("[input] => gamepad %d (%s) disconnected\n", instance, cntrl->name());
    cntrl->close();
}

inline context::controller *context::getController(int id) {
    if (m_controllers.find(id) == m_controllers.end())
        return nullptr;
    return &m_controllers[id];
}

inline void context::updateController(int instance, unsigned char axis, int16_t value) {
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

inline void context::begTextInput() {
    m_textState = textState::kInputting;
    m_textString = "";
}

inline void context::endTextInput() {
    if (m_textState != textState::kInputting)
        return;
    m_textState = textState::kFinished;
}


///! context::controller
inline context::controller::controller()
    : m_gamePad(nullptr)
    , m_joyStick(nullptr)
    , m_name(nullptr)
    , m_instance(0)
{
}

inline context::controller::controller(int id)
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

inline const char *context::controller::name() const {
    return m_name;
}

inline int context::controller::instance() const {
    return m_instance;
}

inline void context::controller::update(float delta, const m::vec3 &limits) {
    // If the velocity is close to the idle position kill it
    if (m::abs(m_velocity[2].x - m_velocity[0].x) < 0.1f) m_velocity[0].x = 0.0f;
    if (m::abs(m_velocity[2].y - m_velocity[0].y) < 0.1f) m_velocity[0].y = 0.0f;
    if (m::abs(m_velocity[3].x - m_velocity[1].x) < 0.1f) m_velocity[1].x = 0.0f;
    if (m::abs(m_velocity[3].y - m_velocity[1].y) < 0.1f) m_velocity[1].y = 0.0f;

    m_position[0] = m::clamp(m_position[0] + m_velocity[0] * delta, m::vec3::origin, limits);
    m_position[1] = m::clamp(m_position[1] + m_velocity[1] * delta, m::vec3::origin, limits);
}

inline void context::controller::close() {
    SDL_GameControllerClose(m_gamePad);
}

/// engine
static engine gEngine;

inline engine::engine()
    : m_textInputHistoryCursor(0)
    , m_autoCompleteCursor(0)
    , m_screenWidth(0)
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    u::unique_ptr<context> ctx(new context);

    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (vid_fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    char name[1024];
    snprintf(name, sizeof name, "Neothyne [%s]", gOperatingSystem);
    ctx->m_window = SDL_CreateWindow(
        name,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        m_screenWidth,
        m_screenHeight,
        flags
    );

    if (!ctx->m_window || !SDL_GL_CreateContext(ctx->m_window)) {
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
    texture icon;
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

bool engine::initTimers() {
    m_frameTimer.reset();
    m_frameTimer.cap(frameTimer::kMaxFPS);
    return true;
}

void engine::deleteConfigRecursive(const u::string &pathName) {
    if (u::dir::isFile(pathName)) {
        u::remove(pathName, u::kFile);
        return;
    }
    u::dir directory(pathName);
    for (const auto &it : directory)
        deleteConfigRecursive(u::fixPath(u::format("%s/%s", pathName, it)));
    u::remove(pathName, u::kDirectory);
}

void engine::deleteConfig() {
    deleteConfigRecursive(m_userPath);
}

bool engine::initData(int &argc, char **argv) {
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
        "screenshots", "cache", "cache/textures", "cache/shaders"
    };
    u::string fixedDirectory;
    if (directory) {
        fixedDirectory = u::fixPath(directory);
        if (!u::exists(fixedDirectory, u::kDirectory)) {
            u::print("Game directory `%s' doesn't exist (falling back to %s)\n",
                fixedDirectory, u::fixPath("./game/"));
        }
    }

    m_gamePath = fixedDirectory.empty() ? u::fixPath("./game") : fixedDirectory;

    // Add trailing path separator
    if (m_gamePath.end()[-1] != u::kPathSep)
        m_gamePath += u::kPathSep;

    // Verify that path even exists
    if (!u::exists(m_gamePath, u::kDirectory)) {
        u::print("Game directory `%s' doesn't exist", u::fixPath(m_gamePath));
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
            if (CTX(m_context)->m_textState == textState::kInputting) {
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
                        m_autoComplete = c::complete::find(CTX(m_context)->m_textString);
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
            }
            keyName = SDL_GetKeyName(e.key.keysym.sym);
            snprintf(format, sizeof format, "%sDn", keyName);
            callBind(format);
            keyState(keyName, true);
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

    for (auto &cntrl : CTX(m_context)->m_controllers)
        cntrl.second.update(m_frameTimer.delta(), { float(m_screenWidth), float(m_screenHeight), 0.0f });
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
    char name[1024];
    snprintf(name, sizeof name, "%s [%s]", title, gOperatingSystem);
    SDL_SetWindowTitle(CTX(m_context)->m_window, name);
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

void engine::screenShot() {
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

    // Reorient because it will be upside down
    auto temp = u::unique_ptr<unsigned char>(new unsigned char[screenSize * 3]);
    texture::reorient(pixels.get(), screenWidth, screenHeight, 3, screenWidth * 3,
        temp.get(), false, true, false);

    // Rasterize some information into the screen shot
    if (scr_info) {
        size_t line = 0;
        auto drawString = [&temp, &line](const char *s) {
            u::vector<unsigned char> pixelData;
            size_t c = 0;
            unsigned char glyph[8*8*3];
            for (; *s; s++, c++) {
                memset(glyph, 0, sizeof glyph);
                unsigned char *prev = &glyph[8*8*3-1];
                uint64_t n = 1;
                for (size_t h = 0; h < 8; h++)
                    for (size_t w = 0; w < 8; w++, n+=n)
                        for (size_t k = 0; k < 3; k++)
                            *prev-- = (kFont[int(*s)] & n) ? 255 : 0;
                const size_t size = pixelData.size();
                pixelData.resize(size + sizeof glyph);
                memcpy(&pixelData[size], glyph, sizeof glyph);
            }
            const size_t size = 8*c*8*3,
                         offset = pixelData.size();
            pixelData.resize(size + offset);
            for (size_t i = 0; i < c; i++) {
                unsigned char *s = &pixelData[8*i*8*3];
                unsigned char *d = &pixelData[offset]+i*8*3;
                for (size_t h = 0; h < 8; h++, s+=8*3, d+=8*c*3)
                    memcpy(d, s, 8*3);
            }
            for (size_t h = 0; h < 8; h++) {
                unsigned char *s = &pixelData[offset]+8*c*3*h;
                unsigned char *d = temp.get() + 8*neoWidth()*3*line + neoWidth()*3*h;
                memcpy(d, s, 8*c*3);
            }
            line++;
        };

        // Print some info
        auto vendor = (const char *)gl::GetString(GL_VENDOR);
        auto renderer = (const char *)gl::GetString(GL_RENDERER);
        auto version = (const char *)gl::GetString(GL_VERSION);
        auto shader = (const char *)gl::GetString(GL_SHADING_LANGUAGE_VERSION);

        drawString(gOperatingSystem);
        drawString(vendor);
        drawString(renderer);
        drawString(version);
        drawString(shader);
        drawString("Extensions");
        for (auto &it : gl::extensions()) {
            char buffer[2048];
            snprintf(buffer, sizeof buffer, " %s", gl::extensionString(it));
            drawString(buffer);
        }
    }

    texture tex;
    if (!tex.from(temp.get(), screenSize*3, screenWidth, screenHeight, false, kTexFormatRGB))
        u::print("[screenshot] => failed to create screenshot texture\n");

    auto fixedPath = u::fixPath(file);
    switch (scr_format) {
    case kSaveBMP:
        if (tex.save(file, kSaveBMP, scr_quality))
            u::print("[screenshot] => %s.bmp\n", fixedPath);
        break;
    case kSaveTGA:
        if (tex.save(file, kSaveTGA, scr_quality))
            u::print("[screenshot] => %s.tga\n", fixedPath);
        break;
    case kSavePNG:
        if (tex.save(file, kSavePNG, scr_quality))
            u::print("[screenshot] => %s.png\n", fixedPath);
        break;
    }
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
[[noreturn]] void neoFatalError(const char *error) {
    writeConfig(gEngine.userPath());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Neothyne: Fatal error", error, nullptr);
    fflush(nullptr);
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
    extern int neoMain(frameTimer&, int argc, char **argv, bool &shutdown);

    signal(SIGINT, neoSignalHandler);
    signal(SIGTERM, neoSignalHandler);

#if !defined(_WIN32)
    // For muxless setups on Linux with combination discrete and non-discrete
    // graphics, we want to utilize the discrete one. By convention, most people
    // use DRI_PRIME=1 for this. This is what we do as well.
    setenv("DRI_PRIME", "1", 1);
#endif

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
        auto &dxtCompressor = varGet<int>("r_dxt_compressor");
        dxtCompressor.set(1);
    }

    u::print("OS: %s\nVendor: %s\nRenderer: %s\nDriver: %s\nShading: %s (using %s)\nExtensions:\n",
        gOperatingSystem, vendor, renderer, version, shader, gl::glslVersionString());

    for (const auto &it : gl::extensions())
        u::print(" %s\n", gl::extensionString(it));

    u::print("Game: %s\nUser: %s\n", gEngine.gamePath(), gEngine.userPath());

    // Launch the game
    const int status = neoMain(gEngine.m_frameTimer, argc, argv, (bool &)gShutdown);
    writeConfig(gEngine.userPath());

    // Instance must be released before OpenGL context is lost
    r::geomMethods::instance().release();

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

#if defined(_MSC_VER)
    freopen(u::fixPath(neoGamePath() + "/stdout.txt").c_str(), "w", stdout);
    freopen(u::fixPath(neoGamePath() + "/stderr.txt").c_str(), "w", stderr);
#endif

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

void neoScreenShot() {
    gEngine.screenShot();
}

void neoDeleteConfig() {
    gEngine.deleteConfig();
}
