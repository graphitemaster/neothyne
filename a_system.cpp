#include <SDL.h>

#include "a_system.h"
#include "a_filter.h"

#include "m_const.h"
#include "m_trig.h"

#include "u_new.h"
#include "u_misc.h"
#include "u_memory.h"

#include "engine.h"

namespace a {

struct lockGuard {
    lockGuard(void *opaque)
        : m_mutex((SDL_mutex *)opaque)
    {
        SDL_LockMutex(m_mutex);
    }

    ~lockGuard() {
        if (m_mutex)
            SDL_UnlockMutex(m_mutex);
    }

    void unlock() {
        SDL_UnlockMutex(m_mutex);
        m_mutex = nullptr;
    }

    operator bool() const {
        return m_mutex;
    }

private:
    SDL_mutex *m_mutex;
};

// Clever technique to make a scope that locks a mutex
#define locked(X) \
    for (lockGuard lock(X); lock; lock.unlock())

static void audioMixer(void *user, Uint8 *stream, int length) {
    const int samples = length / 4;
    short *buffer = (short *)stream;
    a::audio *system = (a::audio *)user;
    float *data = (float *)system->m_mixerData;
    system->mix(data, samples);
    for (int i = 0; i < samples*2; i++)
        buffer[i] = (short)m::floor(data[i] * 0x7fff);
}

void init(a::audio *system, int channels, int flags, int sampleRate, int bufferSize) {
    system->m_mutex = (void *)SDL_CreateMutex();

    SDL_AudioSpec spec;
    spec.freq = sampleRate;
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = bufferSize;
    spec.callback = &audioMixer;
    spec.userdata = (void *)system;

    SDL_AudioSpec got;
    if (SDL_OpenAudio(&spec, &got) < 0) {
        SDL_DestroyMutex((SDL_mutex *)system->m_mutex);
        neoFatalError("failed to initialize audio");
    }

    system->m_mixerData = neoMalloc(sizeof(float) * got.samples*4);
    system->init(channels, got.freq, got.samples * 2, flags);

    u::print("[audio] => device configured for %d channels @ %dHz (%d samples)\n", got.channels, got.freq, got.samples);
    SDL_PauseAudio(0);
}

void stop(a::audio *system) {
    SDL_CloseAudio();
    // free the mixer data after shutting down the mixer thread
    neoFree(system->m_mixerData);
    // destroy the mutex after shutting down the mixer thread
    SDL_DestroyMutex((SDL_mutex*)system->m_mutex);
}

///! instance
instance::instance()
    : m_playIndex(0)
    , m_flags(0)
    , m_baseSampleRate(44100.0f)
    , m_sampleRate(44100.0f)
    , m_relativePlaySpeed(1.0f)
    , m_streamTime(0.0f)
    , m_sourceID(0)
    , m_activeFader(0)
{
    m_volume.x = 1.0f / m::sqrt(2.0f);
    m_volume.y = 1.0f / m::sqrt(2.0f);
    m_volume.z = 1.0f;
    memset(m_faderVolume, 0, sizeof m_faderVolume);
    memset(m_filters, 0, sizeof m_filters);
}

instance::~instance() {
    for (auto *it : m_filters)
        delete it;
}

void instance::init(size_t playIndex, float baseSampleRate, int sourceFlags) {
    m_playIndex = playIndex;
    m_baseSampleRate = baseSampleRate;
    m_sampleRate = m_baseSampleRate;
    m_streamTime = 0.0f;
    m_flags = 0;
    if (sourceFlags & source::kLoop)
        m_flags |= instance::kLooping;
    if (sourceFlags & source::kStereo)
        m_flags |= instance::kStereo;
}

bool instance::rewind() {
    // TODO
    return false;
}

void instance::seek(float seconds, float *scratch, size_t scratchSize) {
    float offset = seconds - m_streamTime;
    if (offset < 0.0f) {
        if (!rewind()) {
            // cannot do a seek backwards unless we can rewind
            return;
        }
        offset = seconds;
    }
    for (size_t discard = size_t(m::floor(m_sampleRate * offset)); discard; ) {
        const size_t samples = u::min(scratchSize >> 1, discard);
        getAudio(scratch, samples);
        discard -= samples;
    }
    m_streamTime = seconds;
}

///! source
source::source()
    : m_flags(0)
    , m_baseSampleRate(44100.0f)
    , m_sourceID(0)
    , m_owner(nullptr)
{
    memset(m_filters, 0, sizeof m_filters);
}

source::~source() {
    if (m_owner)
        m_owner->stopSound(*this);
}

void source::setLooping(bool looping) {
    if (looping)
        m_flags |= kLoop;
    else
        m_flags &= ~kLoop;
}

void source::setFilter(int filterHandle, filter *filter_) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    m_filters[filterHandle] = filter_;
}

