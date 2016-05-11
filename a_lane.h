#ifndef A_LANE_HDR
#define A_LANE_HDR
#include "a_system.h"

namespace a {

struct lane;
struct laneInstance final : sourceInstance {
    laneInstance(lane *parent);
    virtual void getAudio(float *buffer, size_t samples) final;
    virtual bool hasEnded() const final;
    virtual ~laneInstance() final;

private:
    friend struct lane;
    lane* m_parent;
    u::vector<float> m_scratch;
};

struct lane final : source {
    lane();
    virtual laneInstance *create() final;
    virtual void setFilter(int filterHandle, filter *filter_) final;
    int play(source &sound, float volume = 1.0f, float pan = 0.0f, bool paused = false);

private:
    friend struct laneInstance;
    int m_channelHandle;
    laneInstance *m_instance;
};

}

#endif
