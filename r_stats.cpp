#include "gui.h"
#include "cvar.h"
#include "r_stats.h"

NVAR(int, r_stats, "rendering statistics", 0, 1, 1);

namespace r {

u::map<const char *, stat> stat::m_stats;

static constexpr size_t kSpace = 20u;

void stat::render(size_t x) {
    if (r_stats) {
        // calculate total vertical space needed
        size_t space = kSpace;
        for (const auto &it : m_stats)
            space += it.second.space();
        // shift up by vertical space
        size_t next = space;
        for (const auto &it : m_stats)
            next = it.second.draw(x, next);
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
