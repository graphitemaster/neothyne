#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_mutex.h>

#include "a_system.h"
#include "a_filter.h"

#include "m_const.h"
#include "m_trig.h"

#include "u_new.h"
#include "u_misc.h"
#include "u_memory.h"

#include "engine.h"

#include "c_variable.h"

VAR(u::string, snd_device, "sound device");
VAR(u::string, snd_driver, "sound driver");
VAR(int, snd_frequency, "sound frequency", 11025, 48000, 44100);
VAR(int, snd_samples, "sound samples", 1024, 65536, 2048);
VAR(int, snd_voices, "maximum voices for mixing", 16, 128, 32);

namespace a {

LockGuard::LockGuard(void *opaque)
    : m_mutex(opaque)
{
    SDL_LockMutex((SDL_mutex *)m_mutex);
}

LockGuard::~LockGuard() {
    if (m_mutex)
        SDL_UnlockMutex((SDL_mutex *)m_mutex);
}

void LockGuard::unlock() {
    SDL_UnlockMutex((SDL_mutex *)m_mutex);
    m_mutex = nullptr;
}

static void audioMixer(void *user, Uint8 *stream, int length) {
    const int samples = length / 4;
    short *buffer = (short *)stream;
    a::Audio *system = (a::Audio *)user;
    float *data = (float *)system->m_mixerData;
    system->mix(data, samples);
    for (int i = 0; i < samples*2; i++)
        buffer[i] = (short)m::floor(data[i] * 0x7fff);
}

///! SourceInstance
SourceInstance::SourceInstance()
    : m_playIndex(0)
    , m_flags(0)
    , m_channels(1)
    , m_pan(0.0f)
    , m_baseSampleRate(44100.0f)
    , m_sampleRate(44100.0f)
    , m_relativePlaySpeed(1.0f)
    , m_streamTime(0.0f)
    , m_sourceID(0)
    , m_laneHandle(0)
    , m_activeFader(0)
{
    m_volume.x = 1.0f / m::sqrt(2.0f);
    m_volume.y = 1.0f / m::sqrt(2.0f);
    m_volume.z = 1.0f;
    memset(m_faderVolume, 0, sizeof m_faderVolume);
    memset(m_filters, 0, sizeof m_filters);
}

SourceInstance::~SourceInstance() {
    for (auto *it : m_filters)
        delete it;
}

void SourceInstance::init(size_t playIndex, float baseSampleRate, size_t channels, int sourceFlags) {
    m_playIndex = playIndex;
    m_baseSampleRate = baseSampleRate;
    m_sampleRate = m_baseSampleRate;
    m_channels = channels;
    m_streamTime = 0.0f;
    m_flags = 0;
    if (sourceFlags & Source::kLoop)
        m_flags |= SourceInstance::kLooping;
}

bool SourceInstance::rewind() {
    return false;
}

void SourceInstance::seek(float seconds, float *scratch, size_t scratchSize) {
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

///! Source
Source::Source()
    : m_flags(0)
    , m_baseSampleRate(44100.0f)
    , m_channels(1)
    , m_sourceID(0)
    , m_owner(nullptr)
{
    memset(m_filters, 0, sizeof m_filters);
}

Source::~Source() {
    if (m_owner)
        m_owner->stopSound(*this);
}

void Source::setLooping(bool looping) {
    if (looping)
        m_flags |= kLoop;
    else
        m_flags &= ~kLoop;
}

void Source::setSingleInstance(bool singleInstance) {
    if (singleInstance)
        m_flags |= kSingleInstance;
    else
        m_flags &= ~kSingleInstance;
}

void Source::setFilter(int filterHandle, Filter *filter) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    m_filters[filterHandle] = filter;
}

///! Audio
Audio::Audio(int flags)
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

    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
        neoFatal("failed to initialize audio subsystem `%s'", SDL_GetError());

