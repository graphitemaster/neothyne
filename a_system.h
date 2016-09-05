#ifndef A_SYSTEM_HDR
#define A_SYSTEM_HDR
#include "m_vec.h"

#include "u_vector.h"
#include "u_string.h"

#include "a_fader.h"

namespace a {

struct Audio;
struct Filter;
struct FilterInstance;

struct LockGuard {
    LockGuard(void *opaque);
    ~LockGuard();
    void unlock();
    operator bool() const;
private:
    void *m_mutex;
};

inline LockGuard::operator bool() const {
    return m_mutex;
}

// Clever technique to make a scope that locks a mutex
#define locked(X) \
    for (LockGuard lock(X); lock; lock.unlock())

/// maximum filters per stream
static constexpr int kMaxStreamFilters = 4;

struct SourceInstance {
    enum {
        kLooping = 1 << 0,
        kProtected = 1 << 1,
        kPaused = 1 << 2
    };

    SourceInstance();
    virtual ~SourceInstance();
    void init(size_t playIndex, float baseSampleRate, size_t channels, int sourceFlags);

protected:
    friend struct LaneInstance;
    friend struct Lane;
    friend struct Audio;

    virtual void getAudio(float *buffer, size_t samples) = 0;
    virtual bool hasEnded() const = 0;
    virtual void seek(float seconds, float *scratch, size_t samples);
    virtual bool rewind();

    unsigned int m_playIndex;
    int m_flags;
    size_t m_channels;
    m::vec3 m_volume; // left, right, global scale
    float m_pan;
    float m_baseSampleRate;
    float m_sampleRate;
    float m_relativePlaySpeed;
    float m_streamTime;
    Fader m_panFader;
    Fader m_volumeFader;
    Fader m_relativePlaySpeedFader;
    Fader m_pauseScheduler;
    Fader m_stopScheduler;
    int m_sourceID;
    int m_laneHandle;
    FilterInstance *m_filters[kMaxStreamFilters];

    // if there is an active fader
    int m_activeFader;
    // fader-affected l/r volume
    float m_faderVolume[2 * 2];
};

struct Source {
    enum {
        kLoop = 1 << 0,
        kSingleInstance = 1 << 1
    };

    Source();

    void setLooping(bool looping);
    void setSingleInstance(bool singleInstance);

    virtual void setFilter(int filterHandle, Filter *filter);

    virtual ~Source();
    virtual SourceInstance *create() = 0;

protected:
    friend struct LaneInstance;
    friend struct Lane;
    friend struct Audio;

    int m_flags;
    float m_baseSampleRate;
    size_t m_channels;
    int m_sourceID;
    Filter *m_filters[kMaxStreamFilters];
    Audio *m_owner;
};

struct Audio {
    Audio(int flags);
    ~Audio();

    enum {
        kClipRoundOff = 1 << 0
    };

    void init(int channels, int sampleRate, int bufferSize, int flags);
    void mix(float *buffer, size_t samples);

    int play(Source &sound, float volume = 1.0f, float pan = 0.0f, bool paused = false, int lane = 0);

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
    void setVolumeAll(float volume);
    void setGlobalFilter(int filterHandle, Filter *filter);

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

    // So a driver is what establishes a connection with the device(s) present on
    // the system. Depending on the driver, the devices may be different. To actually
    // get an understanding of which devices are present under which driver we
    // effectively initialize the audio subsystem for each driver and read the
    // device strings. Then we can present these options to the user.
    struct Driver {
        u::string name; // driver name
        u::vector<u::string> devices;
    };

    const u::vector<Driver> &drivers() const;

private:
    friend struct LaneInstance;
    friend struct Lane;

    void mixLane(float *buffer, size_t samples, float *scratch, int lane);

    friend struct Source;

    int findFreeVoice();
    int getHandleFromVoice(int voice) const;
    int getVoiceFromHandle(int voiceHandle) const;

    void stopSound(Source &sound);
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

    static void mixer(void *user, unsigned char *stream, int length);

    u::vector<float> m_scratch;
    size_t m_scratchNeeded;
    u::vector<SourceInstance *> m_voices;
    int m_sampleRate;
    int m_bufferSize;
    int m_flags;
    float m_globalVolume;
    float m_postClipScaler;
    unsigned int m_playIndex;
    Fader m_globalVolumeFader;
    float m_streamTime;
    int m_sourceID;
    Filter *m_filters[kMaxStreamFilters];
    FilterInstance *m_filterInstances[kMaxStreamFilters];

    u::vector<Driver> m_drivers;

    void *m_mutex;
    int m_device;
    float *m_mixerData;
    bool m_hasFloat;
};

inline const u::vector<Audio::Driver> &Audio::drivers() const {
    return m_drivers;
}

}

#endif
