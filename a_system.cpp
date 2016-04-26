#include <SDL.h>

#include "a_system.h"

#include "m_const.h"
#include "m_trig.h"

#include "u_new.h"
#include "u_misc.h"

#include "engine.h"

namespace a {

static void audioMixer(void *user, Uint8 *stream, int length) {
    const int samples = length / 4;
    short *buffer = (short *)stream;
    a::audio *system = (a::audio *)user;
    float *data = (float *)system->m_mixerData;
    system->mix(data, samples);
    for (int i = 0; i < samples*2; i++)
        buffer[i] = (short)(data[i] * 0x7fff);
}

void audioInit(a::audio *system) {
    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = &audioMixer;
    spec.userdata = (void *)system;

    SDL_AudioSpec got;
    if (SDL_OpenAudio(&spec, &got) < 0)
        neoFatalError("failed to initialize audio");

    system->m_mixerData = neoMalloc(sizeof(float) * got.samples*4);

    u::print("[audio] => device configured for %d channels @ %dHz (%d samples)\n", got.channels, got.freq, got.samples);
    SDL_PauseAudio(0);
}

void audioShutdown(a::audio *system) {
    neoFree(system->m_mixerData);
    SDL_CloseAudio();
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

///! producer
producer::producer()
    : m_playIndex(0)
    , m_flags(0)
    , m_baseSampleRate(44100.0f)
    , m_sampleRate(44100.0f)
    , m_relativePlaySpeed(1.0f)
    , m_streamTime(0.0f)
{
    m_volume.x = 1.0f / m::sqrt(2.0f);
    m_volume.y = 1.0f / m::sqrt(2.0f);
    m_volume.z = 1.0f;
}

producer::~producer() {
    // Empty
}

void producer::init(size_t playIndex, float baseSampleRate, int factoryFlags) {
    m_playIndex = playIndex;
    m_baseSampleRate = baseSampleRate;
    m_sampleRate = m_baseSampleRate;
    m_streamTime = 0.0f;
    m_flags = 0;
    if (factoryFlags & factory::kLoop)
        m_flags |= producer::kLooping;
    if (factoryFlags & factory::kStereo)
        m_flags |= producer::kStereo;
}

bool producer::rewind() {
    // TODO
    return false;
}

void producer::seek(float seconds, float *scratch, size_t scratchSize) {
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

///! factory
factory::factory()
    : m_flags(0)
    , m_baseSampleRate(44100.0f)
{
}

factory::~factory() {
    // Empty
}

void factory::setLooping(bool looping) {
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
    , m_playIndex(0)
    , m_streamTime(0)
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
    m_scratchNeeded = 1;
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
        if (!(m_channels[i]->m_flags & producer::kProtected) && m_channels[i]->m_playIndex < slat) {
            slat = m_channels[i]->m_playIndex;
            next = int(i);
        }
    }
    stop(next);
    return next;
}

int audio::play(factory &sound, float volume, float pan, bool paused) {
    int channel = findFreeChannel();
    if (channel < 0) return -1;

    m_channels[channel] = sound.create();
    int handle = channel | (m_playIndex << 8);
    m_channels[channel]->init(m_playIndex, sound.m_baseSampleRate, sound.m_flags);
    if (paused)
        m_channels[channel]->m_flags |= producer::kPaused;

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

    return handle;
}

int audio::getChannelFromHandle(int handle) const {
    const int channel = handle & 0xFF;
    const size_t index = handle >> 8;
    if (m_channels[index] && (m_channels[index]->m_playIndex & 0xFFFFFF) == index)
        return channel;
    return -1;
}

float audio::getVolume(int channelHandle) const {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return 0.0f;
    return m_channels[channel]->m_volume.z;
}

float audio::getStreamTime(int channelHandle) const {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return 0.0f;
    return m_channels[channel]->m_streamTime;
}

float audio::getRelativePlaySpeed(int channelHandle) const {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return 1.0f;
    return m_channels[channel]->m_relativePlaySpeed;
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
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_relativePlaySpeedFader.m_active = false;
    setChannelRelativePlaySpeed(channel, speed);
}

float audio::getSampleRate(int channelHandle) const {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return 0.0f;
    return m_channels[channel]->m_baseSampleRate;
}

void audio::setSampleRate(int channelHandle, float sampleRate) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_baseSampleRate = sampleRate;
    m_channels[channel]->m_sampleRate = sampleRate * m_channels[channel]->m_relativePlaySpeed;
}

void audio::seek(int channelHandle, float seconds) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->seek(seconds, &m_scratch[0], m_scratch.size());
}