    // find the appropriate audio driver
    const int driverCount = SDL_GetNumAudioDrivers();
    const u::string &checkDriver = snd_driver.get();
    if (checkDriver.size())
        u::print("[audio] => searching for driver `%s'\n", checkDriver);
    if (driverCount)
        u::print("[audio] => discovered %d %s\n",
            driverCount + 1, driverCount > 0 ? "drivers" : "driver");
    int driverSearch = checkDriver.empty() ? 0 : -1;
    for (int i = 0; i < driverCount; i++) {
        const char *name = SDL_GetAudioDriver(i);
        if (checkDriver.size()) {
            // found a match for the "configured" driver in init.cfg
            u::print("[audio] => found %s driver `%s'\n",
                name && name == checkDriver ? "matching" : "a", name);
            if (name && name == checkDriver)
                driverSearch = i;
        } else if (name) {
            u::print("[audio] => found driver `%s'\n", name);
        }
    }
    if (driverSearch == -1) {
        const char *fallback = SDL_GetCurrentAudioDriver();
        u::print("[audio] => failed to find driver `%s' (falling back to driver `%s')\n",
            fallback ? fallback : "unknown");
        if (fallback)
            snd_driver.set(fallback);
    }
    const char *driverName = SDL_GetAudioDriver(driverSearch);
    if (SDL_AudioInit(driverName))
        neoFatal("failed to initialize audio driver `%s'", driverName);
    snd_driver.set(driverName);
    u::print("[audio] => using driver `%s'\n", driverName);

    // find the appropriate device
    const int deviceCount = SDL_GetNumAudioDevices(0);
    const u::string &checkDevice = snd_device;
    if (checkDevice.size())
        u::print("[audio] => searching for device `%s'\n", checkDevice);
    if (deviceCount != -1) {
        u::print("[audio] => discovered %d playback %s\n",
            deviceCount, deviceCount > 1 ? "devices" : "device");
    }
    int deviceSearch = checkDevice.empty() ? 0 : -1;
    // report the devices
    for (int i = 0; i < deviceCount; i++) {
        const char *name = SDL_GetAudioDeviceName(i, 0);
        if (checkDevice.size()) {
            // found a match for the "configured" device in init.cfg
            u::print("[audio] => found %s device `%s'\n",
                name && name == checkDevice ? "matching" : "a", name);
            if (name && name == checkDevice)
                deviceSearch = i;
        } else if (name) {
            u::print("[audio] => found device `%s'\n", name);
        }
    }
    if (deviceSearch == -1) {
        const char *fallback = SDL_GetAudioDeviceName(0, 0);
        u::print("[audio] => failed to find device `%s' (falling back to device `%s')\n",
            snd_device.get(), fallback ? fallback : "unknown");
        deviceSearch = 0;
    }

    const char *deviceName = SDL_GetAudioDeviceName(deviceSearch, 0);

    SDL_AudioSpec wantFormat;
    wantFormat.freq = snd_frequency;
    wantFormat.format = AUDIO_S16;
    wantFormat.channels = 2;
    wantFormat.samples = snd_samples;
    wantFormat.callback = &audioMixer;
    wantFormat.userdata = (void *)this;

    SDL_AudioSpec haveFormat;
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(deviceName,
                                                   0,
                                                   &wantFormat,
                                                   &haveFormat,
                                                   0);

    if (device == 0)
        neoFatal("failed to initialize audio (%s)", SDL_GetError());

    // allocate mixer data and initialize the audio engine
    m_device = device;
    m_mutex = (void *)SDL_CreateMutex();
    m_mixerData = neoMalloc(sizeof(float) * haveFormat.samples*4);

    u::print("[audio] => device `%s' configured for %d channels @ %dHz (%d samples)\n",
        deviceName, haveFormat.channels, haveFormat.freq, haveFormat.samples);

    init(snd_voices, haveFormat.freq, haveFormat.samples * 2, flags);

    snd_device.set(deviceName);
    SDL_PauseAudioDevice(device, 0);
}

Audio::~Audio() {
    // pause the audio before shutting it down so SDL can shove off silence
    // into the audio device (this avoids audible noise at shutdown.)
    SDL_PauseAudioDevice(m_device, 1);
    // stop all sounds before shutting down the audio system
    stopAll();
    for (auto *it : m_filterInstances)
        delete it;
    // stop the thread
    SDL_CloseAudioDevice(m_device);
    // free the mixer data after shutting down the mixer thread
    neoFree(m_mixerData);
    // destroy the mutex after shutting down the mixer thread
    SDL_DestroyMutex((SDL_mutex*)m_mutex);
}

