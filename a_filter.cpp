#include "a_filter.h"
#include "a_system.h"

#include "m_trig.h"

#include "u_misc.h"

#include <stdio.h>

namespace a {

///! filter
void filter::init(source *) {
    // Empty
}

filter::~filter() {
    // Empty
}

///! filterInstance
filterInstance::~filterInstance() {
    // Empty
}

///! echoFilter
echoFilterInstance::echoFilterInstance(echoFilter *parent)
    : m_parent(parent)
    , m_offset(0)
{
}

void echoFilterInstance::filter(float *buffer, size_t samples, bool stereo, float sampleRate) {
    if (m_buffer.empty()) {
        const size_t length = m::ceil(m_parent->m_delay * sampleRate) * (stereo ? 2 : 1);
        m_buffer.resize(length);
    }
    const size_t process = samples * (stereo ? 2 : 1);
    const float decay = m_parent->m_decay;
    for (size_t i = 0; i < process; i++) {
        const float sample = buffer[i] + m_buffer[m_offset] * decay;
        m_buffer[m_offset] = sample;
        buffer[i] = sample;
        m_offset = (m_offset + 1) % m_buffer.size();
    }
}

echoFilterInstance::~echoFilterInstance() {
    // Empty
}

void echoFilter::init(source *) {
    // Empty
}

echoFilter::echoFilter()
    : m_delay(1.0f)
    , m_decay(0.5f)
{
}

void echoFilter::setParams(float delay, float decay) {
    m_delay = delay;
    m_decay = decay;
}

filterInstance *echoFilter::create() {
    return new echoFilterInstance(this);
}

///! BQRFilterInstance
static void calcBQRParams(int type,
                          float sampleRate,
                          float frequency,
                          float resonance,
                          m::vec3 &a,
                          m::vec2 &b)
{
    const m::vec2 omega = m::sincos((2.0f * m::kPi * frequency) / sampleRate);
    const float alpha = omega.x / (2.0f * resonance);
    const float scalar = 1.0f / (1.0f + alpha);

    switch (type) {
    case BQRFilter::kLowPass:
        a.x = 0.5f * (1.0f - omega.y) * scalar;
        a.y = (1.0f - omega.y) * scalar;
        a.z = a.x;
        b = { -2.0f * omega.y * scalar, (1.0f - alpha) * scalar };
        break;
    case BQRFilter::kHighPass:
        a.x = 0.5f * (1.0f + omega.y) * scalar;
        a.y = -(1.0f + omega.y) * scalar;
        a.z = a.x;
        b = { -2.0f * omega.y * scalar, (1.0f - alpha) * scalar };
        break;
    case BQRFilter::kBandPass:
        a.x = alpha * scalar;
        a.y = 0.0f;
        a.z = -a.x;
        b = { -2.0f * omega.y * scalar, (1.0f - alpha) * scalar };
        break;
    }
}

BQRFilterInstance::BQRFilterInstance(BQRFilter *parent)
    : m_parent(parent)
    , m_a(m_parent->m_a)
    , m_b(m_parent->m_b)
    , m_active(m_parent->m_active)
{
}

void BQRFilterInstance::filter(float *buffer, size_t samples, bool stereo, float) {
    if (!m_active)
        return;
    size_t pitch = stereo ? 2 : 1;
    for (size_t s = 0; s < pitch; s++) {
        for (size_t i = 0; i < samples; i += 2) {
            // filter the inputs
            float x = buffer[i * pitch + s];
            m_y2[s] = (m_a.x * x) + (m_a.y * m_x1[s]) + (m_a.z * m_x2[s])
                                  - (m_b.x * m_y1[s]) - (m_b.y * m_y2[s]);
            buffer[i * pitch + s] = m_y2[s];
            // permute filter ops to reduce movement
            m_x2[s] = buffer[(i + 1) * pitch + s];
            m_y1[s] = (m_a.y * x) + (m_a.x * m_x2[s]) + (m_a.z * m_x1[s])
                                  - (m_b.x * m_y2[s]) - (m_b.y * m_y1[s]);
            // move it a little
            m_x1[s] = m_x2[s];
            m_x2[s] = x;
        }
        // apply a very small impulse to prevent underflow
        m_y1[s] += 1.0e-26f;
    }
}

BQRFilterInstance::~BQRFilterInstance() {
    // Empty
}

void BQRFilter::init(source *) {
    // Empty
}

BQRFilter::BQRFilter()
    : m_active(false)
{
    // Empty
}

void BQRFilter::setParams(int type, float sampleRate, float frequency, float resonance) {
    calcBQRParams(type, sampleRate, frequency, resonance, m_a, m_b);
    m_active = true;
}

BQRFilter::~BQRFilter() {
    // Empty
}

BQRFilterInstance *BQRFilter::create() {
    return new BQRFilterInstance(this);
}

}
