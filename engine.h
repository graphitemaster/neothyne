#ifndef ENGINE_HDR
#define ENGINE_HDR
#include <SDL2/SDL.h>
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

enum vSyncOption {
    kSyncTear = -1, ///< Late swap tearing
    kSyncNone,      ///< No vertical syncronization
    kSyncEnabled,   ///< Vertical syncronization
    kSyncRefresh    ///< No vertical syncronization, cap framerate to monitor refresh rate
};

u::map<int, int> &neoKeyState(int key = 0, bool keyDown = false, bool keyUp = false);
void neoMouseDelta(int *deltaX, int *deltaY);
void neoSwap();
size_t neoWidth();
size_t neoHeight();
void neoRelativeMouse(bool state);
void neoCenterMouse();
void neoSetWindowTitle(const char *title);
void neoResize(size_t width, size_t height);
const u::string &neoUserPath();
const u::string &neoGamePath();

template <typename... Ts>
inline void neoFatal(const char *fmt, const Ts&... ts) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Neothyne: Fatal error",
        u::format(fmt, ts...).c_str(), nullptr);
    abort();
}

void neoSetVSyncOption(vSyncOption option);

#endif