void Audio::init(int voices, int sampleRate, int bufferSize, int flags) {
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

float Audio::getPostClipScaler() const {
    LockGuard lock(m_mutex);
    return m_postClipScaler;
}

void Audio::setPostClipScaler(float scaler) {
    LockGuard lock(m_mutex);
    m_postClipScaler = scaler;
}

float Audio::getGlobalVolume() const {
    LockGuard lock(m_mutex);
    return m_globalVolume;
}

void Audio::setGlobalVolume(float volume) {
    LockGuard lock(m_mutex);
    m_globalVolumeFader.m_active = 0;
    m_globalVolume = volume;
}

int Audio::findFreeVoice() {
    size_t slat = 0xFFFFFFFF;
    int next = -1;
    for (size_t i = 0; i < m_voices.size(); i++) {
        if (!m_voices[i])
            return i;
        if (!(m_voices[i]->m_flags & SourceInstance::kProtected) && m_voices[i]->m_playIndex < slat) {
            slat = m_voices[i]->m_playIndex;
            next = int(i);
        }
    }
    U_ASSERT(next != -1);
    stopVoice(next);
    return next;
}

int Audio::play(Source &sound, float volume, float pan, bool paused, int lane) {
    // only one instance is allowed
    if (sound.m_flags & Source::kSingleInstance)
        stopSound(sound);

    u::unique_ptr<SourceInstance> instance(sound.create());
    sound.m_owner = this;

    locked(m_mutex) {
        const int voice = findFreeVoice();
        if (voice < 0)
            return -1;

        if (sound.m_sourceID == 0)
            sound.m_sourceID = m_sourceID++;
        m_voices[voice] = instance.release();
        m_voices[voice]->m_sourceID = sound.m_sourceID;
        m_voices[voice]->m_laneHandle = lane;
        m_voices[voice]->init(m_playIndex, sound.m_baseSampleRate, sound.m_channels, sound.m_flags);
        m_playIndex++;
        if (paused)
            m_voices[voice]->m_flags |= SourceInstance::kPaused;

        setVoicePan(voice, pan);
        setVoiceVolume(voice, volume);
        setVoiceRelativePlaySpeed(voice, 1.0f);

        for (size_t i = 0; i < kMaxStreamFilters; i++)
            if (sound.m_filters[i])
                m_voices[voice]->m_filters[i] = sound.m_filters[i]->create();

        const size_t scratchNeeded = size_t(m::ceil((m_voices[voice]->m_sampleRate / m_sampleRate) * m_bufferSize));
        if (m_scratchNeeded < scratchNeeded) {
            // calculate 1024 byte chunks needed for the scratch space
            size_t next = 1024;
            while (next < scratchNeeded) next <<= 1;
            m_scratchNeeded = next;
        }
        return getHandleFromVoice(voice);
    }
    return -1;
}

int Audio::getHandleFromVoice(int voice) const {
    return m_voices[voice] ? ((voice + 1) | (m_voices[voice]->m_playIndex << 12)) : 0;
}

int Audio::getVoiceFromHandle(int voiceHandle) const {
    if (voiceHandle <= 0)
        return -1;
    const int voice = (voiceHandle & 0xFFFF) - 1;
    const size_t index = voiceHandle >> 12;
    if (m_voices[voice] && (m_voices[voice]->m_playIndex & 0xFFFFF) == index)
        return voice;
    return -1;
}

float Audio::getVolume(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_volume.z;
}

float Audio::getStreamTime(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_streamTime;
}

float Audio::getRelativePlaySpeed(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 1.0f : m_voices[voice]->m_relativePlaySpeed;
}

void Audio::setVoiceRelativePlaySpeed(int voice, float speed) {
    if (m_voices[voice] && speed > 0.0f) {
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

void Audio::setRelativePlaySpeed(int voiceHandle, float speed) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_relativePlaySpeedFader.m_active = 0;
    setVoiceRelativePlaySpeed(voice, speed);
}

float Audio::getSampleRate(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_baseSampleRate;
}

void Audio::setSampleRate(int voiceHandle, float sampleRate) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_baseSampleRate = sampleRate;
    m_voices[voice]->m_sampleRate = sampleRate * m_voices[voice]->m_relativePlaySpeed;
}

void Audio::seek(int voiceHandle, float seconds) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->seek(seconds, &m_scratch[0], m_scratch.size());
}

void Audio::setPaused(int voiceHandle, bool paused) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    setVoicePaused(voice, paused);
}

void Audio::setPausedAll(bool paused) {
    LockGuard lock(m_mutex);
    for (size_t i = 0; i < m_voices.size(); i++)
        setVoicePaused(i, paused);
}

