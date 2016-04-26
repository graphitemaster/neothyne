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
        kProtected = 1 << 0,
        kLooping = 1 << 1
    };

    virtual ~producer() {};

protected:
    virtual void getAudio(float *buffer, size_t samples) = 0;
    virtual bool hasEnded() const = 0;

    friend struct audio;

    unsigned int m_playIndex;
    int m_flags;
    m::vec3 m_volume; // left, right, global scale
    float m_sampleRate;
};

struct factory {
    enum {
        kLoop = 1 << 0
    };

    factory();

    void setLooping(bool looping);

    virtual ~factory() {}
    virtual producer *create() = 0;

private:
    friend struct audio;

    int m_flags;
};

inline factory::factory()
    : m_flags(0)
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

    int play(factory &sound, float volume, float pan);

    float getVolume(int channel) const;
    float getSampleRate(int channel) const;
    bool getProtected(int channel) const;

    void setSampleRate(int channel, float sampleRate);
    void setProtected(int channel, bool protect);

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
    unsigned int m_playIndex;
};

}

#endif
