#include "a_filter.h"
#include "a_lane.h"

#include "u_assert.h"

namespace a {

///! LaneInstance
LaneInstance::LaneInstance(Lane *parent)
    : m_parent(parent)
{
    m_flags |= kProtected;
}

void LaneInstance::getAudio(float *buffer, size_t samples) {
    int handle = m_parent->m_channelHandle;
    if (handle == 0)
        return;

    Audio *owner = m_parent->m_owner;
    if (owner->m_scratchNeeded != m_scratch.size())
        m_scratch.resize(owner->m_scratchNeeded);

    owner->mixLane(buffer, samples, &m_scratch[0], handle);
}

bool LaneInstance::hasEnded() const {
    return false;
}

LaneInstance::~LaneInstance() {
    Audio *owner = m_parent->m_owner;
    for (size_t i = 0; i < owner->m_voices.size(); i++) {
        if (owner->m_voices[i] && owner->m_voices[i]->m_laneHandle == m_parent->m_channelHandle)
            owner->stopVoice(i);
    }
}

///! Lane
Lane::Lane()
    : m_channelHandle(0)
    , m_instance(nullptr)
{
    m_channels = 2;
}

LaneInstance *Lane::create() {
    if (m_channelHandle) {
        int voice = m_owner->getVoiceFromHandle(m_channelHandle);
        U_ASSERT(voice != -1);
        m_owner->stopVoice(voice);
        m_channelHandle = 0;
        m_instance = nullptr;
    }
    return m_instance = new LaneInstance(this);
}

int Lane::play(Source &sound, float volume, float pan, bool paused) {
    if (!m_instance || !m_owner)
        return 0;

    if (m_channelHandle == 0) {
        // find channel the lane is playing on
        for (size_t i = 0; m_channelHandle == 0 && i < m_owner->m_voices.size(); i++)
            if (m_owner->m_voices[i] == m_instance)
                m_channelHandle = m_owner->getHandleFromVoice(i);
        // could not find channel
        if (m_channelHandle == 0)
            return 0;
    }
    return m_owner->play(sound, volume, pan, paused, m_channelHandle);
}

void Lane::setFilter(int filterHandle, Filter *filter) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    m_filters[filterHandle] = filter;
    if (m_instance) locked(m_owner->m_mutex) {
        delete m_instance->m_filters[filterHandle];
        m_instance->m_filters[filterHandle] = nullptr;
        if (filter)
            m_instance->m_filters[filterHandle] = m_filters[filterHandle]->create();
    }
}

}
