#ifndef C_CONSOLE_HDR
#define C_CONSOLE_HDR
#include "u_string.h"
#include "u_optional.h"
#include "u_map.h"

#include "c_variable.h"
#include "c_complete.h"

namespace c {

struct Console {
    typedef u::map<u::string, Reference> Map;

    enum {
        kVarSuccess = 1,
        kVarRangeError,
        kVarTypeError,
        kVarNotFoundError,
        kVarReadOnlyError
    };

    // get a reference to a console variable
    static const Reference &reference(const char *name);

    // get the variable value: variable must exist
    template <typename T>
    static Variable<T> &value(const char *name);

    // get the variable value as a string (or u::none if not exist.)
    static u::optional<u::string> value(const char *name);

    // set the variable value directly
    template <typename T>
    static int set(const char *name, const T &value);

    // set the variable value by string
    static int change(const char *name, const char *value);
    static int change(const u::string &name, const u::string &value);

    // initialize the console subsystem now
    static void initialize();

    // shut down the console subsystem
    static void shutdown();

    // get console variable suggestions for a prefix
    static u::vector<u::string> suggestions(const u::string &prefix);

private:
    friend struct Config;
    friend struct Reference;

    static Reference *m_references;

    static Reference *split(Reference *ref);
    static Reference *merge(Reference *lhs, Reference *rhs);
    static Reference *sort(Reference *begin);

    static Map &map();
    static unsigned char m_mapData[sizeof(Map)];
    static Map *m_map;

    static Complete &complete();
    static unsigned char m_completeData[sizeof(Complete)];
    static Complete *m_complete;
};

}

#endif
