#include <math.h>

#include "engine.h"
#include "gui.h"

#include "u_misc.h"
#include "u_map.h"

#include "m_const.h"

namespace gui {

static struct stringPool {
    static constexpr size_t kTableSize = 1024;
    static constexpr size_t kThreshold = 25; // Anything less is destroyed during collect phase

    ~stringPool() {
        for (size_t i = 0; i < kTableSize; i++) {
            if (!m_table[i])
                continue;
            neoFree(m_table[i]->m_data);
            neoFree(m_table[i]);
        }
    }

    stringPool()
        : m_active(0)
    {
        for (size_t i = 0; i < kTableSize; i++)
            m_table[i] = nullptr;
    }

    const char *operator()(const char *data) {
        const size_t length = strlen(data) + 1;
        size_t hash = u::detail::sdbm((const void *)data, length) & (kTableSize - 1);
        while (m_table[hash] && memcmp((const void *)m_table[hash]->m_data, (const void *)data, length))
            hash = (hash + 1) & (kTableSize - 1);
        if (m_table[hash]) {
            m_table[hash]->m_count++;
            return m_table[hash]->m_data;
        }
        m_table[hash] = neoMalloc(sizeof(entry));
        m_table[hash]->m_data = (char *)memcpy(neoMalloc(length), (const void *)data, length);
        m_active++;
        return m_table[hash]->m_data;
    }

    void collect(size_t &active) {
        for (size_t i = 0; i < kTableSize; i++) {
            if (m_table[i] && m_table[i]->m_count < kThreshold) {
                neoFree(m_table[i]->m_data);
                neoFree(m_table[i]);
                m_table[i] = nullptr;
                m_active--;
            }
        }
        active = m_active;
    }

private:
    struct entry {
        entry(const char *data)
            : m_count(1)
        {
            const size_t length = strlen(data) + 1;
            m_data = (char *)memcpy(neoMalloc(length), data, length);
        }
    private:
        friend struct stringPool;
        char *m_data;
        size_t m_count;
    };
    entry *m_table[kTableSize];
    size_t m_active;

} gStringPool;

void queue::addScissor(int x, int y, int w, int h) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandScissor;
    cmd.flags = x < 0 ? 0 : 1;
    cmd.color = 0;
    cmd.asScissor.x = x;
    cmd.asScissor.y = y;
    cmd.asScissor.w = w;
    cmd.asScissor.h = h;
}

void queue::addRectangle(int x, int y, int w, int h, uint32_t color) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandRectangle;
    cmd.flags = 0;
    cmd.color = color;
    cmd.asRectangle.x = x;
    cmd.asRectangle.y = y;
    cmd.asRectangle.w = w;
    cmd.asRectangle.h = h;
    cmd.asRectangle.r = 0;
}

void queue::addLine(int x0, int y0, int x1, int y1, int r, uint32_t color) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandLine;
    cmd.flags = 0;
    cmd.color = color;
    cmd.asLine.x[0] = x0;
    cmd.asLine.y[0] = y0;
    cmd.asLine.x[1] = x1;
    cmd.asLine.y[1] = y1;
    cmd.asLine.r = r;
}

void queue::addRectangle(int x, int y, int w, int h, int r, uint32_t color) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandRectangle;
    cmd.flags = 0;
    cmd.color = color;
    cmd.asRectangle.x = x;
    cmd.asRectangle.y = y;
    cmd.asRectangle.w = w;
    cmd.asRectangle.h = h;
    cmd.asRectangle.r = r;
}

void queue::addTriangle(int x, int y, int w, int h, int flags, uint32_t color) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandTriangle;
    cmd.flags = flags;
    cmd.color = color;
    cmd.asTriangle.x = x;
    cmd.asTriangle.y = y;
    cmd.asTriangle.w = w;
    cmd.asTriangle.h = h;
}

void queue::addText(int x, int y, int align, const char *contents, uint32_t color) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandText;
    cmd.flags = 0;
    cmd.color = color;
    cmd.asText.x = x;
    cmd.asText.y = y;
    cmd.asText.align = align;
    cmd.asText.contents = gStringPool(contents);
}

