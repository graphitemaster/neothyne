#include "a_filter.h"
#include "a_system.h"

#include "m_trig.h"

#include "u_misc.h"

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
BQRFilterInstance::BQRFilterInstance(BQRFilter *parent)
    : m_parent(parent)
    , m_filterType(m_parent->m_filterType)
    , m_sampleRate(m_parent->m_sampleRate)
    , m_frequency(m_parent->m_frequency)
    , m_resonance(m_parent->m_resonance)
    , m_active(false)
{
    calcParams();
}

void BQRFilterInstance::filter(float *buffer, size_t samples, bool stereo, float) {
    if (!m_active) return;
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

void BQRFilterInstance::calcParams() {
    const m::vec2 omega = m::sincos((2.0f * m::kPi * m_frequency) / m_sampleRate);
    const float alpha = omega.x / (2.0f * m_resonance);
    const float scalar = 1.0f / (1.0f + alpha);
    switch (m_filterType) {
    case BQRFilter::kNone:
        m_active = false;
        break;
    case BQRFilter::kLowPass:
        m_a.x = 0.5f * (1.0f - omega.y) * scalar;
        m_a.y = (1.0f - omega.y) * scalar;
        m_a.z = m_a.x;
        m_b.x = -2.0f * omega.y * scalar;
        m_b.y = (1.0f - alpha) * scalar;
        m_active = true;
        break;
    case BQRFilter::kHighPass:
        m_a.x = 0.5f * (1.0f + omega.y) * scalar;
        m_a.y = -(1.0f + omega.y) * scalar;
        m_a.z = m_a.x;
        m_b.x = -2.0f * omega.y * scalar;
        m_b.y = (1.0f - alpha) * scalar;
        m_active = true;
        break;
    case BQRFilter::kBandPass:
        m_a.x = alpha * scalar;
        m_a.y = 0.0f;
        m_a.z = -m_a.x;
        m_b.x = -2.0f * omega.y * scalar;
        m_b.y = (1.0f - alpha) * scalar;
        m_active = true;
        break;
    }
}

void BQRFilter::init(source *) {
    // Empty
}

BQRFilter::BQRFilter()
{
    // Empty
}

void BQRFilter::setParams(int type, float sampleRate, float frequency, float resonance) {
    m_filterType = type;
    m_sampleRate = sampleRate;
    m_frequency = frequency;
    m_resonance = resonance;
}

BQRFilter::~BQRFilter() {
    // Empty
}

BQRFilterInstance *BQRFilter::create() {
    return new BQRFilterInstance(this);
}

///! DCRemovalFilterInstance
DCRemovalFilterInstance::DCRemovalFilterInstance(DCRemovalFilter *parent)
    : m_offset(0)
    , m_parent(parent)
{
}

void DCRemovalFilterInstance::filter(float *buffer, size_t samples, bool stereo, float sampleRate) {
    if (m_buffer.empty()) {
        m_buffer.resize(size_t(m::ceil(m_parent->m_length * sampleRate)) * (stereo ? 2 : 1));
        m_totals.resize(stereo ? 2 : 1);
    }
    size_t bufferLength = m_buffer.size() / (stereo ? 2 : 1);
    for (size_t i = 0; i < samples; i++) {
        for (size_t j = 0; j < (stereo ? 2 : 1); j++) {
            const int c = j * bufferLength;
            const int b = j * samples;
            float n = buffer[i + b];
            m_totals[j] -= m_buffer[m_offset + c];
            m_totals[j] += n;
            m_buffer[m_offset + c] = n;
            n -= m_totals[j] / bufferLength;
            buffer[i + b] += (n - buffer[i + b]) * m_parent->m_length;
        }
        m_offset = (m_offset + 1) % bufferLength;
    }
}

DCRemovalFilterInstance::~DCRemovalFilterInstance() {
    // Empty
}

DCRemovalFilter::DCRemovalFilter()
    : m_length(0.1f)
{
}

void DCRemovalFilter::setParams(float length) {
    m_length = length;
}

filterInstance *DCRemovalFilter::create() {
    return new DCRemovalFilterInstance(this);
}

}
