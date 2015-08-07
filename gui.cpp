#include "engine.h"
#include "gui.h"
#include "cvar.h"

#include "u_misc.h"
#include "u_lru.h"
#include "u_set.h"

#include "m_const.h"

VAR(int, ui_scroll_speed, "mouse scroll speed", 1, 10, 5);

namespace gui {

static struct stringPool : u::lru<u::string> {
    const char *operator()(const char *what);
} gStringPool;

inline const char *stringPool::operator()(const char *what) {
    const auto f = find(what);
    return f ? f->c_str() : insert(what).c_str();
}

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

void queue::addImage(int x, int y, int w, int h, const char *path) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandImage;
    cmd.color = 0;
    cmd.asImage.x = x;
    cmd.asImage.y = y;
    cmd.asImage.w = w;
    cmd.asImage.h = h;
    cmd.asImage.path = gStringPool(path);
}

void queue::addModel(int x, int y, int w, int h, const char *path, const r::pipeline &p) {
    if (m_commands.full()) return;
    auto &cmd = m_commands.next();
    cmd.type = kCommandModel;
    cmd.color = 0;
    cmd.asModel.x = x;
    cmd.asModel.y = y;
    cmd.asModel.w = w;
    cmd.asModel.h = h;
    cmd.asModel.path = gStringPool(path);
    cmd.asModel.pipeline = p;
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
    void update(mouseState &mouse);

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

    mouseState m_mouse;
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
    m_mouse.x = -1;
    m_mouse.y = -1;
    m_mouse.wheel = 0;
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
        && m_mouse.x >= x && m_mouse.x <= x + w
        && m_mouse.y >= y && m_mouse.y <= y + h;
}

