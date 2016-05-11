#include "a_lane.h"

namespace a {

///! laneInstance
laneInstance::laneInstance(lane *parent)
    : m_parent(parent)
{
    m_flags |= kProtected;
}

void laneInstance::getAudio(float *buffer, size_t samples) {
    int handle = m_parent->m_channelHandle;
    if (handle == 0)
        return;

    audio *owner = m_parent->m_owner;
    if (owner->m_scratchNeeded != m_scratch.size())
        m_scratch.resize(owner->m_scratchNeeded);

    owner->mixLane(buffer, samples, &m_scratch[0], handle);
}

bool laneInstance::hasEnded() const {
    return false;
}

laneInstance::~laneInstance() {
    audio *owner = m_parent->m_owner;
    for (size_t i = 0; i < owner->m_voices.size(); i++) {
        if (owner->m_voices[i] && owner->m_voices[i]->m_laneHandle == m_parent->m_channelHandle)
            owner->stopVoice(i);
    }
}

///! lane
lane::lane()
    : m_channelHandle(0)
    , m_instance(nullptr)
{
    m_channels = 2;
}

laneInstance *lane::create() {
    if (m_channelHandle) {
        m_owner->stopVoice(m_owner->getVoiceFromHandle(m_channelHandle));
        m_channelHandle = 0;
        m_instance = nullptr;
    }
    return m_instance = new laneInstance(this);
}

int lane::play(source &sound, float volume, float pan, bool paused) {
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

}