void audio::setPaused(int channelHandle, bool paused) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    if (paused)
        m_channels[channel]->m_flags |= producer::kPaused;
    else
        m_channels[channel]->m_flags &= ~producer::kPaused;
}

void audio::setPausedAll(bool paused) {
    for (size_t i = 0; i < m_channels.size(); i++) {
        if (m_channels[i]) {
            if (paused)
                m_channels[i]->m_flags |= producer::kPaused;
            else
                m_channels[i]->m_flags &= ~producer::kPaused;
        }
    }
}

bool audio::getPaused(int channelHandle) const {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return false;
    return !!(m_channels[channel]->m_flags & producer::kPaused);
}

bool audio::getProtected(int channelHandle) const {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return false;
    return !!(m_channels[channel]->m_flags & producer::kProtected);
}

void audio::setProtected(int channelHandle, bool protect) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    if (protect)
        m_channels[channel]->m_flags |= producer::kProtected;
    else
        m_channels[channel]->m_flags &= ~producer::kProtected;
}

void audio::setChannelPan(int channel, float pan) {
    if (m_channels[channel]) {
        const m::vec2 panning = m::sincos((pan + 1.0f) * m::kPi / 4.0f);
        m_channels[channel]->m_volume.x = panning.x;
        m_channels[channel]->m_volume.y = panning.y;
    }
}

void audio::setChannelVolume(int channel, float volume) {
    if (m_channels[channel])
        m_channels[channel]->m_volume.z = volume;
}

void audio::setPan(int channelHandle, float pan) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    setChannelPan(channel, pan);
}

void audio::setPanAbsolute(int channelHandle, const m::vec2 &panning) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_panFader.m_active = false;
    m_channels[channel]->m_volume.x = panning.x;
    m_channels[channel]->m_volume.y = panning.y;
}

void audio::setVolume(int channelHandle, float volume) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_volumeFader.m_active = false;
    setChannelVolume(channel, volume);
}

void audio::stop(int channelHandle) {
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    stopChannel(channel);
}

void audio::stopChannel(int channel) {
    if (m_channels[channel]) {
        delete m_channels[channel];
        m_channels[channel] = nullptr;
    }
}

void audio::stopAll() {
    for (size_t i = 0; i < m_channels.size(); i++)
        stopChannel(i);
}

void audio::fadeVolume(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(channelHandle, to);
        return;
    }
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_volumeFader.set(from, to, time, m_channels[channel]->m_streamTime);
}

void audio::fadePan(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(channelHandle, to);
        return;
    }
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_panFader.set(from, to, time, m_channels[channel]->m_streamTime);
}

void audio::fadeRelativePlaySpeed(int channelHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(channelHandle, to);
        return;
    }
    const int channel = getChannelFromHandle(channelHandle);
    if (channel == -1) return;
    m_channels[channel]->m_relativePlaySpeedFader.set(from, to, time, m_channels[channel]->m_streamTime);
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

    // process per-channel faders
    for (size_t i = 0; i < m_channels.size(); i++) {
        if (m_channels[i] && !(m_channels[i]->m_flags & producer::kPaused)) {
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
        if (m_channels[i] && !(m_channels[i]->m_flags & producer::kPaused)) {
            const float panL = m_channels[i]->m_volume.x * m_channels[i]->m_volume.z * m_globalVolume;
            const float panR = m_channels[i]->m_volume.y * m_channels[i]->m_volume.z * m_globalVolume;
            const float next = m_channels[i]->m_sampleRate / m_sampleRate;

            // get the audio from the source into our scratch buffer
            m_channels[i]->getAudio(&m_scratch[0], m::ceil(samples * next));

            float step = 0.0f;
            if (m_channels[i]->m_flags & producer::kStereo) {
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
            if (!(m_channels[i]->m_flags & producer::kLooping) && m_channels[i]->hasEnded())
                stopChannel(i);
        }
    }

    // clipp
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