void queue::addImage(int x, int y, int w, int h, const char *path, bool mipmaps) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandImage;
    cmd.flags = mipmaps;
    cmd.color = 0;
    cmd.asImage.x = x;
    cmd.asImage.y = y;
    cmd.asImage.w = w;
    cmd.asImage.h = h;
    cmd.asImage.path = gStringPool(path);
}

// A reference to something in the gui
typedef size_t ref;

// A widget is just a bounding box with no height.
struct widget {
    static const size_t kInitialIndentation = 100;
    widget();
    void reset();

    int x;
    int y;
    int w;
    ref id;
};

inline widget::widget() {
    reset();
    w = kInitialIndentation;
}

inline void widget::reset() {
    x = 0;
    y = 0;
    w = 0;
    id = 1; // The root reference is 0
}

// Scrollable area
struct scroll {
    scroll();

    int top;
    int bottom;
    int right;
    int areaTop;
    int *value;
    int focusTop;
    int focusBottom;
    ref id;
    bool inside;
};

inline scroll::scroll()
    : top(0)
    , bottom(0)
    , right(0)
    , areaTop(0)
    , value(nullptr)
    , focusTop(0)
    , focusBottom(0)
    , id(0)
    , inside(false)
{
}

struct state {
    state();

    bool anyActive();
    bool isActive(ref thing);
    bool isHot(ref thing);

    bool inRectangle(int x, int y, int w, int h, bool checkScroll = true);

    // To get the commands
    const queue &commands() const;

    // To update input state
    void update(int (&mouse)[4]);

    void clearInput(); // Clear any input state
    void clearActive(); // Clear any active state (reference to active thing)

    // Set active/hot things
    void setActive(ref thing);
    void setHot(ref thing);

    // Implements the logic for button hover/click
    bool buttonLogic(ref thing, bool over);

    ref m_active; // Reference to active thing
    ref m_hot; // Reference to hot thing
    ref m_nextHot; // Reference to hotToBe
    ref m_area; // Reference to root area

    int m_mouse[3]; // X, Y, Scroll
    int m_drag[2]; // X, Y
    float m_dragOrigin;

    bool m_insideCurrentScroll; // Inside a scrollable area
    bool m_isHot; // Something is hot
    bool m_isActive; // Something is active
    bool m_wentActive; // Something just became active

    bool m_left; // left mouse button
    bool m_leftPressed; // left pressed
    bool m_leftReleased; // left released

    widget m_widget;
    queue m_queue;
    scroll m_scroll;
};

inline state::state()
    : m_active(0)
    , m_hot(0)
    , m_nextHot(0)
    , m_area(0)
    , m_dragOrigin(0.0f)
    , m_insideCurrentScroll(false)
    , m_isHot(false)
    , m_isActive(false)
    , m_wentActive(false)
    , m_left(false)
    , m_leftPressed(false)
    , m_leftReleased(false)
{
    m_mouse[0] = -1;
    m_mouse[1] = -1;
    m_mouse[2] = 0; // Scroll
    m_drag[0] = 0;
    m_drag[1] = 0;
}

inline bool state::anyActive() {
    return m_active != 0;
}

inline bool state::isActive(ref thing) {
    return m_active == thing;
}

inline bool state::isHot(ref thing) {
    return m_hot == thing;
}

inline bool state::inRectangle(int x, int y, int w, int h, bool checkScroll) {
    return (!checkScroll || m_insideCurrentScroll)
        && m_mouse[0] >= x && m_mouse[0] <= x + w
        && m_mouse[1] >= y && m_mouse[1] <= y + h;
}

inline void state::clearInput() {
    m_leftPressed = false;
    m_leftReleased = false;
    m_mouse[2] = 0; // Scroll
}

inline void state::clearActive() {
    m_active = 0;
    clearInput();
}

inline void state::setActive(ref thing) {
    m_active = thing;
    m_wentActive = true;
}

inline void state::setHot(ref thing) {
    m_nextHot = thing;
}

inline bool state::buttonLogic(ref thing, bool over) {
    bool result = false;
    // Nothing is active
    if (!anyActive()) {
        if (over) // Hovering it, make hot
            setHot(thing);
        if (isHot(thing) && m_leftPressed) // Hot and pressed, make active
            setActive(thing);
    }
    // Button is active, react on left released
    if (isActive(thing)) {
        m_isActive = true;
        if (over) // Hovering it, make hot
            setHot(thing);
        if (m_leftReleased) { // It's been clicked
            if (isHot(thing))
                result = true;
            clearActive();
        }
    }
    if (isHot(thing))
        m_isHot = true;
    return result;
}