bool Audio::getPaused(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? false : !!(m_voices[voice]->m_flags & SourceInstance::kPaused);
}

bool Audio::getProtected(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? false : !!(m_voices[voice]->m_flags & SourceInstance::kProtected);
}

void Audio::setProtected(int voiceHandle, bool protect) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    if (protect)
        m_voices[voice]->m_flags |= SourceInstance::kProtected;
    else
        m_voices[voice]->m_flags &= ~SourceInstance::kProtected;
}

void Audio::setVoicePaused(int voice, bool paused) {
    if (m_voices[voice]) {
        m_voices[voice]->m_pauseScheduler.m_active = 0;
        if (paused)
            m_voices[voice]->m_flags |= SourceInstance::kPaused;
        else
            m_voices[voice]->m_flags &= ~SourceInstance::kPaused;
    }
}

void Audio::schedulePause(int voiceHandle, float time) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_pauseScheduler.lerp(1.0f, 0.0f, time, m_voices[voice]->m_streamTime);
}

void Audio::scheduleStop(int voiceHandle, float time) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_stopScheduler.lerp(1.0f, 0.0f, time, m_voices[voice]->m_streamTime);
}

void Audio::setVoicePan(int voice, float pan) {
    if (m_voices[voice]) {
        const m::vec2 panning = m::sincos((pan + 1.0f) * m::kPi / 4.0f);
        m_voices[voice]->m_pan = pan;
        m_voices[voice]->m_volume.x = panning.x;
        m_voices[voice]->m_volume.y = panning.y;
    }
}

float Audio::getPan(int voiceHandle) const {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    return voice == -1 ? 0.0f : m_voices[voice]->m_pan;
}

void Audio::setPan(int voiceHandle, float pan) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    setVoicePan(voice, pan);
}

void Audio::setPanAbsolute(int voiceHandle, const m::vec2 &panning) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_panFader.m_active = 0;
    m_voices[voice]->m_volume.x = panning.x;
    m_voices[voice]->m_volume.y = panning.y;
}

void Audio::setVoiceVolume(int voice, float volume) {
    if (m_voices[voice])
        m_voices[voice]->m_volume.z = volume;
}

void Audio::setVolume(int voiceHandle, float volume) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    m_voices[voice]->m_volumeFader.m_active = 0;
    setVoiceVolume(voice, volume);
}

void Audio::setGlobalFilter(int filterHandle, Filter *filter) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;

    locked(m_mutex) {
        delete m_filterInstances[filterHandle];
        m_filterInstances[filterHandle] = nullptr;

        m_filters[filterHandle] = filter;
        if (filter)
            m_filterInstances[filterHandle] = m_filters[filterHandle]->create();
    }
}

void Audio::stop(int voiceHandle) {
    LockGuard lock(m_mutex);
    const int voice = getVoiceFromHandle(voiceHandle);
    if (voice == -1)
        return;
    stopVoice(voice);
}

void Audio::stopVoice(int voice) {
    if (m_voices[voice]) {
        // To prevent recursion issues: load into temporary first
        SourceInstance *instance = m_voices[voice];
        m_voices[voice] = nullptr;
        delete instance;
    }
}

void Audio::stopSound(Source &sound) {
    LockGuard lock(m_mutex);
    if (sound.m_sourceID) {
        for (size_t i = 0; i < m_voices.size(); i++)
            if (m_voices[i] && m_voices[i]->m_sourceID == sound.m_sourceID)
                stopVoice(i);
    }
}

void Audio::setFilterParam(int voiceHandle, int filterHandle, int attrib, float value) {
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (voiceHandle == 0) locked(m_mutex) {
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->setFilterParam(attrib, value);
        return;
    }
    locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        if (m_voices[voice] && m_voices[voice]->m_filters[filterHandle])
            m_voices[voice]->m_filters[filterHandle]->setFilterParam(attrib, value);
    }
}

void Audio::fadeFilterParam(int voiceHandle,
                            int filterHandle,
                            int attrib,
                            float from,
                            float to,
                            float time)
{
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (voiceHandle == 0) locked(m_mutex) {
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->fadeFilterParam(attrib, from, to, time, m_streamTime);
        return;
    }
    locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        if (m_voices[voice] && m_voices[voice]->m_filters[filterHandle])
            m_voices[voice]->m_filters[filterHandle]->fadeFilterParam(attrib, from, to, time, m_streamTime);
    }
}

