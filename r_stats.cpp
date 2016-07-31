#include <float.h>

#include "engine.h"
#include "texture.h"
#include "gui.h"
#include "c_variable.h"

#include "r_common.h"
#include "r_stats.h"

NVAR(int, r_stats, "rendering statistics", 0, 1, 1);
NVAR(int, r_stats_gpu_meminfo, "show GPU memory info if supported", 0, 1, 1);
NVAR(int, r_stats_histogram, "rendering statistics histogram", 0, 1, 1);
NVAR(int, r_stats_histogram_duration, "duration in seconds to collect histogram samples", 1, 10, 2);
NVAR(float, r_stats_histogram_size, "size of histogram in screen width percentage", 0.25f, 1.0f, 0.5f);
NVAR(float, r_stats_histogram_max, "maximum mspf to base histogram on", 0.0f, 100.0f, 30.0f);
NVAR(float, r_stats_histogram_transparency, "histogram transparency", 0.25f, 1.0f, 1.0f);

namespace r {

u::deferred_data<u::map<const char *, stat>, false> stat::m_stats;
u::vector<float> stat::m_histogram;
u::vector<unsigned char> stat::m_texture;

static constexpr size_t kSpace = 20u;

void stat::drawHistogram(size_t x, size_t next) {
    // draw histogram
    if (!m_histogram.size())
        return;

    gui::drawText(x, next, gui::kAlignLeft, "Histogram", gui::RGBA(255, 255, 0));
    next -= kSpace*2;
    const m::vec3 bad(1.0f, 0.0f, 0.0f);
    const m::vec3 good(0.0f, 1.0f, 0.0f);
    const size_t renderWidth = m::floor((neoWidth()-kSpace*4) * r_stats_histogram_size);
    const size_t renderHeight = kSpace*2;

    // ignore subpixel samples
    size_t sampleCount = m_histogram.size();
    while ((renderWidth % sampleCount) != 0)
        sampleCount--;
    const size_t sampleWidth = renderWidth / sampleCount;

    m_texture.destroy();
    m_texture.resize(renderWidth * renderHeight * 4);

    for (size_t i = 0; i < sampleCount; i++) {
        const auto &it = m_histogram[i];
        const float scaledSample = it >= r_stats_histogram_max ? 1.0f : it / r_stats_histogram_max;
        const m::vec3 color = bad*scaledSample+good*(1.0f-scaledSample);
        const size_t height = m::floor(kSpace*2.0f*scaledSample);
        for (size_t h = 1; h < height; ++h) {
            unsigned char *dst = &m_texture[0] + renderWidth*(renderHeight-h)*4 + sampleWidth*i*4;
            for (size_t w = 0; w < sampleWidth; ++w) {
                *dst++ = color.x*255.0f;
                *dst++ = color.y*255.0f;
                *dst++ = color.z*255.0f;
                *dst++ = 255.0f * r_stats_histogram_transparency;
            }
        }
    }
    // render the texture's contents into the UI directly
    gui::drawTexture(x + kSpace, next-kSpace+5, renderWidth, renderHeight, m_texture);
}

size_t stat::drawMemoryInfo(size_t x, size_t next) {
    const auto color = gui::RGBA(255,255,255);
    if (gl::has(gl::NVX_gpu_memory_info)) {
        gui::drawText(x, next, gui::kAlignLeft, "Memory Info", gui::RGBA(255, 255, 0));
        next -= kSpace;

        GLint read[5];
        size_t memory[4];
        gl::GetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &read[0]);
        gl::GetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &read[1]);
        gl::GetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &read[2]);
        gl::GetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &read[3]);
        gl::GetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &read[4]);
        memory[0] = size_t(read[0]) * 1024u;
        memory[1] = size_t(read[1]) * 1024u;
        memory[2] = size_t(read[2]) * 1024u;
        memory[3] = size_t(read[4]) * 1024u;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Dedicated Memory: %s", u::sizeMetric(memory[0])).c_str(), color);
        next -= kSpace;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Total Memory: %s", u::sizeMetric(memory[1])).c_str(), color);
        next -= kSpace;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Available Memory: %s", u::sizeMetric(memory[2])).c_str(), color);
        next -= kSpace;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Eviction Count: %zu", size_t(read[3])).c_str(), color);
        next -= kSpace;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Eviction Memory: %s", u::sizeMetric(memory[3])).c_str(), color);
        next -= kSpace;
    } else if (gl::has(gl::ATI_meminfo)) {
        gui::drawText(x, next, gui::kAlignLeft, "Memory Info", gui::RGBA(255, 255, 0));
        next -= kSpace;

        GLint vbo[4]; size_t vbomem[4];
        GLint tex[4]; size_t texmem[4];
        GLint rbf[4]; size_t rbfmem[4];
        gl::GetIntegerv(GL_VBO_FREE_MEMORY_ATI, vbo);
        gl::GetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, tex);
        gl::GetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, rbf);
        for (size_t i = 0; i < 4; i++) {
            vbomem[i] = size_t(vbo[i]) * 1024u;
            texmem[i] = size_t(tex[i]) * 1024u;
            rbfmem[i] = size_t(rbf[i]) * 1024u;
        }

        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Vertex: Total (%s) - Largest (%s) | Auxiliary: Total (%s) - Largest (%s)",
                u::sizeMetric(vbomem[0]), u::sizeMetric(vbomem[1]),
                u::sizeMetric(vbomem[2]), u::sizeMetric(vbomem[2])).c_str(), color);
        next -= kSpace;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Texture: Total (%s) - Largest (%s) | Auxiliary: Total (%s) - Largest (%s)",
                u::sizeMetric(texmem[0]), u::sizeMetric(texmem[1]),
                u::sizeMetric(texmem[2]), u::sizeMetric(texmem[3])).c_str(), color);
        next -= kSpace;
        gui::drawText(x + kSpace, next, gui::kAlignLeft,
            u::format("Buffer: Total (%s) - Largest (%s) | Auxiliary: Total (%s) - Largest (%s)",
                u::sizeMetric(rbfmem[0]), u::sizeMetric(rbfmem[1]),
                u::sizeMetric(rbfmem[2]), u::sizeMetric(rbfmem[3])).c_str(), color);
        next -= kSpace;
    }
    return next;
}