///! audio
audio::audio()
    : m_scratchNeeded(0)
    , m_sampleRate(0)
    , m_bufferSize(0)
    , m_flags(0)
    , m_globalVolume(0)
    , m_postClipScaler(0.95f)
    , m_playIndex(0)
    , m_streamTime(0)
    , m_sourceID(1)
    , m_mutex(nullptr)
    , m_mixerData(nullptr)
{
    memset(m_filters, 0, sizeof m_filters);
    memset(m_filterInstances, 0, sizeof m_filterInstances);
}

audio::~audio() {
    stopAll();
    for (auto *it : m_filterInstances)
        delete it;
}

void audio::init(int channels, int sampleRate, int bufferSize, int flags) {
    m_globalVolume = 1.0f;
    m_channels.resize(channels);
    m_sampleRate = sampleRate;
    m_scratchNeeded = 2048;
    m_scratch.resize(2048);
    m_bufferSize = bufferSize;
    m_flags = flags;
    m_postClipScaler = 0.95f;
    u::print("[audio] => initialized for %d channels @ %dHz with %s buffer\n",
        channels, sampleRate, u::sizeMetric(bufferSize));
}

float audio::getPostClipScaler() const {
    lockGuard lock(m_mutex);
    return m_postClipScaler;
}

void audio::setPostClipScaler(float scaler) {
    lockGuard lock(m_mutex);
    m_postClipScaler = scaler;
}

void audio::setGlobalVolume(float volume) {
    lockGuard lock(m_mutex);
    m_globalVolumeFader.m_active = 0;
    m_globalVolume = volume;
}

int audio::findFreeChannel() {
    size_t slat = 0xFFFFFFFF;
    int next = -1;
    for (size_t i = 0; i < m_channels.size(); i++) {
        if (!m_channels[i])
            return i;
        if (!(m_channels[i]->m_flags & instance::kProtected) && m_channels[i]->m_playIndex < slat) {
            slat = m_channels[i]->m_playIndex;
            next = int(i);
        }
    }
    stopChannel(next);
    return next;
}

int audio::play(source &sound, float volume, float pan, bool paused) {
    u::unique_ptr<instance> instance(sound.create());
    sound.m_owner = this;

    locked(m_mutex) {
        const int channel = findFreeChannel();
        if (channel < 0)
            return -1;

        if (sound.m_sourceID == 0)
            sound.m_sourceID = m_sourceID++;
        m_channels[channel] = instance.release();
        m_channels[channel]->m_sourceID = sound.m_sourceID;
        int handle = channel | (m_playIndex << 12);
        m_channels[channel]->init(m_playIndex, sound.m_baseSampleRate, sound.m_flags);
        if (paused)
            m_channels[channel]->m_flags |= instance::kPaused;

        setChannelPan(channel, pan);
        setChannelVolume(channel, volume);
        setChannelRelativePlaySpeed(channel, 1.0f);

        for (size_t i = 0; i < kMaxStreamFilters; i++)
            if (sound.m_filters[i])
                m_channels[channel]->m_filters[i] = sound.m_filters[i]->create();

        m_playIndex++;
        const size_t scratchNeeded = size_t(m::ceil((m_channels[channel]->m_sampleRate / m_sampleRate) * m_bufferSize));
        if (m_scratchNeeded < scratchNeeded) {
            // calculate 1024 byte chunks needed for the scratch space
            size_t next = 1024;
            while (next < scratchNeeded) next <<= 1;
            m_scratchNeeded = next;
        }
        return handle;
    }

    return -1;
}

int audio::getChannelFromHandle(int handle) const {
    if (handle < 0)
        return -1;
    const int channel = handle & 0xFF;
    const size_t index = handle >> 12;
    if (m_channels[index] && (m_channels[index]->m_playIndex & 0xFFFFF) == index)
        return channel;
    return -1;
}

float audio::getVolume(int channelHandle) const {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    return channel == -1 ? 0.0f : m_channels[channel]->m_volume.z;
}

