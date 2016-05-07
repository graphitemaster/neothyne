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
    , m_channels(1)
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

void instance::init(size_t playIndex, float baseSampleRate, size_t channels, int sourceFlags) {
    m_playIndex = playIndex;
    m_baseSampleRate = baseSampleRate;
    m_sampleRate = m_baseSampleRate;
    m_channels = channels;
    m_streamTime = 0.0f;
    m_flags = 0;
    if (sourceFlags & source::kLoop)
        m_flags |= instance::kLooping;
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
    , m_channels(1)
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

void audio::init(int voices, int sampleRate, int bufferSize, int flags) {
    m_globalVolume = 1.0f;
    m_voices.resize(voices);
    m_sampleRate = sampleRate;
    m_scratchNeeded = 2048;
    m_scratch.resize(2048);
    m_bufferSize = bufferSize;
    m_flags = flags;
    m_postClipScaler = 0.95f;
    u::print("[audio] => initialized for %d voices @ %dHz with %s buffer\n",
        voices, sampleRate, u::sizeMetric(bufferSize));
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

int audio::findFreeVoice() {
    size_t slat = 0xFFFFFFFF;
    int next = -1;
    for (size_t i = 0; i < m_voices.size(); i++) {
        if (!m_voices[i])
            return i;
        if (!(m_voices[i]->m_flags & instance::kProtected) && m_voices[i]->m_playIndex < slat) {
            slat = m_voices[i]->m_playIndex;
            next = int(i);
        }
    }
    stopVoice(next);
    return next;
}

int audio::play(source &sound, float volume, float pan, bool paused) {
    u::unique_ptr<instance> instance(sound.create());
    sound.m_owner = this;

    locked(m_mutex) {
        const int voice = findFreeVoice();
        if (voice < 0)
            return -1;

        if (sound.m_sourceID == 0)
            sound.m_sourceID = m_sourceID++;
        m_voices[voice] = instance.release();
        m_voices[voice]->m_sourceID = sound.m_sourceID;
        int handle = voice | (m_playIndex << 12);
        m_voices[voice]->init(m_playIndex, sound.m_baseSampleRate, sound.m_channels, sound.m_flags);
        if (paused)
            m_voices[voice]->m_flags |= instance::kPaused;

        setVoicePan(voice, pan);
        setVoiceVolume(voice, volume);
        setVoiceRelativePlaySpeed(voice, 1.0f);

        for (size_t i = 0; i < kMaxStreamFilters; i++)
            if (sound.m_filters[i])
                m_voices[voice]->m_filters[i] = sound.m_filters[i]->create();

        m_playIndex++;
        const size_t scratchNeeded = size_t(m::ceil((m_voices[voice]->m_sampleRate / m_sampleRate) * m_bufferSize));
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

int audio::getVoiceFromHandle(int voiceHandle) const {
    if (voiceHandle < 0)
        return -1;
    const int voice = voiceHandle & 0xFF;
    const size_t index = voiceHandle >> 12;
    if (m_voices[voice] && (m_voices[voice]->m_playIndex & 0xFFFFF) == index)
        return voice;
    return -1;
}

float audio::getVolume(int voiceHandle) const {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_volume.z;
}

float audio::getStreamTime(int voiceHandle) const {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_streamTime;
}

float audio::getRelativePlaySpeed(int voiceHandle) const {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 1.0f : m_voices[voice]->m_relativePlaySpeed;
}

void audio::setVoiceRelativePlaySpeed(int voice, float speed) {
    if (m_voices[voice]) {
        m_voices[voice]->m_relativePlaySpeed = speed;
        m_voices[voice]->m_sampleRate = m_voices[voice]->m_baseSampleRate * speed;
        const size_t scratchNeeded = size_t(m::ceil((m_voices[voice]->m_sampleRate / m_sampleRate) * m_bufferSize));
        if (m_scratchNeeded < scratchNeeded) {
            // calculate 1024 byte chunks needed for the scratch space
            size_t next = 1024;
            while (next < scratchNeeded) next <<= 1;
            m_scratchNeeded = next;
        }
    }
}

void audio::setRelativePlaySpeed(int voiceHandle, float speed) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_relativePlaySpeedFader.m_active = 0;
    setVoiceRelativePlaySpeed(voice, speed);
}

float audio::getSampleRate(int voiceHandle) const {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_baseSampleRate;
}

void audio::setSampleRate(int voiceHandle, float sampleRate) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_baseSampleRate = sampleRate;
    m_voices[voice]->m_sampleRate = sampleRate * m_voices[voice]->m_relativePlaySpeed;
}

void audio::seek(int voiceHandle, float seconds) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->seek(seconds, &m_scratch[0], m_scratch.size());
}

void audio::setPaused(int voiceHandle, bool paused) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    setVoicePaused(voice, paused);
}

void audio::setPausedAll(bool paused) {
    lockGuard lock(m_mutex);
    for (size_t i = 0; i < m_voices.size(); i++)
        setVoicePaused(i, paused);
}

