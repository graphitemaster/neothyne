#ifndef A_SYSTEM_HDR
#define A_SYSTEM_HDR
#include "m_vec.h"
#include "u_vector.h"

namespace a {

struct audio;

struct fader {
    fader();

    void set(float from, float to, float time, float startTime);
    float get(float time);

private:
    friend struct audio;

    float m_current;
    float m_from;
    float m_to;
    float m_delta;
    float m_time;
    float m_startTime;
    float m_endTime;
    int m_active;
};

struct instance {
    enum {
        kLooping = 1 << 0,
        kStereo = 1 << 1,
        kProtected = 1 << 2,
        kPaused = 1 << 3
    };

    instance();
    virtual ~instance();
    void init(size_t playIndex, float baseSampleRate, int sourceFlags);

protected:
    virtual void getAudio(float *buffer, size_t samples) = 0;
    virtual bool hasEnded() const = 0;
    virtual void seek(float seconds, float *scratch, size_t samples);
    virtual bool rewind();

    friend struct audio;

    unsigned int m_playIndex;
    int m_flags;
    m::vec3 m_volume; // left, right, global scale
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
};

struct source {
    enum {
        kLoop = 1 << 0,
        kStereo = 1 << 1
    };

    source();

    void setLooping(bool looping);

    virtual ~source();
    virtual instance *create() = 0;

protected:
    friend struct audio;

    int m_flags;
    float m_baseSampleRate;
    int m_sourceID;
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

    int play(source &sound, float volume = 1.0f, float pan = 0.0f, bool paused = false);

    void seek(int channelHandle, float seconds);
    void stop(int channelHandle);
    void stopAll();

    float getStreamTime(int channelHandle) const;
    bool getPaused(int channelHandle) const;
    float getVolume(int channelHandle) const;
    float getSampleRate(int channelHandle) const;
    bool getProtected(int channelHandle) const;
    float getPostClipScaler() const;
    float getRelativePlaySpeed(int channelHandle) const;

    void setGlobalVolume(float volume);
    void setPaused(int channelHandle, bool paused);
    void setPausedAll(bool paused);
    void setRelativePlaySpeed(int channelHandle, float speed);
    void setPostClipScaler(float scaler);
    void setProtected(int channelHandle, bool protect);
    void setSampleRate(int channelHandle, float sampleRate);
    void setPan(int channelHandle, float panning);
    void setPanAbsolute(int channelHandle, const m::vec2 &panning);
    void setVolume(int channelHandle, float volume);

    void fadeVolume(int channelHandle, float from, float to, float time);
    void fadePan(int channelHandle, float from, float to, float time);
    void fadeRelativePlaySpeed(int channelHandle, float from, float to, float time);
    void fadeGlobalVolume(float from, float to, float time);

    void schedulePause(int channelHandle, float time);
    void scheduleStop(int channelHandle, float time);

protected:
    friend struct source;

    int findFreeChannel();
    int getChannelFromHandle(int channelHandle) const;

    void stopSound(source &sound);
    void stopChannel(int channel);
    void setChannelPan(int channel, float panning);
    void setChannelVolume(int channel, float volume);
    void setChannelPaused(int channel, bool paused);
    void setChannelRelativePlaySpeed(int channel, float speed);

private:
    u::vector<float> m_scratch;
    size_t m_scratchNeeded;
    u::vector<instance *> m_channels;
    int m_sampleRate;
    int m_bufferSize;
    int m_flags;
    float m_globalVolume;
    float m_postClipScaler;
    unsigned int m_playIndex;
    fader m_globalVolumeFader;
    float m_streamTime;
    int m_sourceID;

public:
    void *m_mutex;
    float *m_mixerData;
};

void init(a::audio *system, int channels = 32, int flags = audio::kClipRoundOff, int sampleRate = 44100, int bufferSize = 2048);
void stop(a::audio *system);

}

#endif