inline void state::update(int (&mouse)[4]) {
    // Update the input state
    bool left = mouse[3] & kMouseButtonLeft;
    m_mouse[0] = mouse[0];
    m_mouse[1] = mouse[1];
    m_mouse[2] = mouse[2] * -2;
    m_leftPressed = !m_left && left;
    m_leftReleased = m_left && !left;
    m_left = left;
}

////////////////////////////////////////////////////////////////////////////////
/// The Singleton
static state S; // [S]tate

#define Q (S.m_queue)  // [Q]ueue
#define W (S.m_widget) // [W]idget
#define A (S.m_area)   // [A]rea
#define B (S.m_scroll) // Scroll [B]ar

// Constants
static constexpr int kButtonHeight = 20;
static constexpr int kSliderHeight = 16;
static constexpr int kSliderMarkerWidth = 12;
static constexpr int kCollapseSize = 8;
static constexpr int kCheckBoxSize = 20;
static constexpr int kDefaultSpacing = 6;
static constexpr int kTextHeight = 8;
static constexpr int kScrollAreaPadding = 8;
static constexpr int kIndentationSize = 16;
static constexpr int kAreaHeader = 28;

bool areaBegin(const char *contents, int x, int y, int w, int h, int &value, bool style) {
    A++;
    W.id = 0;
    B.id = (A << 16) | W.id;

    const size_t header = (!contents || !*contents) ? 0 : kAreaHeader;

    W.x = x + kScrollAreaPadding;
    W.y = y+h-header + value;
    W.w = w - kScrollAreaPadding*4;

    B.top = y-header+h;
    B.bottom = y+kScrollAreaPadding/2;
    B.right = x+w-kScrollAreaPadding*3;
    B.value = &value;
    B.areaTop = W.y;
    B.focusTop = y-header;
    B.focusBottom = y-header+h;
    B.inside = S.inRectangle(x, y, w, h, false);

    int totalHeight = int(neoHeight());
    int totalWidth = int(neoWidth());

    S.m_insideCurrentScroll = B.inside;
    if (style) {
        if (x == 0 && y == totalHeight-h && w == totalWidth) {
            Q.addImage(0, y, w, 25, "<nocompress>textures/ui/menu_b");
            Q.addImage(0, y+25, w, h-25, "<nocompress>textures/ui/menu_c");
        } else if (x == 0 && y == 0 && w == totalWidth) {
            Q.addImage(0, h-25, w, 25, "<nocompress>textures/ui/menu_t");
            Q.addImage(0, 0, w, h-25, "<nocompress>textures/ui/menu_c");
        } else if (x == 0 && y == 0 && h == totalHeight) {
            Q.addImage(w-25, 0, 25, h, "<nocompress>textures/ui/menu_l");
            Q.addImage(0, 0, w-25, h, "<nocompress>textures/ui/menu_c");
        } else if (x == totalWidth-w && y == 0 && h == totalHeight) {
            Q.addImage(x, 0, 25, h, "<nocompress>textures/ui/menu_r");
            Q.addImage(x+25, y, w-25, h, "<nocompress>textures/ui/menu_c");
        } else {
            Q.addImage(x, y+h-25, 25, 25, "<nocompress>textures/ui/menu_tl");
            Q.addImage(x+w-25, y+h-25, 25, 25, "<nocompress>textures/ui/menu_tr");
            Q.addImage(x, y, 25, 25, "<nocompress>textures/ui/menu_bl");
            Q.addImage(x+w-25, y, 25, 25, "<nocompress>textures/ui/menu_br");
            Q.addImage(x+25, y+h-25, w-50, 25, "<nocompress>textures/ui/menu_t");
            Q.addImage(x+25, y, w-50, 25, "<nocompress>textures/ui/menu_b");
            Q.addImage(x, y+25, 25, h-50, "<nocompress>textures/ui/menu_l");
            Q.addImage(x+w-25, y+25, 25, h-50, "<nocompress>textures/ui/menu_r");
            Q.addImage(x+25, y+25, w-50, h-50, "<nocompress>textures/ui/menu_c");
        }
    }
    if (contents) {
        Q.addText(x+header/2, y+h-header/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(255, 255, 255, 128));
    }
    Q.addScissor(x+kScrollAreaPadding, y+kScrollAreaPadding, w-kScrollAreaPadding*4,
        h-header-kScrollAreaPadding);

    return B.inside;
}

