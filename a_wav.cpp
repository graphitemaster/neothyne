#include "engine.h"

#include "a_wav.h"
#include "u_file.h"

namespace a {

wavInstance::wavInstance(wav *parent)
    : m_parent(parent)
    , m_file(u::fopen(m_parent->m_fileName, "rb"))
    , m_offset(0)
{
    if (m_file) {
        int seek = fseek(m_file, m_parent->m_dataOffset, SEEK_SET);
        U_ASSERT(seek == 0);
    }
}

wavInstance::~wavInstance() {
    // Empty
}

template <typename T>
static inline T read(u::file &fp) {
    T value;
    (void)fread(&value, 1, sizeof value, fp);
    return value;
}

// convenience function for reading stream data
static void readData(u::file &fp,
                     float *buffer,
                     size_t samples,
                     size_t pitch,
                     size_t channels,
                     size_t srcChannels,
                     size_t bits)
{
    switch (bits) {
    case 8:
        for (size_t i = 0; i < samples; i++) {
            for (size_t j = 0; j < srcChannels; j++) {
                const auto sample = read<int8_t>(fp) / float(0x80);
                if (j == 0)
                    buffer[i] = sample;
                else if (channels > 1 && j == 1)
                    buffer[i + pitch] = sample;
            }
        }
        break;
    case 16:
        for (size_t i = 0; i < samples; i++) {
            for (size_t j = 0; j < srcChannels; j++) {
                const auto sample = read<int16_t>(fp) / float(0x8000);
                if (j == 0)
                    buffer[i] = sample;
                else if (channels > 1 && j == 1)
                    buffer[i + pitch] = sample;
            }
        }
        break;
    }
}

void wavInstance::getAudio(float *buffer, size_t samples) {
    if (!m_file) return;

    const size_t channels = m_channels;
    size_t copySize = samples;
    if (copySize + m_offset > m_parent->m_sampleCount)
        copySize = m_parent->m_sampleCount - m_offset;

    readData(m_file, buffer, copySize, samples, channels, m_parent->m_channels, m_parent->m_bits);

    if (copySize == samples) {
        m_offset = samples;
        return;
    }

    if (m_flags & sourceInstance::kLooping) {
        int seek = fseek(m_file, m_parent->m_dataOffset, SEEK_SET);
        U_ASSERT(seek == 0);
        readData(m_file, buffer + copySize, samples - copySize, samples,
            channels, m_parent->m_channels, m_parent->m_bits);
        m_offset = samples - copySize;
    } else {
        for (size_t i = 0; i < channels; i++)
            memset(buffer + copySize + i * samples, 0, sizeof(float) * (samples - copySize));
        m_offset += samples;
    }
}

bool wavInstance::rewind() {
    if (m_file) {
        if (fseek(m_file, m_parent->m_dataOffset, SEEK_SET) != 0)
            return false;
    }
    m_offset = 0;
    m_streamTime = 0.0f;
    return true;
}

bool wavInstance::hasEnded() const {
    return !(m_flags & sourceInstance::kLooping) && m_offset >= m_parent->m_sampleCount;
}

///! wav
wav::wav()
    : m_dataOffset(0)
    , m_bits(0)
    , m_sampleCount(0)
{
}

wav::~wav() {
    // Empty
}

bool wav::load(u::file &fp) {
    read<int32_t>(fp);

    if (read<int32_t>(fp) != u::fourCC<int32_t>("WAVE"))
        return false;
    int32_t chunk = read<int32_t>(fp);

    // may be aligned with a JUNK section
    if (chunk == u::fourCC<int32_t>("JUNK")) {
        int32_t size = read<int32_t>(fp);
        if (size & 1)
            size++;
        for (int32_t i = 0; i < size; i++)
            read<int8_t>(fp);
        chunk = read<int32_t>(fp);
    }

    if (chunk != u::fourCC<int32_t>("fmt "))
        return false;

    const auto subFirstChunkSize = read<int32_t>(fp);
    const auto audioFormat = read<int16_t>(fp);
    const auto channels = read<int16_t>(fp);
    const auto sampleRate = read<int32_t>(fp);

    read<int32_t>(fp);
    read<int16_t>(fp);

    const auto bitsPerSample = read<int16_t>(fp);

    if (audioFormat != 1 || subFirstChunkSize != 16
        || (bitsPerSample != 8 && bitsPerSample != 16))
        return false;

    chunk = read<int32_t>(fp);
    if (chunk == u::fourCC<int32_t>("LIST")) {
        const auto listSize = read<int32_t>(fp);
        for (int32_t i = 0; i < listSize; i++)
            read<int8_t>(fp);
        chunk = read<int32_t>(fp);
    }

    if (chunk != u::fourCC<int32_t>("data"))
        return false;

    m_channels = channels;
    if (channels > 1)
        m_channels = 2;

    const auto subSecondChunkSize = read<int32_t>(fp);
    const auto samples = (subSecondChunkSize / (bitsPerSample / 8)) / channels;

    m_dataOffset = ftell(fp);
    m_bits = bitsPerSample;
    m_baseSampleRate = sampleRate;
    m_sampleCount = samples;

    return true;
}

bool wav::load(const char *fileName) {
    m_sampleCount = 0;
    m_fileName = neoGamePath() + fileName;
    u::file fp = u::fopen(m_fileName, "rb");
    if (fp) {
        const int32_t tag = read<int32_t>(fp);
        if (tag != u::fourCC<int32_t>("RIFF"))
            return false;
        return load(fp);
    }
    return false;
}

sourceInstance *wav::create() {
    return new wavInstance(this);
}

}