float audio::getStreamTime(int channelHandle) const {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    return channel == -1 ? 0.0f : m_channels[channel]->m_streamTime;
}

float audio::getRelativePlaySpeed(int channelHandle) const {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    return channel == -1 ? 1.0f : m_channels[channel]->m_relativePlaySpeed;
}

void audio::setChannelRelativePlaySpeed(int channel, float speed) {
    if (m_channels[channel]) {
        m_channels[channel]->m_relativePlaySpeed = speed;
        m_channels[channel]->m_sampleRate = m_channels[channel]->m_baseSampleRate * speed;
        const size_t scratchNeeded = size_t(m::ceil((m_channels[channel]->m_sampleRate / m_sampleRate) * m_bufferSize));
        if (m_scratchNeeded < scratchNeeded) {
            // calculate 1024 byte chunks needed for the scratch space
            size_t next = 1024;
            while (next < scratchNeeded) next <<= 1;
            m_scratchNeeded = next;
        }
    }
}

void audio::setRelativePlaySpeed(int channelHandle, float speed) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    m_channels[channel]->m_relativePlaySpeedFader.m_active = 0;
    setChannelRelativePlaySpeed(channel, speed);
}

float audio::getSampleRate(int channelHandle) const {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    return channel == -1 ? 0.0f : m_channels[channel]->m_baseSampleRate;
}

void audio::setSampleRate(int channelHandle, float sampleRate) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    m_channels[channel]->m_baseSampleRate = sampleRate;
    m_channels[channel]->m_sampleRate = sampleRate * m_channels[channel]->m_relativePlaySpeed;
}

void audio::seek(int channelHandle, float seconds) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    m_channels[channel]->seek(seconds, &m_scratch[0], m_scratch.size());
}

void audio::setPaused(int channelHandle, bool paused) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    setChannelPaused(channel, paused);
}

void audio::setPausedAll(bool paused) {
    lockGuard lock(m_mutex);
    for (size_t i = 0; i < m_channels.size(); i++)
        setChannelPaused(i, paused);
}

bool audio::getPaused(int channelHandle) const {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    return channel == -1 ? false : !!(m_channels[channel]->m_flags & instance::kPaused);
}

bool audio::getProtected(int channelHandle) const {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    return channel == -1 ? false : !!(m_channels[channel]->m_flags & instance::kProtected);
}

void audio::setProtected(int channelHandle, bool protect) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    if (protect)
        m_channels[channel]->m_flags |= instance::kProtected;
    else
        m_channels[channel]->m_flags &= ~instance::kProtected;
}

void audio::setChannelPaused(int channel, bool paused) {
    if (m_channels[channel]) {
        m_channels[channel]->m_pauseScheduler.m_active = 0;
        if (paused)
            m_channels[channel]->m_flags |= instance::kPaused;
        else
            m_channels[channel]->m_flags &= ~instance::kPaused;
    }
}

#define FadeSetRelative(FADER, TYPE, FROM, TO, TIME) \
    (m_channels[channel]->FADER.set(fader::TYPE, (FROM), (TO), (TIME), m_channels[channel]->m_streamTime))

void audio::schedulePause(int channelHandle, float time) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    FadeSetRelative(m_pauseScheduler, kLERP, 1.0f, 0.0f, time);
}

void audio::scheduleStop(int channelHandle, float time) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    FadeSetRelative(m_stopScheduler, kLERP, 1.0f, 0.0f, time);
}

void audio::setChannelPan(int channel, float pan) {
    if (m_channels[channel]) {
        const m::vec2 panning = m::sincos((pan + 1.0f) * m::kPi / 4.0f);
        m_channels[channel]->m_volume.x = panning.x;
        m_channels[channel]->m_volume.y = panning.y;
    }
}

void audio::setPan(int channelHandle, float pan) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    setChannelPan(channel, pan);
}

void audio::setPanAbsolute(int channelHandle, const m::vec2 &panning) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    m_channels[channel]->m_panFader.m_active = 0;
    m_channels[channel]->m_volume.x = panning.x;
    m_channels[channel]->m_volume.y = panning.y;
}

void audio::setChannelVolume(int channel, float volume) {
    if (m_channels[channel])
        m_channels[channel]->m_volume.z = volume;
}

void audio::setVolume(int channelHandle, float volume) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    m_channels[channel]->m_volumeFader.m_active = 0;
    setChannelVolume(channel, volume);
}

