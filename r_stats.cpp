#include <float.h>

#include "engine.h"
#include "gui.h"
#include "cvar.h"

#include "r_stats.h"

NVAR(int, r_stats, "rendering statistics", 0, 1, 1);
NVAR(int, r_stats_histogram, "rendering statistics histogram", 0, 1, 1);
NVAR(int, r_stats_histogram_duration, "duration in seconds to collect histogram samples", 1, 10, 2);
NVAR(float, r_stats_histogram_size, "size of histogram in screen width percentage", 0.25f, 1.0f, 0.5f);

namespace r {

u::map<const char *, stat> stat::m_stats;
u::vector<float> stat::m_histogram;

static constexpr size_t kSpace = 20u;

void stat::render(size_t x) {
    if (r_stats) {
        // calculate total vertical space needed
        size_t space = kSpace;
        for (const auto &it : m_stats)
            space += it.second.space();

        if (r_stats_histogram) {
            space += kSpace;   // 1 for "Histogram" test
            space += kSpace*2; // 2 for "Histogram" bars
        }

        // shift up by vertical space
        size_t next = space;
        for (const auto &it : m_stats)
            next = it.second.draw(x, next);

        if (r_stats_histogram) {
            // draw histogram
            gui::drawText(x, next, gui::kAlignLeft, "Histogram", gui::RGBA(255, 255, 0));
            next -= kSpace*2;
            // get minimum and maximum value in histogram
            static m::vec2 range(FLT_MIN, FLT_MAX);
            for (auto &it : m_histogram) {
                range.x = u::max(range.x, it);
                range.y = u::min(range.y, it);
            }
            if (range.x != 0.0f && range.y != 0.0f) {
                const m::vec3 red(1.0f, 0.0f, 0.0f);
                const m::vec3 white(1.0f, 1.0f, 1.0f);
                const float sampleWidth = ((neoWidth()-kSpace*4) * r_stats_histogram_size) / m_histogram.size(); // to the center of the screen
                for (size_t i = 0; i < m_histogram.size(); i++) {
                    const auto &it = m_histogram[i];
                    const float scaledSample = it / range.x; // 16ms is max
                    const m::vec3 color = red*scaledSample+white*(1.0f-scaledSample);
                    const auto rgba = gui::RGBA(color.x*255.0f, color.y*255.0f, color.z*255.0f);
                    // TODO: make faster by updating a texture instead of drawing thousands of tiny rectangles
                    gui::drawRectangle(x + kSpace + sampleWidth*i, next-kSpace+5, sampleWidth, kSpace*2*scaledSample, rgba);
                }
            }
        }
        // always collect samples even if r_stats_histogram is not enabled
        // this way if someone toggles it on the previous samples are immediately
        // available.
        const frameTimer &timer = neoFrameTimer();
        m_histogram.push_back(timer.mspf());
        if (m_histogram.size() > size_t(timer.fps()*r_stats_histogram_duration))
            m_histogram.erase(m_histogram.begin(), m_histogram.begin()+1);
    }
}

stat *stat::get(const char *name) {
    auto find = m_stats.find(name);
    U_ASSERT(find != m_stats.end());
    return &find->second;
}

stat *stat::add(const char *name, const char *description) {
    auto find = m_stats.find(name);
    if (find == m_stats.end())
        find = m_stats.insert( { name, { name, description } } ).first;
    else
        find->second.m_instances++;
    return &find->second;
}

size_t stat::space() const {
    // calculate how much vertical space this is going to take:
    size_t space = kSpace;
    if (m_instances > 1)    space += kSpace;
    if (m_vboMemory)        space += kSpace;
    if (m_iboMemory)        space += kSpace;
    if (m_textureCount)     space += kSpace * (m_textureCount > 1 ? 3 : 1);
    return space;
}

size_t stat::draw(size_t x, size_t y) const {
    const auto color = gui::RGBA(255,255,255);
    gui::drawText(x, y, gui::kAlignLeft, u::format("%s", description()).c_str(), gui::RGBA(255, 255, 0));
    y -= kSpace;
    if (m_instances > 1) {
        // Sometimes there are multiple instances
        gui::drawText(x + kSpace, y, gui::kAlignLeft, u::format("Instances: %zu", m_instances).c_str(), color);
        y -= kSpace;
    }
    if (m_vboMemory) {
        gui::drawText(x + kSpace, y, gui::kAlignLeft, u::format("Vertex Memory: %s", u::sizeMetric(m_vboMemory)).c_str(), color);
        y -= kSpace;
    }
    if (m_iboMemory) {
        gui::drawText(x + kSpace, y, gui::kAlignLeft, u::format("Index Memory: %s", u::sizeMetric(m_iboMemory)).c_str(), color);
        y -= kSpace;
    }
    if (m_textureCount > 1) {
        // Multiple textures: indicate count and total memory usage
        gui::drawText(x + kSpace, y, gui::kAlignLeft, "Textures:", color);
        y -= kSpace;
        gui::drawText(x + 40, y, gui::kAlignLeft, u::format("Count: %zu", m_textureCount).c_str(), color);
        y -= kSpace;
        gui::drawText(x + 40, y, gui::kAlignLeft, u::format("Memory: %s", u::sizeMetric(m_textureMemory)).c_str(), color);
        y -= kSpace;
    } else if (m_textureCount) {
        // Single texture: has no count just indicate texture memory (memory of the single texture)
        gui::drawText(x + kSpace, y, gui::kAlignLeft, u::format("Texture Memory: %s", u::sizeMetric(m_textureMemory)).c_str(), color);
        y -= kSpace;
    }
    return y;
}

}
