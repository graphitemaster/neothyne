#include "engine.h"

#include "a_wav.h"
#include "u_file.h"
#include "u_optional.h"

namespace a {

WAVFileInstance::WAVFileInstance(WAVFile *parent)
    : m_parent(parent)
    , m_file(u::fopen(m_parent->m_fileName, "rb"))
    , m_offset(0)
{
    if (m_file) {
        int seek = fseek(m_file, m_parent->m_dataOffset, SEEK_SET);
        U_ASSERT(seek == 0);
    }
}

WAVFileInstance::~WAVFileInstance() {
    // Empty
}

template <typename T>
static inline bool read(u::file &fp, T &value) {
    bool result = fread(&value, sizeof value, 1, fp) == 1;
    if (result) {
        value = u::endianSwap(value);
        return true;
    }
    return false;
}

// convenience function for reading stream data
static bool readData(u::file &fp,
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
                int8_t load;
                if (!read(fp, load))
                    return false;
                const float sample = load / float(0x80);
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
                int16_t load;
                if (!read(fp, load))
                    return false;
                const float sample = load / float(0x8000);
                if (j == 0)
                    buffer[i] = sample;
                else if (channels > 1 && j == 1)
                    buffer[i + pitch] = sample;
            }
        }
        break;
    }
    return true;
}

void WAVFileInstance::getAudio(float *buffer, size_t samples) {
    if (!m_file) return;

    const size_t channels = m_channels;
    size_t copySize = samples;
    if (copySize + m_offset > m_parent->m_sampleCount)
        copySize = m_parent->m_sampleCount - m_offset;

    if (!readData(m_file, buffer, copySize, samples, channels, m_parent->m_channels, m_parent->m_bits))
        return;

    if (copySize == samples) {
        m_offset = samples;
        return;
    }

    if (m_flags & SourceInstance::kLooping) {
        int seek = fseek(m_file, m_parent->m_dataOffset, SEEK_SET);
        U_ASSERT(seek == 0);
        if (!readData(m_file, buffer + copySize, samples - copySize, samples,
            channels, m_parent->m_channels, m_parent->m_bits)) return;
        m_offset = samples - copySize;
    } else {
        for (size_t i = 0; i < channels; i++)
            memset(buffer + copySize + i * samples, 0, sizeof(float) * (samples - copySize));
        m_offset += samples;
    }
}

bool WAVFileInstance::rewind() {
    if (m_file) {
        if (fseek(m_file, m_parent->m_dataOffset, SEEK_SET) != 0)
            return false;
    }
    m_offset = 0;
    m_streamTime = 0.0f;
    return true;
}

bool WAVFileInstance::hasEnded() const {
    return !(m_flags & SourceInstance::kLooping) && m_offset >= m_parent->m_sampleCount;
}

///! WAVFile
WAVFile::WAVFile()
    : m_dataOffset(0)
    , m_bits(0)
    , m_sampleCount(0)
{
}

WAVFile::~WAVFile() {
    // Empty
}

bool WAVFile::load(u::file &fp) {
    int32_t skip4;
    int16_t skip2;
    int8_t skip1;

    if (!read(fp, skip4))
        return false;

    int32_t fourcc;
    if (!read(fp, fourcc))
        return false;
    if (fourcc != u::fourCC<int32_t>("WAVE"))
        return false;

    int32_t chunk;
    if (!read(fp, chunk))
        return false;

    // may be aligned with a JUNK section
    if (chunk == u::fourCC<int32_t>("JUNK")) {
        int32_t size;
        if (!read(fp, size))
            return false;
        if (size & 1)
            size++;
        for (int32_t i = 0; i < size; i++) {
            if (!read(fp, skip1))
                return false;
        }
        if (!read(fp, chunk))
            return false;
    }

    if (chunk != u::fourCC<int32_t>("fmt "))
        return false;

    int32_t subFirstChunkSize;
    if (!read(fp, subFirstChunkSize))
        return false;
    int16_t audioFormat;
    if (!read(fp, audioFormat))
        return false;
    int16_t channels;
    if (!read(fp, channels))
        return false;
    int32_t sampleRate;
    if (!read(fp, sampleRate))
        return false;
    if (!read(fp, skip4))
        return false;
    if (!read(fp, skip2))
        return false;

    int16_t bitsPerSample;
    if (!read(fp, bitsPerSample))
        return false;

    if (audioFormat != 1 || subFirstChunkSize != 16
        || (bitsPerSample != 8 && bitsPerSample != 16))
        return false;

    if (!read(fp, chunk))
        return false;
    if (chunk == u::fourCC<int32_t>("LIST")) {
        int32_t listSize;
        if (!read(fp, listSize))
            return false;
        for (int32_t i = 0; i < listSize; i++)
            if (!read(fp, skip1))
                return false;
        if (!read(fp, chunk))
            return false;
    }

    if (chunk != u::fourCC<int32_t>("data"))
        return false;

    m_channels = channels;
    if (m_channels > 1)
        m_channels = 2;

    int32_t subSecondChunkSize;
    if (!read(fp, subSecondChunkSize))
        return false;

    const auto samples = (subSecondChunkSize / (bitsPerSample / 8)) / channels;

    m_dataOffset = ftell(fp);
    m_bits = bitsPerSample;
    m_baseSampleRate = sampleRate;
    m_sampleCount = samples;

    return true;
}

bool WAVFile::load(const char *fileName) {
    m_sampleCount = 0;
    m_fileName = neoGamePath() + fileName;
    u::file fp = u::fopen(m_fileName, "rb");
    if (fp) {
        int32_t tag;
        if (!read(fp, tag))
            return false;
        if (tag != u::fourCC<int32_t>("RIFF"))
            return false;
        return load(fp);
    }
    return false;
}

SourceInstance *WAVFile::create() {
    return new WAVFileInstance(this);
}

}
