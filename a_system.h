#ifndef A_SYSTEM_HDR
#define A_SYSTEM_HDR
#include "m_vec.h"
#include "u_vector.h"
#include "a_fader.h"

namespace a {

struct audio;
struct filter;
struct filterInstance;

struct lockGuard {
    lockGuard(void *opaque);
    ~lockGuard();
    void unlock();
    operator bool() const;
private:
    void *m_mutex;
};

inline lockGuard::operator bool() const {
    return m_mutex;
}

// Clever technique to make a scope that locks a mutex
#define locked(X) \
    for (lockGuard lock(X); lock; lock.unlock())

/// maximum filters per stream
static constexpr int kMaxStreamFilters = 4;

struct sourceInstance {
    enum {
        kLooping = 1 << 0,
        kProtected = 1 << 1,
        kPaused = 1 << 2
    };

    sourceInstance();
    virtual ~sourceInstance();
    void init(size_t playIndex, float baseSampleRate, size_t channels, int sourceFlags);

protected:
    friend struct laneInstance;
    friend struct lane;

    virtual void getAudio(float *buffer, size_t samples) = 0;
    virtual bool hasEnded() const = 0;
    virtual void seek(float seconds, float *scratch, size_t samples);
    virtual bool rewind();

    friend struct audio;

    unsigned int m_playIndex;
    int m_flags;
    size_t m_channels;
    m::vec3 m_volume; // left, right, global scale
    float m_pan;
    float m_baseSampleRate;
    float m_sampleRate;
    float m_relativePlaySpeed;
    float m_streamTime;
    fader m_panFader;
    fader m_volumeFader;
    fader m_relativePlaySpeedFader;
    fader m_pauseScheduler;
    fader m_stopScheduler;
    int m_sourceID;
    int m_laneHandle;
    filterInstance *m_filters[kMaxStreamFilters];

    // if there is an active fader
    int m_activeFader;
    // fader-affected l/r volume
    float m_faderVolume[2 * 2];
};

struct source {
    enum {
        kLoop = 1 << 0
    };

    source();

    void setLooping(bool looping);
    virtual void setFilter(int filterHandle, filter *filter_);

    virtual ~source();
    virtual sourceInstance *create() = 0;

protected:
    friend struct laneInstance;
    friend struct lane;

    friend struct audio;

    int m_flags;
    float m_baseSampleRate;
    size_t m_channels;
    int m_sourceID;
    filter *m_filters[kMaxStreamFilters];
    audio *m_owner;
};

struct audio {
    audio();
    ~audio();

    enum {
        kClipRoundOff = 1 << 0
    };

    void init(int channels, int sampleRate, int bufferSize, int flags);
    void mix(float *buffer, size_t samples);

    int play(source &sound, float volume = 1.0f, float pan = 0.0f, bool paused = false, int lane = 0);

    void seek(int voiceHandle, float seconds);
    void stop(int voiceHandle);
    void stopAll();

    float getStreamTime(int voiceHandle) const;
    bool getPaused(int voiceHandle) const;
    float getVolume(int voiceHandle) const;
    float getPan(int voiceHandle) const;
    float getSampleRate(int voiceHandle) const;
    bool getProtected(int voiceHandle) const;
    float getPostClipScaler() const;
    float getGlobalVolume() const;
    float getRelativePlaySpeed(int voiceHandle) const;

    void setGlobalVolume(float volume);
    void setPaused(int voiceHandle, bool paused);
    void setPausedAll(bool paused);
    void setRelativePlaySpeed(int voiceHandle, float speed);
    void setPostClipScaler(float scaler);
    void setProtected(int voiceHandle, bool protect);
    void setSampleRate(int voiceHandle, float sampleRate);
    void setPan(int voiceHandle, float panning);
    void setPanAbsolute(int voiceHandle, const m::vec2 &panning);
    void setVolume(int voiceHandle, float volume);
    void setGlobalFilter(int filterHandle, filter *filter_);

    void fadeVolume(int voiceHandle, float to, float time);
    void fadePan(int voiceHandle, float to, float time);
    void fadeRelativePlaySpeed(int voiceHandle, float to, float time);
    void fadeGlobalVolume(float to, float time);

    void schedulePause(int voiceHandle, float time);
    void scheduleStop(int voiceHandle, float time);

    void oscVolume(int voiceHandle, float from, float to, float time);
    void oscPan(int voiceHandle, float from, float to, float time);
    void oscRelativePlaySpeed(int voiceHandle, float from, float to, float time);
    void oscGlobalVolume(float from, float to, float time);

private:
    friend struct laneInstance;
    friend struct lane;

    void mixLane(float *buffer, size_t samples, float *scratch, int lane);

    friend struct source;

    int findFreeVoice();
    int getHandleFromVoice(int voice) const;
    int getVoiceFromHandle(int voiceHandle) const;

    void stopSound(source &sound);
    void stopVoice(int channel);

    void setVoicePan(int channel, float panning);
    void setVoiceVolume(int channel, float volume);
    void setVoicePaused(int channel, bool paused);
    void setVoiceRelativePlaySpeed(int channel, float speed);

    void setFilterParam(int voiceHandle, int filterHandle, int attrib, float value);
    void fadeFilterParam(int voiceHandle, int filterHandle, int attrib, float from, float to, float time);
    void oscFilterParam(int voiceHandle, int filterHandle, int attrib, float from, float to, float time);

    void clip(const float *U_RESTRICT src, float *U_RESTRICT dst, size_t samples, const m::vec2 &volume);
    void interlace(const float *U_RESTRICT src, float *U_RESTRICT dst, size_t samples, size_t channels);

    u::vector<float> m_scratch;
    size_t m_scratchNeeded;
    u::vector<sourceInstance *> m_voices;
    int m_sampleRate;
    int m_bufferSize;
    int m_flags;
    float m_globalVolume;
    float m_postClipScaler;
    unsigned int m_playIndex;
    fader m_globalVolumeFader;
    float m_streamTime;
    int m_sourceID;
    filter *m_filters[kMaxStreamFilters];
    filterInstance *m_filterInstances[kMaxStreamFilters];

public:
    void *m_mutex;
    float *m_mixerData;
};

void init(a::audio *system, int channels = 32, int flags = audio::kClipRoundOff, int sampleRate = 44100, int bufferSize = 2048);
void stop(a::audio *system);

}

#endif