void audio::setGlobalFilter(int filterHandle, filter *filter_) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;

    locked(m_mutex) {
        delete m_filterInstances[filterHandle];
        m_filterInstances[filterHandle] = nullptr;

        m_filters[filterHandle] = filter_;
        if (filter_)
            m_filterInstances[filterHandle] = m_filters[filterHandle]->create();
    }
}

void audio::stop(int channelHandle) {
    lockGuard lock(m_mutex);
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;
    stopChannel(channel);
}

void audio::stopChannel(int channel) {
    if (m_channels[channel]) {
        delete m_channels[channel];
        m_channels[channel] = nullptr;
    }
}

void audio::stopSound(source &sound) {
    lockGuard lock(m_mutex);
    if (sound.m_sourceID) {
        for (size_t i = 0; i < m_channels.size(); i++)
            if (m_channels[i] && m_channels[i]->m_sourceID == sound.m_sourceID)
                stopChannel(i);
    }
}

void audio::setFilterParam(int channelHandle, int filterHandle, int attrib, float value) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (channelHandle == 0) {
        lockGuard lock(m_mutex);
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->setFilterParam(attrib, value);
        return;
    }
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;

    locked(m_mutex) {
        if (m_channels[channel] && m_channels[channel]->m_filters[filterHandle])
            m_channels[channel]->m_filters[filterHandle]->setFilterParam(attrib, value);
    }
}

void audio::fadeFilterParam(int channelHandle,
                            int filterHandle,
                            int attrib,
                            float from,
                            float to,
                            float time)
{
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (channelHandle == 0) {
        lockGuard lock(m_mutex);
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->fadeFilterParam(attrib, from, to, time, m_streamTime);
        return;
    }
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;

    locked(m_mutex) {
        if (m_channels[channel] && m_channels[channel]->m_filters[filterHandle])
            m_channels[channel]->m_filters[filterHandle]->fadeFilterParam(attrib, from, to, time, m_streamTime);
    }
}

void audio::oscFilterParam(int channelHandle,
                           int filterHandle,
                           int attrib,
                           float from,
                           float to,
                           float time)
{
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (channelHandle == 0) {
        lockGuard lock(m_mutex);
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->oscFilterParam(attrib, from, to, time, m_streamTime);
        return;
    }
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1)
        return;

    locked(m_mutex) {
        if (m_channels[channel] && m_channels[channel]->m_filters[filterHandle])
            m_channels[channel]->m_filters[filterHandle]->oscFilterParam(attrib, from, to, time, m_streamTime);
    }
}

void audio::stopAll() {
    lockGuard lock(m_mutex);
    for (size_t i = 0; i < m_channels.size(); i++)
        stopChannel(i);
}

void audio::fadeVolume(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(channelHandle, to);
        return;
    } else locked(m_mutex) {
        const int channel = getChannelFromHandle(channelHandle);
        if (channel == -1)
            return;
        FadeSetRelative(m_volumeFader, kLERP, from, to, time);
    }
}

void audio::fadePan(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(channelHandle, to);
        return;
    } else locked(m_mutex) {
        const int channel = getChannelFromHandle(channelHandle);
        if (channel == -1)
            return;
        FadeSetRelative(m_panFader, kLERP, from, to, time);
    }
}

void audio::fadeRelativePlaySpeed(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(channelHandle, to);
        return;
    } else locked(m_mutex) {
        const int channel = getChannelFromHandle(channelHandle);
        if (channel == -1)
            return;
        FadeSetRelative(m_relativePlaySpeedFader, kLERP, from, to, time);
    }
}

void audio::fadeGlobalVolume(float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setGlobalVolume(to);
        return;
    } else {
        lockGuard lock(m_mutex);
        m_streamTime = 0.0f; // avoid ~6 day rollover
        m_globalVolumeFader.set(fader::kLERP, from, to, time, m_streamTime);
    }
}

void audio::oscVolume(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(channelHandle, to);
        return;
    } else locked(m_mutex) {
        const int channel = getChannelFromHandle(channelHandle);
        if (channel == -1)
            return;
        FadeSetRelative(m_volumeFader, kLFO, from, to, time);
    }
}

void audio::oscPan(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(channelHandle, to);
        return;
    } else locked(m_mutex) {
        const int channel = getChannelFromHandle(channelHandle);
        if (channel == -1)
            return;
        FadeSetRelative(m_panFader, kLFO, from, to, time);
    }
}

