#include "u_eon.h"
#include "u_string.h"
#include "u_new.h"
#include "u_misc.h"

namespace u {

static u::string readName(const char **d, const char *end) {
    const char *data = *d;
    while (data != end && !strchr(":,", *data) && !u::isspace(*data))
        data++;
    u::string value { *d, size_t(data - *d) };
    *d = data;
    return value;
}

static u::string readToken(const char **d, const char *end) {
    const char *data = *d;
    while (data != end && *data != ',' && !u::isspace(*data))
        data++;
    u::string value { *d, size_t(data - *d) };
    *d = data;
    return value;
}

// TODO: make more efficient
static u::string readString(const char **d, const char *end) {
    const char *data = *d;
    char quote = *data;
    int more = 0;
    u::string value(&quote, 1);
    for (++data; data != end && (more || *data != quote); ++data) {
        if (more)
            --more;
        if (*data == '\\')
            more = 1;
        value += *data;
    }
    if (data != end)
        value += *data++;
    *d = data;
    return u::move(value);
}

static u::string readValue(const char **d, const char *end) {
    const char *data = *d;
    if (strchr("\"\\", *data))
        return readString(d, end);
    return readToken(d, end);
}

eon::entry::entry(entry *parent)
    : type(kValue)
    , name(nullptr)
    , next(nullptr)
    , parent(parent)
    , head(nullptr)
    , tail(nullptr)
    , count(0)
    , spaces(0)
{
}

void eon::entry::append(entry *child) {
    count++;
    if (tail) {
        tail = (tail->next = child);
    } else {
        head = child;
        tail = child;
    }
    child->parent = this;
}

eon::eon()
    : m_head(new entry(nullptr))
    , m_tail(m_head)
{
    m_head->spaces = -1;
    m_head->type = kSection;
}

// It's not the most brilliant looking parser, but it's quite dense.
bool eon::load(const char *data, const char *end) {
    entry *current = new entry(m_tail);
    entry *forward = current;

    enum {
        kNoBadIndent = 1 << 0,
        kNoSpaceCount = 1 << 1
    };
    int state = 0; // Enum flags
    u::string value;

    while (data != end) {
        if (data[0] == '\n') {
handleNewLine:
            state = 0;
            current->spaces = 0;
            ++data;
            continue;
        }

        // /^#/ starts a comment line
        if (current->spaces == 0 && *data == '#') {
            do ++data; while (data != end && *data != '\n');
            if (data == end)
                break;
            goto handleNewLine;
        }

        // Indentation comes first
        if (*data == ' ') {
            const char *where = data;
            do ++data; while (data != end && *data == ' ');
            if (!(state & kNoSpaceCount))
                current->spaces += data - where;
            continue;
        }

        // Value or Section follows: check indentation first to find the
        // real parent
        while (current->spaces <= current->parent->spaces)
            current->parent = current->parent->parent;
        if (current->parent->type != kSection)
            current->parent = current->parent->parent;

        // Check siblings
        entry *sibling = current->parent->head;
        if (sibling && current->spaces != sibling->spaces) {
            if (state & kNoBadIndent) {
                current->spaces = sibling->spaces;
            } else {
                return false;
            }
        }

        // We know where we ought to be, lets insert there
        current->parent->append(current);
        forward = new entry(current);

        // Parse the value
        if (*data == '_' || u::isalpha(data[0])) {
            value = readName(&data, end);
            if (data != end && *data == ':') {
                current->type = kSection;
                ++data;
                forward->spaces = current->spaces + 1;
                goto handleValue;
            }
            if (data == end)
                goto handleValue;
            if (*data == ',') {
handleComma:
                ++data;
                forward->spaces = current->parent->spaces + 1;
                state |= kNoSpaceCount;
                state |= kNoBadIndent;
handleValue:
                current->value = value.copy();
                current = forward;
                forward = nullptr;
                continue;
            }
        }

        // Read just the value
        value += readValue(&data, end);
        if (*data == ',')
            goto handleComma;
        goto handleValue;
    }
    if (forward)
        destroy(forward);
    return true;
}

void eon::destroy(entry *top) {
    if (top->name)
        neoFree(top->name);
    for (; top->head; top->head = top->head->next)
        destroy(top->head);
    delete top;
}

eon::~eon() {
    destroy(m_head);
}

}
