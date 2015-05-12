#ifndef ENGINE_HDR
#define ENGINE_HDR
#include <stdint.h>

#include "u_map.h"
#include "u_string.h"
#include "u_misc.h"
#include "u_stack.h"

/// Frame timer
struct frameTimer {
    static constexpr size_t kMaxFPS = 0; ///< For capping framerate (0 = disabled)
    static constexpr float kDampenEpsilon = 0.00001f; ///< Dampening to stabilize framerate readings

    frameTimer();

    void cap(float maxFps); ///< Cap framerate to `maxFps' 0 to disable
    float mspf() const; ///< Milliseconds per frame
    int fps() const; ///< Frames per second
    float delta() const; ///< Frame delta
    uint32_t ticks() const; ///< Ticks
    void reset();
    bool update();

    void lock();
    void unlock();

private:
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
};

/// Types of frame synchronization methods
enum {
    kSyncTear = -1, ///< Late swap tearing
    kSyncNone,      ///< No vertical syncronization
    kSyncEnabled,   ///< Vertical syncronization
    kSyncRefresh    ///< No vertical syncronization, cap framerate to monitor refresh rate
};

/// Represents the given mouse state
struct mouseState {
    mouseState();
    enum {
        kMouseButtonLeft = 1 << 0, ///< left mouse button
        kMouseButtonRight = 1 << 1 ///< right mouse button
    };
    int x; ///< x position
    int y; ///< y position
    int wheel; ///< wheel value
    int button; ///< flags
};

inline mouseState::mouseState()
    : x(0)
    , y(0)
    , wheel(0)
    , button(0)
{
}

enum class textState {
    kInactive,
    kInputting,
    kFinished
};

/// Engine object
struct engine {
    engine();
    ~engine();

    bool init(int &argc, char **argv);

    typedef void (*bindFunction)();

    u::map<u::string, int> &keyState(const u::string &key = "", bool keyDown = false, bool keyUp = false);
    void mouseDelta(int *deltaX, int *deltaY);
    mouseState mouse() const;
    void bindSet(const u::string &what, bindFunction handler);
    void swap();
    size_t width() const;
    size_t height() const;
    void relativeMouse(bool state);
    bool relativeMouse();
    void centerMouse();
    void setWindowTitle(const char *title);
    void resize(size_t width, size_t height);
    void setVSyncOption(int option);
    void screenShot();

    const u::string &userPath() const;
    const u::string &gamePath() const;
    textState textInput(u::string &what);

    frameTimer m_frameTimer; // TODO: private

protected:
    bool initContext();
    bool initTimers();
    bool initData(int &argc, char **argv);

private:
    u::map<u::string, int> m_keyMap;
    u::map<u::string, bindFunction> m_binds;
    u::stack<u::string, 32> m_textInputHistory;
    size_t m_textInputHistoryCursor;
    u::vector<u::string> m_autoComplete;
    size_t m_autoCompleteCursor;
    u::string m_userPath;
    u::string m_gamePath;
    mouseState m_mouseState;
    size_t m_screenWidth;
    size_t m_screenHeight;
    size_t m_refreshRate;
    void *m_context; ///< pimpl for context
};

void neoFatalError(const char *error);
void *neoGetProcAddress(const char *proc);

template <typename... Ts>
inline void neoFatal(const char *fmt, const Ts&... ts) {
    neoFatalError(u::format(fmt, ts...).c_str());
}

// Wrapper
u::map<u::string, int> &neoKeyState(const u::string &key = "", bool keyDown = false, bool keyUp = false);
mouseState neoMouseState();
textState neoTextState(u::string &what);
void neoMouseDelta(int *deltaX, int *deltaY);
void neoSwap();
size_t neoWidth();
size_t neoHeight();
void neoRelativeMouse(bool state);
bool neoRelativeMouse();
void neoCenterMouse();
void neoSetWindowTitle(const char *title);
void neoResize(size_t width, size_t height);
const u::string &neoUserPath();
const u::string &neoGamePath();
void neoBindSet(const u::string &what, void (*handler)());
void (*neoBindGet(const u::string &what))();
void neoSetVSyncOption(int option);
void neoScreenShot();

#endif
