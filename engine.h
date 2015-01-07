#ifndef ENGINE_HDR
#define ENGINE_HDR
#include <stdint.h>

#include "u_map.h"
#include "u_string.h"
#include "u_misc.h"

struct frameTimer {
    static constexpr size_t kMaxFPS = 0; // For capping framerate (0 = disabled)
    static constexpr float kDampenEpsilon = 0.00001f; // The dampening to remove flip-flip in frame metrics

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

enum {
    kSyncTear = -1, ///< Late swap tearing
    kSyncNone,      ///< No vertical syncronization
    kSyncEnabled,   ///< Vertical syncronization
    kSyncRefresh    ///< No vertical syncronization, cap framerate to monitor refresh rate
};

struct mouseState {
    // kMouseButton flags for packet
    enum {
        kMouseButtonLeft = 1 << 0,
        kMouseButtonRight = 1 << 1
    };
    int x;
    int y;
    int wheel;
    int button;
};

u::map<u::string, int> &neoKeyState(const u::string &key = "", bool keyDown = false, bool keyUp = false);
mouseState neoMouseState();
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

void neoFatalError(const char *error);

template <typename... Ts>
inline void neoFatal(const char *fmt, const Ts&... ts) {
    neoFatalError(u::format(fmt, ts...).c_str());
}

void neoSetVSyncOption(int option);

void *neoGetProcAddress(const char *proc);

#endif