void areaFinish(int inc, bool autoScroll) {
    Q.addScissor(-1, -1, -1, -1);

    const int x = B.right + kScrollAreaPadding/2;
    const int y = B.bottom;
    const int w = kScrollAreaPadding*2;
    const int h = B.top - B.bottom;

    const int stop = B.areaTop;
    const int sbot = W.y;
    const int sh = stop - sbot;

    float barHeight = float(h)/float(sh);

    if (barHeight < 1.0f) {
        if (autoScroll) {
            *B.value = m::clamp(*B.value + inc, 0, sh - h);
        } else {
            const float barY = m::clamp(float(y - sbot) / float(sh), 0.0f, 1.0f);
            const auto id = B.id;
            const int hx = x;
            const int hy = y + int(barY * h);
            const int hw = w;
            const int hh = int(barHeight * h);
            const int range = h - (hh - 1);
            S.buttonLogic(id, S.inRectangle(hx, hy, hw, hh));
            if (S.isActive(id)) {
                float u = float(hy - y) / float(range);
                if (S.m_wentActive) {
                    S.m_drag[1] = S.m_mouse[1];
                    S.m_dragOrigin = u;
                }
                if (S.m_drag[1] != S.m_mouse[1]) {
                    u = m::clamp(S.m_dragOrigin + (S.m_mouse[1] - S.m_drag[1]) / float(range), 0.0f, 1.0f);
                    *B.value = int((1 - u) * (sh - h));
                }
            }
            // Background
            Q.addImage(x, y+h-6, w, 6, "<nocompress>textures/ui/scrollbar_vt");
            Q.addImage(x, y+6, w, h-12, "<nocompress>textures/ui/scrollbar_vm");
            Q.addImage(x, y, w, 6, "<nocompress>textures/ui/scrollbar_vb");
            // Bar
            if (S.isActive(id)) {
                Q.addImage(hx, hy+hh-6, hw, 6, "<nocompress>textures/ui/scrollbarknob_v1t");
                Q.addImage(hx, hy+6, hw, hh-12, "<nocompress>textures/ui/scrollbarknob_vm");
                Q.addImage(hx, hy, hw, 6, "<nocompress>textures/ui/scrollbarknob_v1b");
            } else {
                Q.addImage(hx, hy+hh-6, hw, 6, "<nocompress>textures/ui/scrollbarknob_v0t");
                Q.addImage(hx, hy+6, hw, hh-12, "<nocompress>textures/ui/scrollbarknob_vm");
                Q.addImage(hx, hy, hw, 6, "<nocompress>textures/ui/scrollbarknob_v0b");
                //Q.addRectangle(hx, hy, hw, hh, float(w)/2-1,
                //    S.isHot(id) ? RGBA(255, 0, 225, 96) : RGBA(255, 255, 255, 64));
            }
            // Scrolling
            if (B.inside)
                *B.value = m::clamp(*B.value + inc*S.m_mouse[2], 0, sh - h);
        }
    }
    S.m_insideCurrentScroll = false;
}

bool button(const char *contents, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool over = enabled && S.inRectangle(x, y, w, h);
    const bool result = S.buttonLogic(id, over);

    if (enabled) {
        if (S.isHot(id)) {
            Q.addImage(x, y, 6, kButtonHeight, "<nocompress>textures/ui/button_1l");
            Q.addImage(x+5, y, w-10, kButtonHeight, "<nocompress>textures/ui/button_1m");
            Q.addImage(x+w-6, y, 6, kButtonHeight, "<nocompress>textures/ui/button_1r");
        } else {
            Q.addImage(x, y, 6, kButtonHeight, "<nocompress>textures/ui/button_0l");
            Q.addImage(x+5, y, w-10, kButtonHeight, "<nocompress>textures/ui/button_0m");
            Q.addImage(x+w-6, y, 6, kButtonHeight, "<nocompress>textures/ui/button_0r");
        }
        Q.addText(x+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, S.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
    } else {
        Q.addText(x+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128, 128, 128, 200));
    }

    return result;
}

