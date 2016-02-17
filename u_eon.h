#ifndef U_EON_HDR
#define U_EON_HDR
#include <stddef.h>
// Easy Object Notation

namespace u {

struct eon {
    eon();
    ~eon();
    bool load(const char *data, const char *end);

    enum {
        kSection,
        kValue
    };

    struct entry {
        entry(entry *parent);
        void append(entry *child);

        int type;
        union {
            char *name;
            char *value;
        };
        entry *next;
        entry *parent;
        entry *head;
        entry *tail;
        size_t count;
        int spaces; // Root has -1
    };

    entry *root() const  { return m_head; }

private:
    static void destroy(entry *top);
    entry *m_head;
    entry *m_tail;
};

}

#endif