void Audio::oscFilterParam(int voiceHandle,
                           int filterHandle,
                           int attrib,
                           float from,
                           float to,
                           float time)
{
    if (filterHandle < 0 || filterHandle >= kMaxStreamFilters)
        return;
    if (voiceHandle == 0) locked(m_mutex) {
        if (m_filterInstances[filterHandle])
            m_filterInstances[filterHandle]->oscFilterParam(attrib, from, to, time, m_streamTime);
        return;
    }
    locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        if (m_voices[voice] && m_voices[voice]->m_filters[filterHandle])
            m_voices[voice]->m_filters[filterHandle]->oscFilterParam(attrib, from, to, time, m_streamTime);
    }
}

void Audio::stopAll() {
    LockGuard lock(m_mutex);
    for (size_t i = 0; i < m_voices.size(); i++)
        stopVoice(i);
}

void Audio::fadeVolume(int voiceHandle, float to, float time) {
    const float from = getVolume(voiceHandle);
    if (time <= 0.0f || to == from) {
        setVolume(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        m_voices[voice]->m_volumeFader.lerp(from, to, time, m_voices[voice]->m_streamTime);
    }
}

void Audio::fadePan(int voiceHandle, float to, float time) {
    const float from = getPan(voiceHandle);
    if (time <= 0.0f || to == from) {
        setPan(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        m_voices[voice]->m_panFader.lerp(from, to, time, m_voices[voice]->m_streamTime);
    }
}

void Audio::fadeRelativePlaySpeed(int voiceHandle, float to, float time) {
    const float from = getRelativePlaySpeed(voiceHandle);
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        m_voices[voice]->m_relativePlaySpeedFader.lerp(from, to, time, m_voices[voice]->m_streamTime);
    }
}

void Audio::fadeGlobalVolume(float to, float time) {
    const float from = getGlobalVolume();
    if (time <= 0.0f || to == from) {
        setGlobalVolume(to);
        return;
    } else locked(m_mutex) {
        m_streamTime = 0.0f; // avoid ~6 day rollover
        m_globalVolumeFader.lerp(from, to, time, m_streamTime);
    }
}

void Audio::oscVolume(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setVolume(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        m_voices[voice]->m_volumeFader.lfo(from, to, time, m_voices[voice]->m_streamTime);
    }
}

void Audio::oscPan(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setPan(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        m_voices[voice]->m_panFader.lfo(from, to, time, m_voices[voice]->m_streamTime);
    }
}

void Audio::oscRelativePlaySpeed(int voiceHandle, float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setRelativePlaySpeed(voiceHandle, to);
        return;
    } else locked(m_mutex) {
        const int voice = getVoiceFromHandle(voiceHandle);
        if (voice == -1)
            return;
        m_voices[voice]->m_relativePlaySpeedFader.lfo(from, to, time, m_voices[voice]->m_streamTime);
    }
}

void Audio::oscGlobalVolume(float from, float to, float time) {
    if (time <= 0.0f || to == from) {
        setGlobalVolume(to);
        return;
    } else locked(m_mutex) {
        m_streamTime = 0.0f; // avoid ~6 day rollover
        m_globalVolumeFader.lfo(from, to, time, m_streamTime);
    }
}

