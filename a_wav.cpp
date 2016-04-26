#include "engine.h"

#include "a_wav.h"
#include "u_file.h"

namespace a {

wavProducer::wavProducer(wav *parent)
    : m_parent(parent)
    , m_offset(0)
{
}

void wavProducer::getAudio(float *buffer, size_t samples) {
    if (m_parent->m_data == nullptr)
        return;

    size_t copySize = samples;
    if (copySize + m_offset > m_parent->m_samples)
        copySize = m_parent->m_samples - m_offset;

    const size_t channels = m_flags & kStereo ? 2 : 1;

    memcpy(buffer, m_parent->m_data + m_offset * channels, sizeof(float) * copySize * channels);
    if (copySize != samples) {
        if (m_flags & producer::kLooping) {
            memcpy(buffer + copySize * channels, m_parent->m_data, sizeof(float) * (samples - copySize) * channels);
            m_offset = samples - copySize;
        } else {
            memset(buffer + copySize * channels, 0, sizeof(float) * (samples - copySize) * channels);
            m_offset += samples - copySize;
        }
    } else {
        m_offset += samples;
    }
}

bool wavProducer::hasEnded() const {
    return m_offset >= m_parent->m_samples;
}

wav::wav()
    : m_data(nullptr)
{
}

wav::~wav() {
    neoFree(m_data);
}

bool wav::load(const char *file, int channel) {
    m_samples = 0;
    auto read = u::read(neoGamePath() + file, "rb");
    if (!read) {
        u::print("NOT FOUND!\n");
        return false;
    }

    const auto &data = *read;
    const unsigned char *cursor = &data[0];

    // TODO: util
    auto fourCC = [](const char (&abcd)[5]) -> int32_t {
        return (abcd[3] << 24) | (abcd[2] << 16) | (abcd[1] << 8) | abcd[0];
    };

    // TODO: utilify
    auto read32 = [](const unsigned char **cursor) { int32_t i; memcpy(&i, *cursor, sizeof i); *cursor += sizeof i; return i; };
    auto read16 = [](const unsigned char **cursor) { int16_t i; memcpy(&i, *cursor, sizeof i); *cursor += sizeof i; return i; };
    auto read8  = [](const unsigned char **cursor) { int8_t i;  memcpy(&i, *cursor, sizeof i); *cursor += sizeof i; return i; };

    if (read32(&cursor) != fourCC("RIFF"))
        return false;
    read32(&cursor);
    if (read32(&cursor) != fourCC("WAVE"))
        return false;
    if (read32(&cursor) != fourCC("fmt "))
        return false;
    int subChunkSize = read32(&cursor);
    int audioFormat = read16(&cursor);
    int channels = read16(&cursor);
    int sampleRate = read32(&cursor);
    read32(&cursor);
    read16(&cursor);
    int bitsPerSample = read16(&cursor);
    if (audioFormat != 1 || subChunkSize != 16 || (bitsPerSample != 8 && bitsPerSample != 16)) {
        // Unsupported
        return false;
    }
    int chunk = read32(&cursor);
    if (chunk == fourCC("LIST")) {
        size_t size = read32(&cursor);
        for (size_t i = 0; i < size; i++)
            read8(&cursor);
        chunk = read32(&cursor);
    }
    if (chunk != fourCC("data"))
        return false;

    int readChannels = 1;

    if (channels > 1) {
        readChannels = 2;
        m_flags |= kStereo;
    }

    int subDataChunkSize = read32(&cursor);
    size_t samples = (subDataChunkSize / (bitsPerSample / 8)) / channels;
    m_data = neoMalloc(sizeof(float) * samples * readChannels);
    if (bitsPerSample == 8) {
        for (size_t i = 0; i < samples; i++) {
            for (int j = 0; j < channels; j++) {
                if (j == channel) {
                    m_data[i * readChannels] = read8(&cursor) / float(0x80);
                } else {
                    if (readChannels > 1 && j == channel + 1) {
                        m_data[i * readChannels + 1] = read8(&cursor) / float(0x80);
                    } else {
                        read8(&cursor);
                    }
                }
            }
        }
    } else if (bitsPerSample == 16) {
        for (size_t i = 0; i < samples; i++) {
            for (int j = 0; j < channels; j++) {
                if (j == channel) {
                    m_data[i * readChannels] = read16(&cursor) / float(0x8000);
                } else {
                    if (readChannels > 1 && j == channel + 1) {
                        m_data[i * readChannels + 1] = read16(&cursor) / float(0x8000);
                    } else {
                        read16(&cursor);
                    }
                }
            }
        }
    }
    m_baseSampleRate = sampleRate;
    m_samples = samples;
    return true;
}

producer *wav::create() {
    return new wavProducer(this);
}

}