bool audio::getPaused(int voiceHandle) const {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? false : !!(m_voices[voice]->m_flags & instance::kPaused);
}

bool audio::getProtected(int voiceHandle) const {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? false : !!(m_voices[voice]->m_flags & instance::kProtected);
}

void audio::setProtected(int voiceHandle, bool protect) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    if (protect)
        m_voices[voice]->m_flags |= instance::kProtected;
    else
        m_voices[voice]->m_flags &= ~instance::kProtected;
}

void audio::setVoicePaused(int voice, bool paused) {
    if (m_voices[voice]) {
        m_voices[voice]->m_pauseScheduler.m_active = 0;
        if (paused)
            m_voices[voice]->m_flags |= instance::kPaused;
        else
            m_voices[voice]->m_flags &= ~instance::kPaused;
    }
}

#define FadeSetRelative(FADER, TYPE, FROM, TO, TIME) \
    (m_voices[voice]->FADER.set(fader::TYPE, (FROM), (TO), (TIME), m_voices[voice]->m_streamTime))

void audio::schedulePause(int voiceHandle, float time) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    FadeSetRelative(m_pauseScheduler, kLERP, 1.0f, 0.0f, time);
}

void audio::scheduleStop(int voiceHandle, float time) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    FadeSetRelative(m_stopScheduler, kLERP, 1.0f, 0.0f, time);
}

void audio::setVoicePan(int voice, float pan) {
    if (m_voices[voice]) {
        const m::vec2 panning = m::sincos((pan + 1.0f) * m::kPi / 4.0f);
        m_voices[voice]->m_volume.x = panning.x;
        m_voices[voice]->m_volume.y = panning.y;
    }
}

void audio::setPan(int voiceHandle, float pan) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    setVoicePan(voice, pan);
}

void audio::setPanAbsolute(int voiceHandle, const m::vec2 &panning) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_panFader.m_active = 0;
    m_voices[voice]->m_volume.x = panning.x;
    m_voices[voice]->m_volume.y = panning.y;
}

void audio::setVoiceVolume(int voice, float volume) {
    if (m_voices[voice])
        m_voices[voice]->m_volume.z = volume;
}

void audio::setVolume(int voiceHandle, float volume) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_volumeFader.m_active = 0;
    setVoiceVolume(voice, volume);
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

void audio::stop(int voiceHandle) {
    lockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    stopVoice(voice);
}

void audio::stopVoice(int voice) {
    if (m_voices[voice]) {
        delete m_voices[voice];
        m_voices[voice] = nullptr;
    }
}

void audio::stopSound(source &sound) {
    lockGuard lock(m_mutex);
    if (sound.m_sourceID) {
        for (size_t i = 0; i < m_voices.size(); i++)
            if (m_voices[i] && m_voices[i]->m_sourceID == sound.m_sourceID)
                stopVoice(i);
    }
}

void audio::setFilterParam(int voiceHandle, int filterHandle, int attrib, float value) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (voiceHandle == 0) {
        lockGuard lock(m_mutex);
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->setFilterParam(attrib, value);
        return;
    }
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;

    locked(m_mutex) {
        if (m_voices[voice] && m_voices[voice]->m_filters[filterHandle])
            m_voices[voice]->m_filters[filterHandle]->setFilterParam(attrib, value);
    }
}

void audio::fadeFilterParam(int voiceHandle,
                            int filterHandle,
                            int attrib,
                            float from,
                            float to,
                            float time)
{
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (voiceHandle == 0) {
        lockGuard lock(m_mutex);
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->fadeFilterParam(attrib, from, to, time, m_streamTime);
        return;
    }
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;

    locked(m_mutex) {
        if (m_voices[voice] && m_voices[voice]->m_filters[filterHandle])
            m_voices[voice]->m_filters[filterHandle]->fadeFilterParam(attrib, from, to, time, m_streamTime);
    }
}

void audio::oscFilterParam(int voiceHandle,
                           int filterHandle,
                           int attrib,
                           float from,
                           float to,
                           float time)
{
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (voiceHandle == 0) {
        lockGuard lock(m_mutex);
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->oscFilterParam(attrib, from, to, time, m_streamTime);
        return;
    }
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;

    locked(m_mutex) {
        if (m_voices[voice] && m_voices[voice]->m_filters[filterHandle])
            m_voices[voice]->m_filters[filterHandle]->oscFilterParam(attrib, from, to, time, m_streamTime);
    }
}

void audio::stopAll() {
    lockGuard lock(m_mutex);
    for (size_t i = 0; i < m_voices.size(); i++)
        stopVoice(i);
}

void audio::fadeVolume(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        FadeSetRelative(m_volumeFader, kLERP, from, to, time);
    }
}

void audio::fadePan(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        FadeSetRelative(m_panFader, kLERP, from, to, time);
    }
}