bool item(const char *contents, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool over = enabled && S.inRectangle(x, y, w, h);
    const bool result = S.buttonLogic(id, over);

    if (S.isHot(id))
        Q.addRectangle(x, y, w, h, 2.0f, RGBA(255,196,0,S.isActive(id)?196:96));
    if (enabled) {
        Q.addText(x+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(255,255,255,200));
    } else {
        Q.addText(x+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128,128,128,200));
    }

    return result;
}

bool check(const char *contents, bool checked, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool over = enabled && S.inRectangle(x, y, w, h);
    const bool result = S.buttonLogic(id, over);

    const int cx = x+kButtonHeight/2-kCheckBoxSize/2;
    const int cy = y+kButtonHeight/2-kCheckBoxSize/2;

    if (checked) {
        if (enabled) {
            Q.addImage(cx-4, cy, kCheckBoxSize, kCheckBoxSize, "<nocompress>textures/ui/check_1");
        } else {
            Q.addImage(cx-4, cy, kCheckBoxSize, kCheckBoxSize, "<nocompress>textures/ui/check_2");
        }
    } else {
        Q.addImage(cx-4, cy, kCheckBoxSize, kCheckBoxSize, "<nocompress>textures/ui/check_0");
    }
    if (enabled) {
        Q.addText(x+kButtonHeight, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, S.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
    } else {
        Q.addText(x+kButtonHeight, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128, 128, 128, 200));
    }

    return result;
}

bool collapse(const char *contents, const char *subtext, bool checked, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const int cx = x+kButtonHeight/2-kCollapseSize/2;
    const int cy = y+kButtonHeight/2-kCollapseSize/2;

    const bool over = enabled && S.inRectangle(x, y, w, h);
    const bool result = S.buttonLogic(id, over);

    Q.addTriangle(cx, cy, kCollapseSize, kCollapseSize, checked ? 2 : 1,
        RGBA(255, 255, 255, S.isActive(id) ? 255 : 200));

    if (enabled) {
        Q.addText(x+kButtonHeight, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, S.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
    } else {
        Q.addText(x+kButtonHeight, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128, 128, 128, 200));
    }

    if (subtext && *subtext) {
        Q.addText(x+w-kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignRight,
            subtext, RGBA(255, 255, 255, 128));
    }

    return result;
}

void label(const char *contents) {
    const int x = W.x;
    const int y = W.y - kButtonHeight;

    W.y -= kButtonHeight;

    Q.addText(x, y+kButtonHeight/2-kTextHeight/2, kAlignLeft, contents,
        RGBA(255, 255, 255, 255));
}

void value(const char *contents) {
    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;

    W.y -= kButtonHeight;

    Q.addText(x+w-kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignRight,
        contents, RGBA(255, 255, 255, 200));
}