void audio::oscRelativePlaySpeed(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(channelHandle, to);
        return;
    } else locked(m_mutex) {
        const int channel = getChannelFromHandle(channelHandle);
        if (channel == -1)
            return;
        FadeSetRelative(m_relativePlaySpeedFader, kLFO, from, to, time);
    }
}

void audio::oscGlobalVolume(float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setGlobalVolume(to);
        return;
    } else locked(m_mutex) {
        m_streamTime = 0.0f; // avoid ~6 day rollover
        m_globalVolumeFader.set(fader::kLFO, from, to, time, m_streamTime);
    }
}

void audio::mix(float *buffer, size_t samples) {
    float bufferTime = samples / float(m_sampleRate);
    m_streamTime += bufferTime;

    // process global faders
    m::vec2 globalVolume;
    globalVolume.x = m_globalVolume;
    if (m_globalVolumeFader.m_active)
        m_globalVolume = m_globalVolumeFader(m_streamTime);
    globalVolume.y = m_globalVolume;

    // lock guard scope
    locked(m_mutex) {
        // process per-channel faders
        for (size_t i = 0; i < m_channels.size(); i++) {
            if (!m_channels[i] || (m_channels[i]->m_flags & instance::kPaused))
                continue;

            m_channels[i]->m_activeFader = 0;
            if (m_globalVolumeFader.m_active > 0)
                m_channels[i]->m_activeFader = 1;

            m_channels[i]->m_streamTime += bufferTime;

            if (m_channels[i]->m_relativePlaySpeedFader.m_active > 0) {
                const float speed = m_channels[i]->m_relativePlaySpeedFader(m_channels[i]->m_streamTime);
                setChannelRelativePlaySpeed(i, speed);
            }

            m::vec2 volume;
            volume.x = m_channels[i]->m_volume.z;
            if (m_channels[i]->m_volumeFader.m_active > 0) {
                m_channels[i]->m_volume.z = m_channels[i]->m_volumeFader(m_channels[i]->m_streamTime);
                m_channels[i]->m_activeFader = 1;
            }
            volume.y = m_channels[i]->m_volume.z;

            m::vec2 panL;
            m::vec2 panR;
            panL.x = m_channels[i]->m_volume.x;
            panR.x = m_channels[i]->m_volume.y;
            if (m_channels[i]->m_panFader.m_active > 0) {
                const float pan = m_channels[i]->m_panFader(m_channels[i]->m_streamTime);
                setChannelPan(i, pan);
                m_channels[i]->m_activeFader = 1;
            }
            panL.y = m_channels[i]->m_volume.x;
            panR.y = m_channels[i]->m_volume.y;

            if (m_channels[i]->m_pauseScheduler.m_active) {
                m_channels[i]->m_pauseScheduler(m_channels[i]->m_streamTime);
                if (m_channels[i]->m_pauseScheduler.m_active == -1) {
                    m_channels[i]->m_pauseScheduler.m_active = 0;
                    setChannelPaused(i, true);
                }
            }

            if (m_channels[i]->m_activeFader) {
                m_channels[i]->m_faderVolume[0*2+0] = panL.x * volume.x * globalVolume.x;
                m_channels[i]->m_faderVolume[0*2+1] = panL.y * volume.y * globalVolume.y;
                m_channels[i]->m_faderVolume[1*2+0] = panR.x * volume.x * globalVolume.x;
                m_channels[i]->m_faderVolume[1*2+1] = panR.y * volume.y * globalVolume.y;
            }

            if (m_channels[i]->m_stopScheduler.m_active) {
                m_channels[i]->m_stopScheduler(m_channels[i]->m_streamTime);
                if (m_channels[i]->m_stopScheduler.m_active == -1) {
                    m_channels[i]->m_stopScheduler.m_active = 0;
                    stopChannel(i);
                }
            }
        }

        // resize the scratch buffer if required
        if (m_scratch.size() < m_scratchNeeded)
            m_scratch.resize(m_scratchNeeded);

        // clear accumulation
        for (size_t i = 0; i < samples << 1; i++)
            buffer[i] = 0.0f;

        // accumulate active sound sources
        for (size_t i = 0; i < m_channels.size(); i++) {
            if (!m_channels[i] || (m_channels[i]->m_flags & instance::kPaused))
                continue;

            const float next = m_channels[i]->m_sampleRate / m_sampleRate;
            float step = 0.0f;

            const size_t readSamples = m::ceil(samples * next);

            // get the audio from the source into our scratch buffer
            if (m_channels[i]->hasEnded())
                memset(&m_scratch[0], 0, sizeof(float) * m_scratch.size());
            else
                m_channels[i]->getAudio(&m_scratch[0], readSamples);

            for (size_t j = 0; j < kMaxStreamFilters; j++) {
                if (m_channels[i]->m_filters[j]) {
                    m_channels[i]->m_filters[j]->filter(&m_scratch[0],
                                                        readSamples,
                                                        m_channels[i]->m_flags & instance::kStereo ? 2  : 1,
                                                        m_channels[i]->m_sampleRate,
                                                        m_streamTime);
                }
            }

            if (m_channels[i]->m_activeFader) {
                float panL = m_channels[i]->m_faderVolume[0];
                float panR = m_channels[i]->m_faderVolume[2];
                const float panLi = (m_channels[i]->m_faderVolume[1] - m_channels[i]->m_faderVolume[0]) / samples;
                const float panRi = (m_channels[i]->m_faderVolume[3] - m_channels[i]->m_faderVolume[2]) / samples;
                if (m_channels[i]->m_flags & instance::kStereo) {
                    for (size_t j = 0; j < samples; j++, step += next, panL += panLi, panR += panRi) {
                        const float sampleL = m_scratch[(int)m::floor(step)];
                        const float sampleR = m_scratch[(int)m::floor(step) + samples];
                        buffer[j] += sampleL * panL;
                        buffer[j + samples] += sampleR * panR;
                    }
                } else {
                    for (size_t j = 0; j < samples; j++, step += next, panL += panLi, panR += panRi) {
                        const float sampleM = m_scratch[(int)m::floor(step)];
                        buffer[j] += sampleM * panL;
                        buffer[j + samples] += sampleM * panR;
                    }
                }
            } else {
                const float panL = m_channels[i]->m_volume.x * m_channels[i]->m_volume.z * m_globalVolume;
                const float panR = m_channels[i]->m_volume.y * m_channels[i]->m_volume.z * m_globalVolume;
                if (m_channels[i]->m_flags & instance::kStereo) {
                    for (size_t j = 0; j < samples; j++, step += next) {
                        const float sampleL = m_scratch[(int)m::floor(step)];
                        const float sampleR = m_scratch[(int)m::floor(step) + samples];
                        buffer[j] += sampleL * panL;
                        buffer[j + samples] += sampleR * panR;
                    }
                } else {
                    for (size_t j = 0; j < samples; j++, step += next) {
                        const float sampleM = m_scratch[(int)m::floor(step)];
                        buffer[j] += sampleM * panL;
                        buffer[j + samples] += sampleM * panR;
                    }
                }
            }
            // clear the channel if the sound is complete
            if (!(m_channels[i]->m_flags & instance::kLooping) && m_channels[i]->hasEnded())
                stopChannel(i);
        }
        // global filter
        for (size_t i = 0; i < kMaxStreamFilters; i++) {
            if (m_filterInstances[i])
                m_filterInstances[i]->filter(&buffer[0], samples, 2, m_sampleRate, m_streamTime);
        }
    }

    clip(buffer, &m_scratch[0], samples);
    interlace(&m_scratch[0], buffer, samples, 2);
}

void audio::clip(const float *U_RESTRICT src, float *U_RESTRICT dst, size_t samples) {
    // clip
    if (m_flags & kClipRoundOff) {
        // round off clipping is less aggressive
        for (size_t i = 0; i < samples << 1; i++) {
            float sample = src[i];
            /**/ if (sample <= -1.65f) sample = -0.9862875f;
            else if (sample >=  1.65f) sample =  0.9862875f;
            else                       sample =  0.87f * sample - 0.1f * sample * sample * sample;
            dst[i] = sample * m_postClipScaler;
        }
    } else {
        // standard clipping may introduce noise and aliasing - need proper
        // hi-pass filter to prevent this
        for (size_t i = 0; i < samples; i++)
            dst[i] = m::clamp(src[i], -1.65f, 1.65f) * m_postClipScaler;
    }
}

void audio::interlace(const float *U_RESTRICT src,
                      float *U_RESTRICT dst,
                      size_t samples,
                      size_t channels)
{
    // converts deinterlaced audio samples from 111222 to 121212
    for (size_t j = 0, k = 0; j < channels; j++)
        for (size_t i = j; i < samples * channels; i += channels, k++)
            dst[i] = src[k];
}

}
