#ifndef U_LOG_HDR
#define U_LOG_HDR
#include <SDL_atomic.h>

#include "u_misc.h"
#include "u_file.h"
#include "u_vector.h"

namespace u {

struct Logger {

    Logger(FILE *file);
    Logger(const char *file);
    ~Logger();

    template <typename... T>
    void operator()(const char *fmt, T&&... args) {
        write(u::move(u::format(fmt, u::forward<T>(args)...)));
    }

private:
    void write(u::string &&str);

    // Note: instances of Logger are static in the Log structure below, which means
    // m_init gets a static value of zero via the OS before any initialization occurs.
    // We depend on this behavior to prevent concurrent initialization of Logger since
    // we've disabled thread-safe initialization of static objects. We implement it
    // ourselfs with a simple compare if (atomic_add(&m_init, 1) != 0) return in
    // the constuctors where atomic_add returns the previous value. The very first
    // call will only ever return zero.
    SDL_atomic_t m_init;
    void *m_mutex;
    u::vector<char> m_buffer;
    FILE *m_file;
};

struct Log {
    static Logger err;
    static Logger out;
};

}

#endif
