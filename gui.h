#ifndef GUI_HDR
#define GUI_HDR
#include <stdint.h>

#include "u_stack.h"

#include "r_pipeline.h"

struct MouseState;

namespace gui {

enum {
    kMouseButtonLeft = 1 << 0,
    kMouseButtonRight = 1 << 1
};

enum {
    kAlignLeft,
    kAlignCenter,
    kAlignRight
};

enum {
    kCommandRectangle,
    kCommandTriangle,
    kCommandLine,
    kCommandText,
    kCommandScissor,
    kCommandImage,
    kCommandModel,
    kCommandTexture
};

struct Box {
    int x;
    int y;
    int w;
    int h;
};

struct Rectangle : Box {
    int r; // Roundness
};

struct Triangle : Box { };
struct Scissor : Box { };

struct Image : Box {
    const char *path;
};

struct Texture : Box {
    // note: all texture data for the GUI must be RGBA8, this code will
    // automatically delete the memory referencing the data
    unsigned char *data;
};

struct Model : Box {
    const char *path;
    r::pipeline pipeline;
    int su;
    int sv;
};

struct Text {
    int x;
    int y;
    int align;
    const char *contents;
};

struct Line {
    int x[2];
    int y[2];
    int r;
};

struct Command {
    Command();

    int type;
    int flags;
    uint32_t color;

    union {
        Line asLine;
        Rectangle asRectangle;
        Scissor asScissor;
        Triangle asTriangle;
        Text asText;
        Image asImage;
        Model asModel;
        Texture asTexture;
    };
};

inline Command::Command()
    : type(-1)
    , flags(0)
    , color(0)
{
}

struct Queue {
    static constexpr size_t kCommandQueueSize = 5000;
    const u::stack<Command, kCommandQueueSize> &operator()() const;
    void reset();
    void addScissor(int x, int y, int w, int h);
    void addRectangle(int x, int y, int w, int h, uint32_t color);
    void addLine(int x0, int y0, int x1, int y1, int r, uint32_t color);
    void addRectangle(int x, int y, int w, int h, int r, uint32_t color);
    void addTriangle(int x, int y, int w, int h, int flags, uint32_t color);
    void addText(int x, int y, int align, const char *contents, uint32_t color);
    void addTexture(int x, int y, int w, int h, const unsigned char *data);
    void addImage(int x, int y, int w, int h, const char *path);
    void addModel(int x, int y, int w, int h, const char *path, const r::pipeline &p, int su = 0, int sv = 0);
private:
    u::stack<Command, kCommandQueueSize> m_commands;
};

inline void Queue::reset() {
    m_commands.reset();
}

inline const u::stack<Command, Queue::kCommandQueueSize> &Queue::operator()() const {
    return m_commands;
}

inline constexpr uint32_t RGBA(unsigned char R, unsigned char G, unsigned char B, unsigned char A = 255) {
    return R | (G << 8) | (B << 16) | (A << 24);
}

/// Returns index in elements list based upon selection
/// `title' is an optional string that gets left-justified on the selection bar
template <typename T>
int selector(const char *title, int selected, const u::vector<T> &elements, bool enabled = true);
/// Returns true when clicked, false otherwise
bool button(const char *contents, bool enabled = true);
/// Returns true when clicked, false otherwise
bool item(const char *contents, bool enabled = true);
/// Returns true when clicked, false otherwise
bool check(const char *contents, bool checked, bool enabled = true);
/// Returns true when clicked, false otherwise
bool collapse(const char *contents, const char *subtext, bool checked, bool enabled = true);
/// A label is left justified text
void label(const char *contents);
/// A value is right justified text
void value(const char *contents);
/// Returns true when updated, false otherwise, `value' is a reference to the value
/// which is changed, `min' and `'max' define the applicable range for the slider,
/// while `inc' specifies how much the value should increment when sliding or scrolling.
template <typename T>
bool slider(const char *contents, T &value, T min, T max, T inc, bool enabled = true);
/// Indent the widget space so that any widgets after the call will be indented
void indent();
/// Dedent the widget space so that any widgets after the vall will be dedented
void dedent();
/// Add vertical separation
void separator();
/// A heading (vertical separation + line)
void heading();
/// Construct a menu of name `contents' at position `x,y' of size `w,h', `value
/// is a reference to a variable which contains the amount of "scroll" of the
/// area if the contents exceed the vertical size specified by `h'.
bool areaBegin(const char *contents, int x, int y, int w, int h, int& value, bool style = true);
/// Called when finished the menu, `inc' is the value of which to increase the
/// the `value' as previous referenced in the parent `areaBegin' call when
/// scrolling in the area, while autoScroll will enable automatic scrolling of
/// the area and hide the scroll bar
void areaFinish(int inc = 5, bool autoScroll = false);

// Primitive drawing
void drawLine(int x0, int y0, int x1, int y1, int r, uint32_t color);
void drawLine(int x0, int y0, int x1, int y1, int r, uint32_t color);
void drawRectangle(int x, int y, int w, int h, uint32_t color);
void drawRectangle(int x, int y, int w, int h, int r, uint32_t color);
void drawText(int x, int y, int align, const char *contents, uint32_t color);
void drawTriangle(int x, int y, int w, int h, int flags, uint32_t color);
void drawImage(int x, int y, int w, int h, const char *path);
void drawModel(int x, int y, int w, int h, const char *path, const r::pipeline &p, int su = 0, int sv = 0);
void drawTexture(int x, int y, int w, int h, const u::vector<unsigned char> &rgba);

const Queue &commands();

void begin(MouseState &mouse);
void finish();

}
#endif
