#include <SDL.h>

#include "a_system.h"

#include "m_const.h"
#include "m_trig.h"

#include "u_new.h"
#include "u_misc.h"

#include "engine.h"

namespace a {

#define LOCK()   SDL_LockMutex((SDL_mutex *)m_mutex)
#define UNLOCK() SDL_UnlockMutex((SDL_mutex *)m_mutex)

static void audioMixer(void *user, Uint8 *stream, int length) {
    const int samples = length / 4;
    short *buffer = (short *)stream;
    a::audio *system = (a::audio *)user;
    float *data = (float *)system->m_mixerData;
    system->mix(data, samples);
    for (int i = 0; i < samples*2; i++)
        buffer[i] = (short)(data[i] * 0x7fff);
}

void audioInit(a::audio *system, int channels, int flags, int sampleRate, int bufferSize) {
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

void audioShutdown(a::audio *system) {
    SDL_CloseAudio();
    // free the mixer data after shutting down the mixer thread
    neoFree(system->m_mixerData);
    // destroy the mutex after shutting down the mixer thread
    SDL_DestroyMutex((SDL_mutex*)system->m_mutex);
}

///! fader
fader::fader()
    : m_current(0.0f)
    , m_from(0.0f)
    , m_to(0.0f)
    , m_delta(0.0f)
    , m_time(0.0f)
    , m_startTime(0.0f)
    , m_endTime(0.0f)
    , m_active(false)
{
}

void fader::set(float from, float to, float time, float startTime) {
    m_current = from;
    m_from = from;
    m_to = to;
    m_time = time;
    m_startTime = startTime;
    m_delta = to - from;
    m_endTime = m_startTime + time;
    m_active = true;
}

float fader::get(float currentTime) {
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
        m_active = false;
        return m_to;
    }
    m_current = m_from + m_delta * ((currentTime - m_startTime) / m_time);
    return m_current;
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
{
    m_volume.x = 1.0f / m::sqrt(2.0f);
    m_volume.y = 1.0f / m::sqrt(2.0f);
    m_volume.z = 1.0f;
}

instance::~instance() {
    // Empty
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

///! audio
audio::audio()
    : m_scratchNeeded(0)
    , m_sampleRate(0)
    , m_bufferSize(0)
    , m_flags(0)
    , m_globalVolume(0)
    , m_postClipScaler(0.0f)
    , m_playIndex(0)
    , m_streamTime(0)
    , m_sourceID(1)
    , m_mutex(nullptr)
    , m_mixerData(nullptr)
{
}

audio::~audio() {
    stopAll();
}

void audio::init(int channels, int sampleRate, int bufferSize, int flags) {
    m_globalVolume = 1.0f;
    m_channels.resize(channels);
    m_sampleRate = sampleRate;
    m_scratchNeeded = 2048;
    m_scratch.resize(2048);
    m_bufferSize = bufferSize;
    m_flags = flags;
    m_postClipScaler = 0.5f;
    u::print("[audio] => initialized for %d channels @ %dHz with %s buffer\n",
        channels, sampleRate, u::sizeMetric(bufferSize));
}

float audio::getPostClipScaler() const {
    return m_postClipScaler;
}

void audio::setPostClipScaler(float scaler) {
    m_postClipScaler = scaler;
}

void audio::setGlobalVolume(float volume) {
    m_globalVolumeFader.m_active = false;
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
    LOCK();
    const int channel = findFreeChannel();
    if (channel < 0) {
        UNLOCK();
        return -1;
    }

    if (sound.m_sourceID == 0) {
        sound.m_sourceID = m_sourceID++;
        sound.m_owner = this;
    }
    m_channels[channel] = sound.create();
    m_channels[channel]->m_sourceID = sound.m_sourceID;
    int handle = channel | (m_playIndex << 8);
    m_channels[channel]->init(m_playIndex, sound.m_baseSampleRate, sound.m_flags);
    if (paused)
        m_channels[channel]->m_flags |= instance::kPaused;

    setChannelPan(channel, pan);
    setChannelVolume(channel, volume);
    setChannelRelativePlaySpeed(channel, 1.0f);

    m_playIndex++;
    const size_t scratchNeeded = size_t(m::ceil((m_channels[channel]->m_sampleRate / m_sampleRate) * m_bufferSize));
    if (m_scratchNeeded < scratchNeeded) {
        // calculate 1024 byte chunks needed for the scratch space
        size_t next = 1024;
        while (next < scratchNeeded) next <<= 1;
        m_scratchNeeded = next;
    }

    UNLOCK();
    return handle;
}

int audio::getChannelFromHandle(int handle) const {
    if (handle < 0)
        return -1;
    const int channel = handle & 0xFF;
    const size_t index = handle >> 8;
    if (m_channels[index] && (m_channels[index]->m_playIndex & 0xFFFFFF) == index)
        return channel;
    return -1;
}

float audio::getVolume(int channelHandle) const {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return 0.0f;
    }
    const float value = m_channels[channel]->m_volume.z;
    UNLOCK();
    return value;
}

float audio::getStreamTime(int channelHandle) const {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return 0.0f;
    }
    const float value = m_channels[channel]->m_streamTime;
    UNLOCK();
    return value;
}

float audio::getRelativePlaySpeed(int channelHandle) const {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return 1.0f;
    }
    const float value = m_channels[channel]->m_relativePlaySpeed;
    UNLOCK();
    return value;
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
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_relativePlaySpeedFader.m_active = false;
    setChannelRelativePlaySpeed(channel, speed);
    UNLOCK();
}

