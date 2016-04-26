#include <SDL.h>

#include "a_system.h"

#include "m_const.h"
#include "m_trig.h"

#include "u_new.h"

#include "engine.h"

namespace a {

static void audioMixer(void *user, Uint8 *stream, int length) {
    const int samples = length / 4;
    short *buffer = (short *)stream;
    a::audio *system = (a::audio *)user;
    float *data = (float *)system->mixerData;
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

    system->mixerData = neoMalloc(sizeof(float) * got.samples*4);

    u::print("[audio] => initialized for %d channels @ %dHz (%d samples)\n", got.channels, got.freq, got.samples);
    SDL_PauseAudio(0);
}

void audioShutdown(a::audio *system) {
    neoFree(system->mixerData);
    SDL_CloseAudio();
}

void factory::setLooping(bool looping) {
    if (looping)
        m_flags |= kLoop;
    else
        m_flags &= ~kLoop;
}

producer::producer()
    : m_playIndex(0)
    , m_flags(0)
    , m_baseSampleRate(44100.0f)
    , m_sampleRate(44100.0f)
    , m_relativePlaySpeed(1.0f)
{
    m_volume.x = 1.0f / m::sqrt(2.0f);
    m_volume.y = 1.0f / m::sqrt(2.0f);
    m_volume.z = 1.0f;
}

void audio::init(int channels, int sampleRate, int bufferSize, int flags) {
    m_globalVolume = 1.0f;
    m_channels.resize(channels);
    m_sampleRate = sampleRate;
    m_scratchNeeded = 1;
    m_bufferSize = bufferSize;
    m_flags = flags;
    m_postClipScaler = 0.5f;
}

void audio::setVolume(float volume) {
    m_globalVolume = volume;
}

int audio::findFreeChannel() {
    unsigned int lowest = 0xFFFFFFFF;
    int current = -1;
    for (size_t i = 0; i < m_channels.size(); i++) {
        if (m_channels[i] == nullptr)
            return i;
        if ((m_channels[i]->m_flags & producer::kProtected) == 0 && m_channels[i]->m_playIndex < lowest) {
            lowest = m_channels[i]->m_playIndex;
            current = int(i);
        }
    }
    stop(current);
    return current;
}

int audio::play(factory &sound, float volume, float pan, bool paused) {
    int channel = findFreeChannel();
    if (channel < 0)
        return -1;

    m_channels[channel] = sound.create();
    m_channels[channel]->m_playIndex = m_playIndex;
    m_channels[channel]->m_flags = paused ? producer::kPaused : 0;
    m_channels[channel]->m_baseSampleRate = sound.m_baseSampleRate;
    int handle = channel | (m_channels[channel]->m_playIndex << 8);
    setRelativePlaySpeed(handle, 1);
    setPan(handle, pan);
    setVolume(handle, volume);
    if (sound.m_flags & factory::kLoop)
        m_channels[channel]->m_flags |= producer::kLooping;
    if (sound.m_flags & factory::kStereo)
        m_channels[channel]->m_flags |= producer::kStereo;
    m_playIndex++;
    size_t scratchNeeded = size_t(m::ceil((m_channels[channel]->m_sampleRate / m_sampleRate) * m_bufferSize));
    if (m_scratchNeeded < scratchNeeded) {
        size_t next = 1024;
        while (next < scratchNeeded) next <<= 1;
        m_scratchNeeded = next;
    }
    return handle;
}

producer *audio::absChannel(int handle) const {
    int channel = handle & 0xFF;
    unsigned int index = handle >> 8;
    if (m_channels[channel] && (m_channels[channel]->m_playIndex & 0xFFFFFF) == index)
        return m_channels[channel];
    return nullptr;
}

float audio::getRelativePlaySpeed(int handle) const {
    auto channel = absChannel(handle);
    return channel ? channel->m_relativePlaySpeed : 0.0f;
}

float audio::getVolume(int handle) const {
    auto channel = absChannel(handle);
    return channel ? channel->m_volume.z : 0.0f;
}

float audio::getSampleRate(int handle) const {
    auto channel = absChannel(handle);
    return channel ? channel->m_baseSampleRate : 0.0f;
}

float audio::getPostClipScaler() const {
    return m_postClipScaler;
}

void audio::setRelativePlaySpeed(int handle, float speed) {
    auto channel = absChannel(handle);
    if (channel == nullptr)
        return;
    channel->m_relativePlaySpeed = speed;
    channel->m_sampleRate = channel->m_baseSampleRate * channel->m_relativePlaySpeed;
    size_t scratchNeeded = size_t(m::ceil((channel->m_sampleRate / m_sampleRate) * m_bufferSize));
    if (m_scratchNeeded < scratchNeeded) {
        size_t next = 1024;
        while (next < scratchNeeded) next <<= 1;
        m_scratchNeeded = next;
    }
}

void audio::setSampleRate(int handle, float sampleRate) {
    auto channel = absChannel(handle);
    if (channel == nullptr)
        return;
    channel->m_baseSampleRate = sampleRate;
    channel->m_sampleRate = channel->m_baseSampleRate * channel->m_relativePlaySpeed;
}

void audio::setPostClipScaler(float scaler) {
    m_postClipScaler = scaler;
}

bool audio::getProtected(int handle) const {
    auto channel = absChannel(handle);
    return channel ? !!(channel->m_flags & producer::kProtected) : false;
}

bool audio::getPaused(int handle) const {
    auto channel = absChannel(handle);
    return channel ? !!(channel->m_flags & producer::kPaused) : false;
}

void audio::setProtected(int handle, bool protect) {
    auto channel = absChannel(handle);
    if (channel == nullptr)
        return;
    if (protect)
        channel->m_flags |= producer::kProtected;
    else
        channel->m_flags &= ~producer::kProtected;
}

void audio::setPaused(int handle, bool paused) {
    auto channel = absChannel(handle);
    if (channel == nullptr)
        return;
    if (paused)
        channel->m_flags |= producer::kPaused;
    else
        channel->m_flags &= ~producer::kPaused;
}

void audio::setPan(int handle, float pan) {
    setPanAbs(handle, m::sincos((pan + 1.0f) * m::kPi / 4.0f));
}

void audio::setPanAbs(int handle, const m::vec2 &panning) {
    auto channel = absChannel(handle);
    if (channel == nullptr)
        return;
    channel->m_volume.x = panning.x;
    channel->m_volume.y = panning.y;
}

void audio::setVolume(int handle, float volume) {
    auto channel = absChannel(handle);
    if (channel)
        channel->m_volume.z = volume;
}

void audio::stop(int handle) {
    auto channel = absChannel(handle);
    for (size_t i = 0; i < m_channels.size(); i++) {
        if (m_channels[i] != channel)
            continue;
        m_channels[i] = nullptr;
        break;
    }
    delete channel;
}

void audio::stopAbs(int channel) {
    if (m_channels[channel]) {
        delete m_channels[channel];
        m_channels[channel] = nullptr;
    }
}

void audio::stop() {
    for (size_t i = 0; i < m_channels.size(); i++)
        stopAbs(i);
}

void audio::mix(float *buffer, int samples) {
    if (m_scratch.size() < m_scratchNeeded)
        m_scratch.resize(m_scratchNeeded);
    // clear accumulation buffer
    for (int i = 0; i < samples*2; i++)
        buffer[i] = 0.0f;
    // accumulate sound sources
    int index = 0;
    for (auto *it : m_channels) {
        if (it && !(it->m_flags & producer::kPaused)) {
            float lPan = it->m_volume.x * it->m_volume.z * m_globalVolume;
            float rPan = it->m_volume.y * it->m_volume.z * m_globalVolume;
            float next = it->m_sampleRate / m_sampleRate;
            it->getAudio(&m_scratch[0], int(m::ceil(samples * next)));

            float step = 0.0f;
            if (it->m_flags & producer::kStereo) {
                for (int j = 0; j < samples; j++, step += next) {
                    const float sample1 = m_scratch[(int)m::floor(step)*2];
                    const float sample2 = m_scratch[(int)m::floor(step)*2+1];
                    buffer[j * 2 + 0] += sample1 * lPan;
                    buffer[j * 2 + 1] += sample2 * rPan;
                }
            } else {
                for (int j = 0; j < samples; j++, step += next) {
                    const float sample = m_scratch[(int)m::floor(step)];
                    buffer[j * 2 + 0] += sample * lPan;
                    buffer[j * 2 + 1] += sample * rPan;
                }
            }

            // clear channel if the sound is over
            if (!(it->m_flags & producer::kLooping) && it->hasEnded())
                stopAbs(index);
        }
        index++;
    }
    // clipping
    if (m_flags & kClipRoundOff) {
        for (int i = 0; i < samples*2; i++) {
            buffer[i] = (buffer[i] <= -1.65f ? -0.9862875f :
                         buffer[i] >=  1.65f ?  0.9862875f : 0.87f * buffer[i] - 0.1f * buffer[i] * buffer[i] * buffer[i]) * m_postClipScaler;
        }
    } else {
        for (int i = 0; i < samples; i++)
            buffer[i] = m::clamp(buffer[i], -1.0f, 1.0f) * m_postClipScaler;
    }
}

audio::audio()
    : mixerData(nullptr)
    , m_scratchNeeded(0)
    , m_sampleRate(0)
    , m_bufferSize(0)
    , m_flags(0)
    , m_globalVolume(0)
    , m_playIndex(0)
{
}

audio::~audio() {
    stop();
    for (auto *it : m_channels)
        delete it;
}

}
