#ifndef ENGINE_HDR
#define ENGINE_HDR
#include <SDL2/SDL.h>

#include <stdint.h>
#include "util.h"

struct frameTimer {
    static constexpr size_t kMaxFPS = 0; // For capping framerate (0 = disabled)
    static constexpr float kDampenEpsilon = 0.00001f; // The dampening to remove flip-flip in frame metrics

    frameTimer();

    void cap(float maxFps); ///< Cap framerate to `maxFps' 0 to disable
    float mspf(void) const; ///< Milliseconds per frame
    int fps(void) const; ///< Frames per second
    float delta(void) const; ///< Frame delta
    uint32_t ticks(void) const; ///< Ticks
    void reset(void);
    bool update(void);

    void lock(void);
    void unlock(void);

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
void neoSwap(void);
size_t neoWidth(void);
size_t neoHeight(void);
void neoToggleRelativeMouseMode(void);
void neoSetWindowTitle(const char *title);
void neoResize(size_t width, size_t height);
u::string neoPath(void);
void neoFatal(const char *fmt, ...);

void neoSetVSyncOption(vSyncOption option);

#endif