void audio::fadeRelativePlaySpeed(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
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

void audio::oscVolume(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        FadeSetRelative(m_volumeFader, kLFO, from, to, time);
    }
}

void audio::oscPan(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        FadeSetRelative(m_panFader, kLFO, from, to, time);
    }
}

void audio::oscRelativePlaySpeed(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
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
        // process per-voice faders
        for (size_t i = 0; i < m_voices.size(); i++) {
            if (!m_voices[i] || (m_voices[i]->m_flags & instance::kPaused))
                continue;

            m_voices[i]->m_activeFader = 0;
            if (m_globalVolumeFader.m_active > 0)
                m_voices[i]->m_activeFader = 1;

            m_voices[i]->m_streamTime += bufferTime;

            if (m_voices[i]->m_relativePlaySpeedFader.m_active > 0) {
                const float speed = m_voices[i]->m_relativePlaySpeedFader(m_voices[i]->m_streamTime);
                setVoiceRelativePlaySpeed(i, speed);
            }

            m::vec2 volume;
            volume.x = m_voices[i]->m_volume.z;
            if (m_voices[i]->m_volumeFader.m_active > 0) {
                m_voices[i]->m_volume.z = m_voices[i]->m_volumeFader(m_voices[i]->m_streamTime);
                m_voices[i]->m_activeFader = 1;
            }
            volume.y = m_voices[i]->m_volume.z;

            m::vec2 panL;
            m::vec2 panR;
            panL.x = m_voices[i]->m_volume.x;
            panR.x = m_voices[i]->m_volume.y;
            if (m_voices[i]->m_panFader.m_active > 0) {
                const float pan = m_voices[i]->m_panFader(m_voices[i]->m_streamTime);
                setVoicePan(i, pan);
                m_voices[i]->m_activeFader = 1;
            }
            panL.y = m_voices[i]->m_volume.x;
            panR.y = m_voices[i]->m_volume.y;

            if (m_voices[i]->m_pauseScheduler.m_active) {
                m_voices[i]->m_pauseScheduler(m_voices[i]->m_streamTime);
                if (m_voices[i]->m_pauseScheduler.m_active == -1) {
                    m_voices[i]->m_pauseScheduler.m_active = 0;
                    setVoicePaused(i, true);
                }
            }

            if (m_voices[i]->m_activeFader) {
                m_voices[i]->m_faderVolume[0*2+0] = panL.x * volume.x * globalVolume.x;
                m_voices[i]->m_faderVolume[0*2+1] = panL.y * volume.y * globalVolume.y;
                m_voices[i]->m_faderVolume[1*2+0] = panR.x * volume.x * globalVolume.x;
                m_voices[i]->m_faderVolume[1*2+1] = panR.y * volume.y * globalVolume.y;
            }

            if (m_voices[i]->m_stopScheduler.m_active) {
                m_voices[i]->m_stopScheduler(m_voices[i]->m_streamTime);
                if (m_voices[i]->m_stopScheduler.m_active == -1) {
                    m_voices[i]->m_stopScheduler.m_active = 0;
                    stopVoice(i);
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
        for (size_t i = 0; i < m_voices.size(); i++) {
            if (!m_voices[i] || (m_voices[i]->m_flags & instance::kPaused))
                continue;

            const float next = m_voices[i]->m_sampleRate / m_sampleRate;
            float step = 0.0f;

            const size_t readSamples = m::ceil(samples * next);

            // get the audio from the source into our scratch buffer
            if (m_voices[i]->hasEnded())
                memset(&m_scratch[0], 0, sizeof(float) * m_scratch.size());
            else
                m_voices[i]->getAudio(&m_scratch[0], readSamples);

            for (size_t j = 0; j < kMaxStreamFilters; j++) {
                if (m_voices[i]->m_filters[j]) {
                    m_voices[i]->m_filters[j]->filter(&m_scratch[0],
                                                        readSamples,
                                                        m_voices[i]->m_channels,
                                                        m_voices[i]->m_sampleRate,
                                                        m_streamTime);
                }
            }

            if (m_voices[i]->m_activeFader) {
                float panL = m_voices[i]->m_faderVolume[0];
                float panR = m_voices[i]->m_faderVolume[2];
                const float panLi = (m_voices[i]->m_faderVolume[1] - m_voices[i]->m_faderVolume[0]) / samples;
                const float panRi = (m_voices[i]->m_faderVolume[3] - m_voices[i]->m_faderVolume[2]) / samples;
                if (m_voices[i]->m_channels == 2) {
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
                const float panL = m_voices[i]->m_volume.x * m_voices[i]->m_volume.z * m_globalVolume;
                const float panR = m_voices[i]->m_volume.y * m_voices[i]->m_volume.z * m_globalVolume;
                if (m_voices[i]->m_channels == 2) {
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
            if (!(m_voices[i]->m_flags & instance::kLooping) && m_voices[i]->hasEnded())
                stopVoice(i);
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