float audio::getSampleRate(int channelHandle) const {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return 0.0f;
    }
    const float value = m_channels[channel]->m_baseSampleRate;
    UNLOCK();
    return value;
}

void audio::setSampleRate(int channelHandle, float sampleRate) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_baseSampleRate = sampleRate;
    m_channels[channel]->m_sampleRate = sampleRate * m_channels[channel]->m_relativePlaySpeed;
    UNLOCK();
}

void audio::seek(int channelHandle, float seconds) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->seek(seconds, &m_scratch[0], m_scratch.size());
    UNLOCK();
}

void audio::setPaused(int channelHandle, bool paused) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    setChannelPaused(channel, paused);
    UNLOCK();
}

void audio::setPausedAll(bool paused) {
    LOCK();
    for (size_t i = 0; i < m_channels.size(); i++)
        setChannelPaused(i, paused);
    UNLOCK();
}

bool audio::getPaused(int channelHandle) const {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return false;
    }
    const bool value = !!(m_channels[channel]->m_flags & instance::kPaused);
    UNLOCK();
    return value;
}

bool audio::getProtected(int channelHandle) const {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return false;
    }
    const bool value = !!(m_channels[channel]->m_flags & instance::kProtected);
    UNLOCK();
    return value;
}

void audio::setProtected(int channelHandle, bool protect) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    if (protect)
        m_channels[channel]->m_flags |= instance::kProtected;
    else
        m_channels[channel]->m_flags &= ~instance::kProtected;
    UNLOCK();
}

void audio::setChannelPaused(int channel, bool paused) {
    if (m_channels[channel]) {
        if (paused)
            m_channels[channel]->m_flags |= instance::kPaused;
        else
            m_channels[channel]->m_flags &= ~instance::kPaused;
    }
}

void audio::setChannelPan(int channel, float pan) {
    if (m_channels[channel]) {
        const m::vec2 panning = m::sincos((pan + 1.0f) * m::kPi / 4.0f);
        m_channels[channel]->m_volume.x = panning.x;
        m_channels[channel]->m_volume.y = panning.y;
    }
}

void audio::setPan(int channelHandle, float pan) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    setChannelPan(channel, pan);
    UNLOCK();
}

void audio::setPanAbsolute(int channelHandle, const m::vec2 &panning) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_panFader.m_active = false;
    m_channels[channel]->m_volume.x = panning.x;
    m_channels[channel]->m_volume.y = panning.y;
    UNLOCK();
}

void audio::setChannelVolume(int channel, float volume) {
    if (m_channels[channel])
        m_channels[channel]->m_volume.z = volume;
}

void audio::setVolume(int channelHandle, float volume) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_volumeFader.m_active = false;
    setChannelVolume(channel, volume);
    UNLOCK();
}

void audio::stop(int channelHandle) {
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    stopChannel(channel);
    UNLOCK();
}

void audio::stopChannel(int channel) {
    if (m_channels[channel]) {
        delete m_channels[channel];
        m_channels[channel] = nullptr;
    }
}

void audio::stopSound(source &sound) {
    if (sound.m_sourceID) {
        LOCK();
        for (size_t i = 0; i < m_channels.size(); i++)
            if (m_channels[i] && m_channels[i]->m_sourceID == sound.m_sourceID)
                stopChannel(i);
        UNLOCK();
    }
}