void stat::render(size_t x) {
    if (r_stats) {
        // calculate total vertical space needed
        size_t space = kSpace;
        for (const auto &it : *m_stats())
            space += it.second.space();

        if (r_stats_histogram) {
            space += kSpace;   // 1 for "Histogram" text
            space += kSpace*2; // 2 for "Histogram" bars
        }

        if (r_stats_gpu_meminfo) {
            if (gl::has(gl::NVX_gpu_memory_info)) {
                space += kSpace;   // 1 for "Memory Info" text
                space += kSpace*5; // for the information
            } else if (gl::has(gl::ATI_meminfo)) {
                space += kSpace;   // 1 for "Memory Info" text
                space += kSpace*3; // for the information
            }
        }

        // shift up by vertical space
        size_t next = space;
        for (const auto &it : *m_stats())
            next = it.second.draw(x, next);

        // memory information before histogram
        if (r_stats_gpu_meminfo)
            next = drawMemoryInfo(x, next);
        if (r_stats_histogram)
            drawHistogram(x, next);
    } else if (r_stats_gpu_meminfo) {
        // memory information before histogram
        const size_t next = drawMemoryInfo(x, kSpace*4);
        if (r_stats_histogram)
            drawHistogram(x, next);
    } else if (r_stats_histogram) {
        drawHistogram(x, kSpace*4);
    }
    // always collect samples even if r_stats_histogram is not enabled;
    // this way if someone toggles it on, the previous samples are immediately
    // available to be rendered to the texture
    const FrameTimer &timer = neoFrameTimer();
    m_histogram.push_back(timer.mspf());
    if (m_histogram.size() > size_t(timer.fps()*r_stats_histogram_duration))
        m_histogram.erase(m_histogram.begin(), m_histogram.begin()+1);
}

stat *stat::get(const char *name) {
    auto find = m_stats()->find(name);
    U_ASSERT(find != m_stats()->end());
    return &find->second;
}

stat *stat::add(const char *name, const char *description) {
    auto find = m_stats()->find(name);
    if (find == m_stats()->end())
        find = m_stats()->insert( { name, { name, description } } ).first;
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
        gui::drawText(x + kSpace, y, gui::kAlignLeft,
            u::format("Instances: %zu", m_instances).c_str(), color);
        y -= kSpace;
    }
    if (m_vboMemory) {
        gui::drawText(x + kSpace, y, gui::kAlignLeft,
            u::format("Vertex Memory: %s", u::sizeMetric(m_vboMemory)).c_str(), color);
        y -= kSpace;
    }
    if (m_iboMemory) {
        gui::drawText(x + kSpace, y, gui::kAlignLeft,
            u::format("Index Memory: %s", u::sizeMetric(m_iboMemory)).c_str(), color);
        y -= kSpace;
    }
    if (m_textureCount > 1) {
        // Multiple textures: indicate count and total memory usage
        gui::drawText(x + kSpace, y, gui::kAlignLeft, "Textures:", color);
        y -= kSpace;
        gui::drawText(x + 40, y, gui::kAlignLeft,
            u::format("Count: %zu", m_textureCount).c_str(), color);
        y -= kSpace;
        gui::drawText(x + 40, y, gui::kAlignLeft,
            u::format("Memory: %s", u::sizeMetric(m_textureMemory)).c_str(), color);
        y -= kSpace;
    } else if (m_textureCount) {
        // Single texture: has no count just indicate texture memory (memory of the single texture)
        gui::drawText(x + kSpace, y, gui::kAlignLeft,
            u::format("Texture Memory: %s", u::sizeMetric(m_textureMemory)).c_str(), color);
        y -= kSpace;
    }
    return y;
}

}
