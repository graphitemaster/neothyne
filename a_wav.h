#ifndef A_WAV_HDR
#define A_WAV_HDR

#include "a_system.h"

namespace a {

struct wav;
struct wavProducer : producer {
    wavProducer(wav *parent);
    virtual void getAudio(float *buffer, size_t samples);
    virtual bool hasEnded() const;
private:
    wav *m_parent;
    size_t m_offset;
};

struct wav : factory {
    wav();
    virtual ~wav();
    virtual producer *create();
    bool load(const char *file, int channel);
private:
    friend struct wavProducer;
    float *m_data;
    size_t m_samples;
};

}

#endif