void audio::stopAll() {
    LOCK();
    for (size_t i = 0; i < m_channels.size(); i++)
        stopChannel(i);
    UNLOCK();
}

void audio::fadeVolume(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(channelHandle, to);
        return;
    }
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_volumeFader.set(from, to, time, m_channels[channel]->m_streamTime);
    UNLOCK();
}

void audio::fadePan(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(channelHandle, to);
        return;
    }
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_panFader.set(from, to, time, m_channels[channel]->m_streamTime);
    UNLOCK();
}

void audio::fadeRelativePlaySpeed(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(channelHandle, to);
        return;
    }
    LOCK();
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) {
        UNLOCK();
        return;
    }
    m_channels[channel]->m_relativePlaySpeedFader.set(from, to, time, m_channels[channel]->m_streamTime);
    UNLOCK();
}

void audio::fadeGlobalVolume(float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setGlobalVolume(to);
        return;
    }
    m_streamTime = 0.0f; // avoid ~6 day rollover
    m_globalVolumeFader.set(from, to, time, m_streamTime);
}

void audio::mix(float *buffer, size_t samples) {
    float bufferTime = samples / float(m_sampleRate);
    m_streamTime += bufferTime;

    // process global faders
    if (m_globalVolumeFader.m_active)
        m_globalVolume = m_globalVolumeFader.get(m_streamTime);

    LOCK();

    // process per-channel faders
    for (size_t i = 0; i < m_channels.size(); i++) {
        if (m_channels[i] && !(m_channels[i]->m_flags & instance::kPaused)) {
            m_channels[i]->m_streamTime += bufferTime;
            if (m_channels[i]->m_volumeFader.m_active)
                m_channels[i]->m_volume.z = m_channels[i]->m_volumeFader.get(m_channels[i]->m_streamTime);
            if (m_channels[i]->m_relativePlaySpeedFader.m_active) {
                const float speed = m_channels[i]->m_relativePlaySpeedFader.get(m_channels[i]->m_streamTime);
                setChannelRelativePlaySpeed(i, speed);
            }
            if (m_channels[i]->m_panFader.m_active) {
                const float pan = m_channels[i]->m_panFader.get(m_channels[i]->m_streamTime);
                setChannelPan(i, pan);
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
        if (m_channels[i] && !(m_channels[i]->m_flags & instance::kPaused)) {
            const float panL = m_channels[i]->m_volume.x * m_channels[i]->m_volume.z * m_globalVolume;
            const float panR = m_channels[i]->m_volume.y * m_channels[i]->m_volume.z * m_globalVolume;
            const float next = m_channels[i]->m_sampleRate / m_sampleRate;

            // get the audio from the source into our scratch buffer
            m_channels[i]->getAudio(&m_scratch[0], m::ceil(samples * next));

            float step = 0.0f;
            if (m_channels[i]->m_flags & instance::kStereo) {
                for (size_t j = 0; j < samples; j++, step += next) {
                    const float sampleL = m_scratch[(int)m::floor(step) *2 + 0];
                    const float sampleR = m_scratch[(int)m::floor(step) *2 + 1];
                    buffer[j * 2 + 0] += sampleL * panL;
                    buffer[j * 2 + 1] += sampleR * panR;
                }
            } else {
                for (size_t j = 0; j < samples; j++, step += next) {
                    const float sampleM = m_scratch[(int)m::floor(step)];
                    buffer[j * 2 + 0] += sampleM * panL;
                    buffer[j * 2 + 1] += sampleM * panR;
                }
            }

            // clear the channel if the sound is complete
            if (!(m_channels[i]->m_flags & instance::kLooping) && m_channels[i]->hasEnded())
                stopChannel(i);
        }
    }

    UNLOCK();

    // clip
    if (m_flags & kClipRoundOff) {
        // round off clipping is less aggressive
        for (size_t i = 0; i < samples << 1; i++) {
            float sample = buffer[i];
            /**/ if (sample <= -1.65f) sample = -0.9862875f;
            else if (sample >=  1.65f) sample =  0.9862875f;
            else                       sample =  0.87f * sample - 0.1f * sample * sample * sample;
            buffer[i] = sample * m_postClipScaler;
        }
    } else {
        // standard clipping may introduce noise and aliasing - need proper
        // hi-pass filter to prevent this
        for (size_t i = 0; i < samples; i++)
            buffer[i] = m::clamp(buffer[i], -1.0f, 1.0f) * m_postClipScaler;
    }
}

}
