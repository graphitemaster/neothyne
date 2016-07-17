#ifndef A_WAV_HDR
#define A_WAV_HDR

#include "a_system.h"
#include "u_file.h"

namespace a {

struct WAVFile;
struct WAVFileInstance final : SourceInstance {
    WAVFileInstance(WAVFile *parent);
    virtual void getAudio(float *buffer, size_t samples) final;
    virtual bool rewind() final;
    virtual bool hasEnded() const final;
    virtual ~WAVFileInstance() final;
private:
    WAVFile *m_parent;
    u::file m_file;
    size_t m_offset;
};

struct WAVFile final : Source {
    WAVFile();
    virtual ~WAVFile() final;
    bool load(const char *fileName);
    virtual SourceInstance *create() final;

private:
    friend struct WAVFileInstance;

    bool load(u::file &fp);
    u::string m_fileName;
    size_t m_dataOffset;
    size_t m_bits;
    size_t m_sampleCount;
};

}

#endif