inline void state::clearInput() {
    m_leftPressed = false;
    m_leftReleased = false;
    m_mouse.wheel = 0;
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

inline void state::update(mouseState &mouse) {
    // Update the input state
    bool left = mouse.button & kMouseButtonLeft;
    m_mouse.x = mouse.x;
    m_mouse.y = mouse.y;
    m_mouse.wheel = mouse.wheel * -ui_scroll_speed;
    m_leftPressed = !m_left && left;
    m_leftReleased = m_left && !left;
    m_left = left;
}

////////////////////////////////////////////////////////////////////////////////
/// The Singleton
static state gState;

#define G (gState)
#define Q (G.m_queue)  // [Q]ueue
#define W (G.m_widget) // [W]idget
#define A (G.m_area)   // [A]rea
#define S (G.m_scroll) // [S]croll

// Constants
static constexpr int kButtonHeight = 17;
static constexpr int kSliderHeight = 17;
static constexpr int kSliderMarkerWidth = 12;
static constexpr int kCollapseSize = 8;
static constexpr int kCheckBoxSize = 17;
static constexpr int kDefaultSpacing = 6;
static constexpr int kTextHeight = 8;
static constexpr int kScrollAreaPadding = 8;
static constexpr int kIndentationSize = 16;
static constexpr int kAreaHeader = 25;

bool areaBegin(const char *contents, int x, int y, int w, int h, int &value, bool style) {
    A++;
    W.id = 0;
    S.id = (A << 16) | W.id;

    const size_t header = (!contents || !*contents) ? 0 : kAreaHeader;

    W.x = x + kScrollAreaPadding;
    W.y = y+h-header + value;
    W.w = w - kScrollAreaPadding*4;

    S.top = y-header+h;
    S.bottom = y+kScrollAreaPadding/2;
    S.right = x+w-kScrollAreaPadding*3;
    S.value = &value;
    S.areaTop = W.y;
    S.focusTop = y-header;
    S.focusBottom = y-header+h;
    S.inside = G.inRectangle(x, y, w, h, false);

    int totalHeight = int(neoHeight());
    int totalWidth = int(neoWidth());

    G.m_insideCurrentScroll = S.inside;
    if (style) {
        // Didn't test the side menus... Could very well be broken, knowing myself. :>  --acerspyro
        if (x == 0 && y == totalHeight-h && w == totalWidth) {
            Q.addImage(0, y-7, w, 25, "textures/ui/menu_b");
            Q.addImage(0, y+18, w, h-18, "textures/ui/menu_c");
        } else if (x == 0 && y == 0 && w == totalWidth) {
            Q.addImage(0, h-18, w, 25, "textures/ui/menu_t");
            Q.addImage(0, 0, w, h-18, "textures/ui/menu_c");
        } else if (x == 0 && y == 0 && h == totalHeight) {
            Q.addImage(w-18, 0, 25, h, "textures/ui/menu_l");
            Q.addImage(0, 0, w-18, h, "textures/ui/menu_c");
        } else if (x == totalWidth-w && y == 0 && h == totalHeight) {
            Q.addImage(x-7, 0, 25, h, "textures/ui/menu_r");
            Q.addImage(x+18, y, w-18, h, "textures/ui/menu_c");
        } else {
            Q.addImage(x-7, y+h-18, 25, 25, "textures/ui/menu_tl");
            Q.addImage(x+w-18, y+h-18, 25, 25, "textures/ui/menu_tr");
            Q.addImage(x-7, y-7, 25, 25, "textures/ui/menu_bl");
            Q.addImage(x+w-18, y-7, 25, 25, "textures/ui/menu_br");
            Q.addImage(x+18, y+h-18, w-36, 25, "textures/ui/menu_t");
            Q.addImage(x+18, y-7, w-36, 25, "textures/ui/menu_b");
            Q.addImage(x-7, y+18, 25, h-36, "textures/ui/menu_l");
            Q.addImage(x+w-18, y+18, 25, h-36, "textures/ui/menu_r");
            Q.addImage(x+18, y+18, w-36, h-36, "textures/ui/menu_c");
        }
    }
    if (contents) {
        Q.addText(x+header/2, y+h-header/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(255, 255, 255, 128));
    }
    Q.addScissor(x+kScrollAreaPadding, y+kScrollAreaPadding, w-kScrollAreaPadding*4,
        h-header-kScrollAreaPadding);

    return S.inside;
}

void areaFinish(int inc, bool autoScroll) {
    Q.addScissor(-1, -1, -1, -1);

    const int x = S.right + kScrollAreaPadding/2;
    const int y = S.bottom;
    const int w = kScrollAreaPadding*2;
    const int h = S.top - S.bottom;

    const int stop = S.areaTop;
    const int sbot = W.y;
    const int scmp = stop - sbot;
    const int sh = m::clamp(scmp, 1, scmp);

    const float barHeight = sh ? float(h) / float(sh) : float(h);

    if (barHeight < 1.0f) {
        if (autoScroll) {
            *S.value = m::clamp(*S.value + inc, 0, sh - h);
        } else {
            const float barY = m::clamp(float(y - sbot) / float(sh), 0.0f, 1.0f);
            const auto id = S.id;
            const int hx = x;
            const int hy = y + int(barY * h);
            const int hw = w;
            const int hh = int(barHeight * h);
            const int range = h - (hh - 1);
            G.buttonLogic(id, G.inRectangle(hx, hy, hw, hh));
            if (G.isActive(id)) {
                float u = float(hy - y) / float(range);
                if (G.m_wentActive) {
                    G.m_drag[1] = G.m_mouse.y;
                    G.m_dragOrigin = u;
                }
                if (G.m_drag[1] != G.m_mouse.y) {
                    u = m::clamp(G.m_dragOrigin + (G.m_mouse.y - G.m_drag[1]) / float(range), 0.0f, 1.0f);
                    *S.value = int((1 - u) * (sh - h));
                }
            }
            // Background
            Q.addImage(x, y+h-6, w, 6, "textures/ui/scrollbar_vt");
            Q.addImage(x, y+6, w, h-11, "textures/ui/scrollbar_vm");
            Q.addImage(x, y, w, 6, "textures/ui/scrollbar_vb");
            // Bar
            if (G.isActive(id)) {
                Q.addImage(hx, hy+hh-5, hw, 6, "textures/ui/scrollbarknob_v1t");
                Q.addImage(hx, hy+6, hw, hh-11, "textures/ui/scrollbarknob_vm");
                Q.addImage(hx, hy, hw, 6, "textures/ui/scrollbarknob_v1b");
            } else {
                Q.addImage(hx, hy+hh-5, hw, 6, "textures/ui/scrollbarknob_v0t");
                Q.addImage(hx, hy+6, hw, hh-11, "textures/ui/scrollbarknob_vm");
                Q.addImage(hx, hy, hw, 6, "textures/ui/scrollbarknob_v0b");
                //Q.addRectangle(hx, hy, hw, hh, float(w)/2-1,
                //    G.isHot(id) ? RGBA(255, 0, 225, 96) : RGBA(255, 255, 255, 64));
            }
            // Scrolling
            if (S.inside)
                *S.value = m::clamp(*S.value + inc*G.m_mouse.wheel, 0, sh - h);
        }
    }
    G.m_insideCurrentScroll = false;
}

bool button(const char *contents, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool over = enabled && G.inRectangle(x, y, w, h);
    const bool result = G.buttonLogic(id, over);

    if (enabled) {
        if (G.isHot(id)) {
            Q.addImage(x, y, 6, kButtonHeight, "textures/ui/button_1l");
            Q.addImage(x+6, y, w-11, kButtonHeight, "textures/ui/button_1m");
            Q.addImage(x+w-6, y, 6, kButtonHeight, "textures/ui/button_1r");
        } else {
            Q.addImage(x, y, 6, kButtonHeight, "textures/ui/button_0l");
            Q.addImage(x+6, y, w-11, kButtonHeight, "textures/ui/button_0m");
            Q.addImage(x+w-6, y, 6, kButtonHeight, "textures/ui/button_0r");
        }
        Q.addText(x+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, G.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
    } else {
        Q.addText(x+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128, 128, 128, 200));
    }

    return result;
}

int selector(const char *title, int selected, const u::initializer_list<const char *> &elements, bool enabled) {
    const auto prev = (A << 16) | ++W.id;
    const auto next = (A << 16) | ++W.id;

    const int y = W.y - kButtonHeight;
    const int w = 30;
    const int h = kButtonHeight;

    const int prevX = W.x;
    const int textX = W.x + w;
    const int nextX = W.x + W.w - w;
    const int textW = W.w - w*2;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool overPrev = G.inRectangle(prevX, y, w, h);
    const bool overNext = G.inRectangle(nextX, y, w, h);
    const bool resultPrev = G.buttonLogic(prev, overPrev);
    const bool resultNext = G.buttonLogic(next, overNext);

    const int last = int(elements.size()) - 1;

    selected = m::clamp(selected, 0, last);

    if (enabled) {
        Q.addImage(textX-w+20, y, textW+20, kButtonHeight, "textures/ui/selector_m");
        Q.addImage(prevX, y, 30, h, G.isHot(prev) ? "textures/ui/arrow_p1" : "textures/ui/arrow_p0");
        Q.addImage(nextX, y, 30, h, G.isHot(next) ? "textures/ui/arrow_n1" : "textures/ui/arrow_n0");
        if (resultPrev && --selected < 0)
            selected = last;
        if (resultNext && ++selected > last)
            selected = 0;
    } else {
        Q.addImage(prevX, y, 30, h, "textures/ui/arrow_p2");
        Q.addImage(textX, y, textW, kButtonHeight, "textures/ui/selector_m");
        Q.addImage(nextX, y, 30, h, "textures/ui/arrow_n2");
    }

    if (title && *title)
        Q.addText(textX+kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            title, RGBA(255, 255, 255, 255));

    Q.addText(textX+textW/2-kButtonHeight/2, y+kButtonHeight/2-kTextHeight/2, kAlignCenter,
        elements[selected], RGBA(255, 255, 225, 255));

    return selected;
}

bool item(const char *contents, bool enabled) {
    const auto id = (A << 16) | ++W.id;

    const int x = W.x;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool over = enabled && G.inRectangle(x, y, w, h);
    const bool result = G.buttonLogic(id, over);

    if (G.isHot(id))
        Q.addRectangle(x, y, w, h, 2.0f, RGBA(255,196,0,G.isActive(id)?196:96));
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

    const int x = W.x + kDefaultSpacing;
    const int y = W.y - kButtonHeight;
    const int w = W.w;
    const int h = kButtonHeight;

    W.y -= kButtonHeight + kDefaultSpacing;

    const bool over = enabled && G.inRectangle(x, y, w, h);
    const bool result = G.buttonLogic(id, over);

    const int cx = x+kButtonHeight/2-kCheckBoxSize/2;
    const int cy = y+kButtonHeight/2-kCheckBoxSize/2;

    if (checked) {
        if (enabled) {
            Q.addImage(cx-4, cy, kCheckBoxSize, kCheckBoxSize, "textures/ui/check_1");
        } else {
            Q.addImage(cx-4, cy, kCheckBoxSize, kCheckBoxSize, "textures/ui/check_2");
        }
    } else {
        Q.addImage(cx-4, cy, kCheckBoxSize, kCheckBoxSize, "textures/ui/check_0");
    }
    if (enabled) {
        Q.addText(x+kButtonHeight, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, G.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
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

    W.y -= kButtonHeight;

    const int cx = x+kButtonHeight/2-kCollapseSize/2;
    const int cy = y+kButtonHeight/2-kCollapseSize/2;

    const bool over = enabled && G.inRectangle(x, y, w, h);
    const bool result = G.buttonLogic(id, over);

    Q.addTriangle(cx, cy, kCollapseSize, kCollapseSize, checked ? 2 : 1,
        RGBA(255, 255, 255, G.isActive(id) ? 255 : 200));

    if (enabled) {
        Q.addText(x+kButtonHeight, y+kButtonHeight/2-kTextHeight/2, kAlignLeft,
            contents, G.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
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

    Q.addImage(x, y, 6, h, "textures/ui/scrollbar_hl");
    Q.addImage(x+6, y, w-11, h, "textures/ui/scrollbar_hm");
    Q.addImage(x+w-6, y, 6, h, "textures/ui/scrollbar_hr");

    const int range = w - kSliderMarkerWidth;
    const float u = m::clamp((float(value) - min) / float(max - min), 0.0f, 1.0f);

    int m = int(u * range);

    const bool over = enabled && G.inRectangle(x+m, y, kSliderMarkerWidth, kSliderHeight);
    const bool result = G.buttonLogic(id, over);
    bool changed = false;

    if (G.isActive(id)) {
        if (G.m_wentActive) {
            G.m_drag[0] = G.m_mouse.x;
            G.m_dragOrigin = u;
        }
        if (G.m_drag[0] != G.m_mouse.x) { // Mouse and drag don't share same coordinate on the X axis
            const float u = m::clamp(G.m_dragOrigin + float(G.m_mouse.x - G.m_drag[0]) / float(range), 0.0f, 1.0f);
            value = min + u * (max - min);
            value = m::floor(value / float(inc) + 0.5f) * float(inc); // Snap to increments
            m = int(u * range);
            changed = true;
        }
    }

    const u::string msg = u::is_floating_point<T>::value
                              ? u::format("%.2f", value)
                              : u::format("%d", value);

    if (enabled) {
        Q.addText(x+kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignLeft,
            contents, G.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
        Q.addText(x+w-kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignRight,
            msg.c_str(), G.isHot(id) ? RGBA(255, 0, 225, 255) : RGBA(255, 255, 255, 200));
    } else {
        Q.addText(x+kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignLeft,
            contents, RGBA(128, 128, 128, 200));
        Q.addText(x+w-kSliderHeight/2, y+kSliderHeight/2-kTextHeight/2, kAlignRight,
            msg.c_str(), RGBA(128, 128, 128, 200));
    }

    if (G.isActive(id)) {
        Q.addImage(float(x+m), y, 6, kSliderHeight, "textures/ui/scrollbarknob_h1l");
        if (kSliderMarkerWidth > 12)
            Q.addImage(float(x+m)+7, y, y+4, kSliderHeight, "textures/ui/scrollbarknob_hm");
        Q.addImage(float(x+m)+kSliderMarkerWidth-6, y, 6, kSliderHeight, "textures/ui/scrollbarknob_h1r");
    } else {
        Q.addImage(float(x+m), y, 6, kSliderHeight, "textures/ui/scrollbarknob_h0l");
        if (kSliderMarkerWidth > 12)
            Q.addImage(float(x+m)+7, y, y+4, kSliderHeight, "textures/ui/scrollbarknob_hm");
        Q.addImage(float(x+m)+kSliderMarkerWidth-6, y, 6, kSliderHeight, "textures/ui/scrollbarknob_h0r");
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

void drawImage(int x, int y, int w, int h, const char *path) {
    Q.addImage(x, y, w, h, path);
}

void drawModel(int x, int y, int w, int h, const char *path, const r::pipeline &p) {
    Q.addModel(x, y, w, h, path, p);
}

const queue &commands() {
    return Q;
}

void begin(mouseState &mouse) {
    G.update(mouse);

    // This hot becomes the nextHot
    G.m_hot = G.m_nextHot;
    G.m_nextHot = 0;

    // Nothing went active, is active or hot
    G.m_wentActive = false;
    G.m_isActive = false;
    G.m_isHot = false;

    G.m_widget.reset();
    G.m_queue.reset();

    G.m_area = 1;
}

void finish() {
    G.clearInput();
}

}