template <typename T>
bool slider(const char *contents, T &value, T min, T max, T inc, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kSliderHeight;

    W.y -= kSliderHeight + kDefaultSpacing;

    Q.addImage(x, y, 6, h, "<nocompress>textures/ui/scrollbar_hl");
    Q.addImage(x+6, y, w-12, h, "<nocompress>textures/ui/scrollbar_hm");
    Q.addImage(x+w-6, y, 6, h, "<nocompress>textures/ui/scrollbar_hr");

    const int range = w - kSliderMarkerWidth;
    const float u = m::clamp((float(value) - min) / float(max - min), 0.0f, 1.0f);

    int m = int(u * range);

    const bool over = enabled && S.inRectangle(x+m, y, kSliderMarkerWidth, kSliderHeight);
    const bool result = S.buttonLogic(id, over);
    bool changed = false;

    if (S.isActive(id)) {
        if (S.m_wentActive) {
            S.m_drag[0] = S.m_mouse[0];
            S.m_dragOrigin = u;
        }
        if (S.m_drag[0] != S.m_mouse[0]) { // Mouse and drag don't share same coordinate on the X axis
            const float u = m::clamp(S.m_dragOrigin + float(S.m_mouse[0] - S.m_drag[0]) / float(range), 0.0f, 1.0f);
            value = min + u * max - min;
            value = floorf(float(value) / float(inc) + 0.5f) * float(inc); // Snap to increments
            m = int(u * range);
            changed = true;
        }
    }

    if (S.isActive(id)) {
        Q.addImage(float(x+m), y, 6, kSliderHeight, "<nocompress>textures/ui/scrollbarknob_h1l");
        if (kSliderMarkerWidth > 12)
            Q.addImage(float(x+m)+6, y, y+6, kSliderHeight, "<nocompress>textures/ui/scrollbarknob_hm");
        Q.addImage(float(x+m)+kSliderMarkerWidth-6, y, 6, kSliderHeight, "<nocompress>textures/ui/scrollbarknob_h1r");
    } else {
        Q.addImage(float(x+m), y, 6, kSliderHeight, "<nocompress>textures/ui/scrollbarknob_h0l");
        if (kSliderMarkerWidth > 12)
            Q.addImage(float(x+m)+6, y, y+6, kSliderHeight, "<nocompress>textures/ui/scrollbarknob_hm");
        Q.addImage(float(x+m)+kSliderMarkerWidth-6, y, 6, kSliderHeight, "<nocompress>textures/ui/scrollbarknob_h0r");
    }

    const int digits = int(ceilf(log10f(inc)));
    u::string msg = u::is_floating_point<T>::value
        ? u::format("%.*f", digits >= 0 ? 0 : -digits, value)
        : u::format("%.*d", digits >= 0 ? 1 : -digits, value);

    if (enabled) {
        Q.addText(x+kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignLeft,
            contents, S.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
        Q.addText(x+w-kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignRight,
            msg.c_str(), S.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
    } else {
        Q.addText(x+kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128, 128, 128, 200));
        Q.addText(x+w-kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignRight,
            msg.c_str(), RGBA(128, 128, 128, 200));
    }

    return result || changed;
}

template bool slider<float>(const char *contents, float &value, float min, float max, float inc, bool enabled);
template bool slider<int>(const char *contents, int &value, int min, int max, int inc, bool enabled);

void indent() {
    W.x += kIndentationSize;
    W.w -= kIndentationSize;
}

void dedent() {
    W.x -= kIndentationSize;
    W.w += kIndentationSize;
}

void separator() {
    W.y -= kDefaultSpacing*2;
}

void heading() {
    const int x = W.x;
    const int y = W.y - kDefaultSpacing;
    const int w = W.w;
    const int h = 1;

    W.y -= kDefaultSpacing*2;

    Q.addRectangle(x, y, w, h, RGBA(255, 255, 255, 32));
}

/// Primitive drawing (not part of the managed menu), useful for doing HUD
void drawLine(int x0, int y0, int x1, int y1, int r, uint32_t color) {
    Q.addLine(x0, y0, x1, y1, r, color);
}

void drawRectangle(int x, int y, int w, int h, uint32_t color) {
    Q.addRectangle(x, y, w, h, color);
}

void drawRectangle(int x, int y, int w, int h, int r, uint32_t color) {
    Q.addRectangle(x, y, w, h, r, color);
}

void drawText(int x, int y, int align, const char *contents, uint32_t color) {
    Q.addText(x, y, align, contents, color);
}

void drawTriangle(int x, int y, int w, int h, int flags, uint32_t color) {
    Q.addTriangle(x, y, w, h, flags, color);
}

void drawImage(int x, int y, int w, int h, const char *path, bool mipmaps) {
    Q.addImage(x, y, w, h, path, mipmaps);
}

const queue &commands() {
    return Q;
}

void begin(int (&mouse)[4]) {
    S.update(mouse);

    // This hot becomes the nextHot
    S.m_hot = S.m_nextHot;
    S.m_nextHot = 0;

    // Nothing went active, is active or hot
    S.m_wentActive = false;
    S.m_isActive = false;
    S.m_isHot = false;

    S.m_widget.reset();
    S.m_queue.reset();

    S.m_area = 1;
}

void finish() {
    S.clearInput();
}

void collect(size_t &active) {
    gStringPool.collect(active);
}

}
