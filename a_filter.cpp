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

}
