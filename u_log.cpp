#include <SDL_mutex.h>

#include "u_log.h"
#include "u_algorithm.h"

namespace u {

#ifdef _WIN32
Logger Log::err("stderr.log");
Logger Log::out("stdout.log");
#else
Logger Log::err(stderr);
Logger Log::out(stdout);
#endif

Logger::Logger(FILE *fp)
    : m_mutex(nullptr)
    , m_file(nullptr)
{
    // prevent concurrent initialization
    if (SDL_AtomicAdd(&m_init, 1) != 0) return;
    m_mutex = (void *)SDL_CreateMutex();
    m_file = fp;
    U_ASSERT(m_file);
    m_buffer.reserve(4096*8); // 8 pages is tuned well for iovec
    // disable buffering in stdio
    setvbuf(m_file, nullptr, _IONBF, 0);
}

Logger::Logger(const char *file)
    : m_mutex(nullptr)
    , m_file(nullptr)
{
    // prevent concurrent initialization
    if (SDL_AtomicAdd(&m_init, 1) != 0) return;
    m_mutex = (void *)SDL_CreateMutex();
    m_file = fopen(file, "w");
    U_ASSERT(m_file);
    m_buffer.reserve(4096*8); // 8 pages is tuned well for iovec
    // disable buffering in stdio
    setvbuf(m_file, nullptr, _IONBF, 0);
}

Logger::~Logger() {
    // prevent concurrent deinitialization
    if (SDL_AtomicAdd(&m_init, -1) != 1) return;
    // do not close stdio streams
    if (m_file != stdout && m_file != stderr)
        fclose(m_file);
    SDL_DestroyMutex((SDL_mutex *)m_mutex);
}

void Logger::write(u::string &&message) {
    U_ASSERT(message.find('\r') == u::string::npos);

    // strip terminal escape sequences if logging to a file
    bool stripColor = m_file != stdout && m_file != stderr;
    if (stripColor && message.find('\e') != u::string::npos) {
        for (size_t i = 0; i < message.size();) {
            if (message[i] == '\e') {
                size_t e = i + 1;
                while (message[e] && message[e] != 'm') e++;
                message.erase(i, e + 1);
            } else {
                i++;
            }
        }
    }

    const bool find = message.find('\n') != u::string::npos;

    SDL_LockMutex((SDL_mutex *)m_mutex);

    // flush the buffer if need be
    if (find || message.size() + m_buffer.size() >= m_buffer.size()) {
        if (U_UNLIKELY(m_buffer.size() && fwrite(&m_buffer[0], m_buffer.size(), 1, m_file) != 1)) {
            abort();
        }
        m_buffer.clear();
    }

    // if there is a newline write the contents out immediately
    if (find) {
#ifdef _WIN32
        // replace all instances of "\n" with "\r\n" for Windows
        message.replace_all("\n", "\r\n");
#endif
        if (U_UNLIKELY(fwrite(&message[0], message.size(), 1, m_file) != 1)) {
            abort();
        }
    } else {
        // otherwise it will fit into our buffer
        m_buffer.insert(m_buffer.end(), message.begin(), message.end());
    }

    SDL_UnlockMutex((SDL_mutex *)m_mutex);
}

}
