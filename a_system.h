#ifndef A_SYSTEM_HDR
#define A_SYSTEM_HDR
#include "m_vec.h"
#include "u_vector.h"

namespace a {

struct audio;

void audioInit(a::audio *system);
void audioShutdown(a::audio *system);

struct producer {
    enum {
        kLooping = 1 << 0,
        kStereo = 1 << 1,
        kProtected = 1 << 2,
        kPaused = 1 << 3
    };

    virtual ~producer() {};

protected:
    virtual void getAudio(float *buffer, size_t samples) = 0;
    virtual bool hasEnded() const = 0;

    friend struct audio;

    unsigned int m_playIndex;
    int m_flags;
    m::vec3 m_volume; // left, right, global scale
    float m_baseSampleRate;
    float m_sampleRate;
    float m_relativePlaySpeed;
};

struct factory {
    enum {
        kLoop = 1 << 0,
        kStereo = 1 << 1
    };

    factory();

    void setLooping(bool looping);

    virtual ~factory() {}
    virtual producer *create() = 0;

protected:
    friend struct audio;

    int m_flags;
    float m_baseSampleRate;
};

inline factory::factory()
    : m_flags(0)
    , m_baseSampleRate(44100.0f)
{
}

struct audio {
    audio();
    ~audio();

    enum {
        kClipRoundOff = 1 << 0
    };

    void init(int channels, int sampleRate, int bufferSize, int flags);
    void setVolume(float volume);

    int play(factory &sound, float volume, float pan, bool paused = false);

    float getVolume(int channel) const;
    float getSampleRate(int channel) const;
    float getPostClipScaler() const;
    float getRelativePlaySpeed(int channel) const;
    bool getProtected(int channel) const;
    bool getPaused(int channel) const;

    void setRelativePlaySpeed(int channel, float speed);
    void setPostClipScaler(float scaler);
    void setSampleRate(int channel, float sampleRate);
    void setProtected(int channel, bool protect);
    void setPaused(int channel, bool paused);

    void setPan(int channel, float pan);
    void setVolume(int channel, float volume);

    void stop(int channel);
    void stop();

    void mix(float *buffer, int samples);

    float *mixerData;

protected:
    int findFreeChannel();
    int absChannel(int handle) const;

    void setPanAbs(int channel, const m::vec2 &volume);
    void stopAbs(int channel);

private:
    u::vector<float> m_scratch;
    size_t m_scratchNeeded;
    u::vector<producer *> m_channels;
    int m_sampleRate;
    int m_bufferSize;
    int m_flags;
    float m_globalVolume;
    float m_postClipScaler;
    unsigned int m_playIndex;
};

}

#endif
