#ifndef A_LANE_HDR
#define A_LANE_HDR
#include "a_system.h"

namespace a {

struct Lane;
struct LaneInstance final : SourceInstance {
    LaneInstance(Lane *parent);
    virtual void getAudio(float *buffer, size_t samples) final;
    virtual bool hasEnded() const final;
    virtual ~LaneInstance() final;

private:
    friend struct Lane;
    Lane* m_parent;
    u::vector<float> m_scratch;
};

struct Lane final : Source {
    Lane();
    virtual LaneInstance *create() final;
    virtual void setFilter(int filterHandle, Filter *filter) final;
    int play(Source &sound, float volume = 1.0f, float pan = 0.0f, bool paused = false);

private:
    friend struct LaneInstance;
    int m_channelHandle;
    LaneInstance *m_instance;
};

}

#endif
