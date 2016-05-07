#ifndef A_WAV_HDR
#define A_WAV_HDR

#include "a_system.h"
#include "u_file.h"

namespace a {

struct wav;
struct wavInstance final : sourceInstance {
    wavInstance(wav *parent);
    virtual void getAudio(float *buffer, size_t samples) final;
    virtual bool rewind() final;
    virtual bool hasEnded() const final;
    virtual ~wavInstance() final;
private:
    wav *m_parent;
    u::file m_file;
    size_t m_offset;
};

struct wav final : source {
    wav();
    virtual ~wav() final;
    bool load(const char *fileName);
    virtual sourceInstance *create() final;

private:
    friend struct wavInstance;

    bool load(u::file &fp);
    u::string m_fileName;
    size_t m_dataOffset;
    size_t m_bits;
    size_t m_channels;
    size_t m_sampleCount;
};

}

#endif
