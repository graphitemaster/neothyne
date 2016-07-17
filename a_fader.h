#ifndef A_FADER_HDR
#define A_FADER_HDR

namespace a {

struct Fader {
    Fader();

    void lerp(float from, float to, float time, float startTime);
    void lfo(float from, float to, float time, float startTime);

    float operator()(float time);

private:
    friend struct audio;
    friend struct Filter;

    float m_current;
    float m_from;
    float m_to;
    float m_delta;
    float m_time;
    float m_startTime;
    float m_endTime;

public:
    // 0: disabled, 1: active, 2: LFO, -1: was active but stopped recently
    int m_active;
};

}

#endif