void Audio::mixLane(float *buffer, size_t samples, float *scratch, int lane) {
    // clear accumulation
    memset(buffer, 0, sizeof(float) * samples * 2);

    // accumulate sources
    for (size_t i = 0; i < m_voices.size(); i++) {
        if (m_voices[i] == nullptr)
            continue;
        if (m_voices[i]->m_laneHandle != lane)
            continue;
        if (m_voices[i]->m_flags & SourceInstance::kPaused)
            continue;

        float step = 0.0f;
        const float next = m_voices[i]->m_sampleRate / m_sampleRate;
        const size_t read = m::ceil(samples * next);

        m_voices[i]->getAudio(scratch, read);

        for (size_t j = 0; j < kMaxStreamFilters; j++) {
            if (m_voices[i]->m_filters[j] == nullptr)
                continue;
            m_voices[i]->m_filters[j]->filter(scratch,
                                              read,
                                              m_voices[i]->m_channels,
                                              m_voices[i]->m_sampleRate,
                                              m_streamTime);
        }

        if (m_voices[i]->m_activeFader) {
            float panL = m_voices[i]->m_faderVolume[0];
            float panR = m_voices[i]->m_faderVolume[2];
            const float incL = (m_voices[i]->m_faderVolume[1] - m_voices[i]->m_faderVolume[0]) / samples;
            const float incR = (m_voices[i]->m_faderVolume[3] - m_voices[i]->m_faderVolume[2]) / samples;

            if (m_voices[i]->m_channels == 2) {
                for (size_t j = 0; j < samples; j++, step += next, panL += incL, panR += incR) {
                    const float sampleL = scratch[(int)m::floor(step)];
                    const float sampleR = scratch[(int)m::floor(step) + samples];
                    buffer[j + samples*0] += sampleL * panL;
                    buffer[j + samples*1] += sampleR * panR;
                }
            } else {
                for (size_t j = 0; j < samples; j++, step += next, panL += incL, panR += incR) {
                    const float sampleM = scratch[(int)m::floor(step)];
                    buffer[j + samples*0] += sampleM * panL;
                    buffer[j + samples*1] += sampleM * panR;
                }
            }
        } else {
            const float panL = m_voices[i]->m_volume.x * m_voices[i]->m_volume.z;
            const float panR = m_voices[i]->m_volume.y * m_voices[i]->m_volume.z;
            if (m_voices[i]->m_channels == 2) {
                for (size_t j = 0; j < samples; j++, step += next) {
                    const float sampleL = scratch[(int)m::floor(step)];
                    const float sampleR = scratch[(int)m::floor(step) + samples];
                    buffer[j + samples*0] += sampleL * panL;
                    buffer[j + samples*1] += sampleR * panR;
                }
            } else {
                for (size_t j = 0; j < samples; j++, step += next) {
                    const float sampleM = scratch[(int)m::floor(step)];
                    buffer[j + samples*0] += sampleM * panL;
                    buffer[j + samples*1] += sampleM * panR;
                }
            }
        }

        // clear is sound is over
        if (!(m_voices[i]->m_flags & SourceInstance::kLooping) && m_voices[i]->hasEnded())
            stopVoice(i);
    }
}

void Audio::mix(float *buffer, size_t samples) {
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
            if (!m_voices[i] || (m_voices[i]->m_flags & SourceInstance::kPaused))
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
                m_voices[i]->m_faderVolume[0*2+0] = panL.x * volume.x;
                m_voices[i]->m_faderVolume[0*2+1] = panL.y * volume.y;
                m_voices[i]->m_faderVolume[1*2+0] = panR.x * volume.x;
                m_voices[i]->m_faderVolume[1*2+1] = panR.y * volume.y;
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

        mixLane(buffer, samples, &m_scratch[0], 0);

        // global filter
        for (size_t i = 0; i < kMaxStreamFilters; i++) {
            if (m_filterInstances[i])
                m_filterInstances[i]->filter(&buffer[0], samples, 2, m_sampleRate, m_streamTime);
        }
    }

    clip(buffer, &m_scratch[0], samples, m_globalVolume);
    interlace(&m_scratch[0], buffer, samples, 2);
}

void Audio::clip(const float *U_RESTRICT src,
                 float *U_RESTRICT dst,
                 size_t samples,
                 const m::vec2 &volume)
{
    // clip
    float volumeNext = volume.x;
    float volumeStep = (volume.y - volume.x) / samples;

    if (m_flags & kClipRoundOff) {
        // round off clipping is less aggressive
        for (size_t j = 0, c = 0; j < 2; j++) {
            volumeNext = volume.x;
            for (size_t i = 0; i < samples; i++, c++, volumeNext += volumeStep) {
                float sample = src[c] * volumeNext;
                /**/ if (sample <= -1.65f) sample = -0.9862875f;
                else if (sample >=  1.65f) sample =  0.9862875f;
                else                       sample =  0.87f * sample - 0.1f * sample * sample * sample;
                dst[c] = sample * m_postClipScaler;
            }
        }
    } else {
        // standard clipping may introduce noise and aliasing - need proper
        // hi-pass filter to prevent this
        for (size_t j = 0, c = 0; j < 2; j++) {
            volumeNext = volume.x;
            for (size_t i = 0; i < samples; i++, c++, volumeNext += volumeStep)
                dst[i] = m::clamp(src[i] * volumeNext, -1.0f, 1.0f) * m_postClipScaler;
        }
    }
}

void Audio::interlace(const float *U_RESTRICT src,
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
