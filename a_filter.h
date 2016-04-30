#ifndef A_FILTER_HDR
#define A_FILTER_HDR
#include "u_vector.h"
#include "m_vec.h"
#include "a_fader.h"

namespace a {

struct source;

struct filterInstance {
    virtual void filter(float *buffer, size_t samples, bool strero, float sampleRate, float time) = 0;

    virtual void setFilterParam(int attrib, float value);
    virtual void fadeFilterParam(int attrib, float from, float to, float time, float startTime);
    virtual void oscFilterParam(int attrib, float from, float to, float time, float startTime);

    virtual ~filterInstance();
};

struct filter {
    virtual filterInstance *create() = 0;
    virtual ~filter();
};

struct echoFilter;

struct echoFilterInstance : filterInstance {
    virtual void filter(float *buffer, size_t samples, bool stereo, float sampleRate, float streamTime);
    virtual ~echoFilterInstance();
    echoFilterInstance(echoFilter *parent);

private:
    u::vector<float> m_buffer;
    echoFilter *m_parent;
    size_t m_offset;
};

struct echoFilter : filter {
    virtual filterInstance *create();
    echoFilter();
    void setParams(float delay, float decay);

private:
    friend struct echoFilterInstance;
    float m_delay;
    float m_decay;
};

struct BQRFilter;

struct BQRFilterInstance : filterInstance {
    BQRFilterInstance(BQRFilter *parent);

    virtual void filter(float *buffer, size_t samples, bool stereo, float sampleRate, float time);

    virtual void setFilterParam(int attrib, float value);
    virtual void fadeFilterParam(int attrib, float from, float to, float time, float startTime);
    virtual void oscFilterParam(int attrib, float from, float to, float time, float startTime);

    virtual ~BQRFilterInstance();

private:
    void calcParams();

    BQRFilter *m_parent;

    m::vec3 m_a;
    m::vec3 m_b;
    m::vec2 m_x1, m_x2;
    m::vec2 m_y1, m_y2;

    int m_filterType;

    float m_sampleRate;
    float m_frequency;
    float m_resonance;

    fader m_resonanceFader;
    fader m_frequencyFader;
    fader m_sampleRateFader;

    bool m_active;
    bool m_dirty;
};

struct BQRFilter : filter {
    // type
    enum {
        kNone,
        kLowPass,
        kHighPass,
        kBandPass
    };

    // attribute
    enum {
        kSampleRate,
        kFrequency,
        kResonance
    };

    virtual BQRFilterInstance *create();
    BQRFilter();
    void setParams(int type, float sampleRate, float frequency, float resonance);
    virtual ~BQRFilter();

private:
    friend struct BQRFilterInstance;
    int m_filterType;
    float m_sampleRate;
    float m_frequency;
    float m_resonance;
};

// check docs/AUDIO.md for explanation of how this does what it does
struct DCRemovalFilter;

struct DCRemovalFilterInstance : filterInstance {
    virtual void filter(float *buffer, size_t samples, bool stereo, float sampleRate, float streamTime);

    virtual ~DCRemovalFilterInstance();

    DCRemovalFilterInstance(DCRemovalFilter *parent);
private:
    u::vector<float> m_buffer;
    u::vector<float> m_totals;
    size_t m_offset;
    DCRemovalFilter *m_parent;
};

struct DCRemovalFilter : filter {
    virtual filterInstance *create();
    DCRemovalFilter();
    void setParams(float length = 0.1f);

private:
    friend struct DCRemovalFilterInstance;
    float m_length;
};

}

#endif
