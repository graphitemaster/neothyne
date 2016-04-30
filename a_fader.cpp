#include "a_fader.h"
#include "m_const.h"
#include "m_trig.h"

namespace a {

fader::fader()
    : m_current(0.0f)
    , m_from(0.0f)
    , m_to(0.0f)
    , m_delta(0.0f)
    , m_time(0.0f)
    , m_startTime(0.0f)
    , m_endTime(0.0f)
    , m_active(0)
{
}

void fader::set(int type, float from, float to, float time, float startTime) {
    switch (type) {
    case kLERP:
        m_current = from;
        m_from = from;
        m_to = to;
        m_time = time;
        m_startTime = startTime;
        m_delta = to - from;
        m_endTime = m_startTime + time;
        m_active = 1;
        break;
    case kLFO:
        m_active = 2;
        m_current = 0.0f;
        m_from = from;
        m_to = to;
        m_time = time;
        m_delta = m::abs(to - from) / 2.0f;
        m_startTime = startTime;
        m_endTime = m::kPi * 2.0f / (m_time * 1000.0f);
        break;
    }
}

float fader::operator()(float currentTime) {
    if (m_active == 2) {
        // LFO
        if (m_startTime > currentTime) {
            // time rolled over
            m_startTime = currentTime;
        }
        const float delta = currentTime - m_startTime;
        return m::sin(delta * m_endTime) * m_delta + (m_from + m_delta);
    }

    if (m_startTime > currentTime) {
        // time rolled over
        float delta = (m_current - m_from) / m_delta;
        m_from = m_current;
        m_startTime = currentTime;
        m_time = m_time * (1.0f - delta); // time left
        m_delta = m_to - m_from;
        m_endTime = m_startTime + m_time;
    }

    if (currentTime > m_endTime) {
        m_active = -1;
        return m_to;
    }
    m_current = m_from + m_delta * ((currentTime - m_startTime) / m_time);
    return m_current;
}

}
