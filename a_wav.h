#ifndef A_WAV_HDR
#define A_WAV_HDR

#include "a_system.h"
#include "u_file.h"

namespace a {

struct wav;
struct wavInstance : instance {
    wavInstance(wav *parent);
    virtual void getAudio(float *buffer, size_t samples);
    virtual bool rewind();
    virtual bool hasEnded() const;
    virtual ~wavInstance();
private:
    wav *m_parent;
    u::file m_file;
    size_t m_offset;
};

struct wav : source {
    wav();
    virtual ~wav();
    bool load(const char *fileName, bool stereo = true, size_t channel_ = 0);
    virtual instance *create();

private:
    friend struct wavInstance;

    bool load(u::file &fp, bool stereo = true, size_t channel_ = 0);
    u::string m_fileName;
    size_t m_dataOffset;
    size_t m_bits;
    size_t m_channels;
    size_t m_channelOffset;
    size_t m_sampleCount;
};

}

#endif
