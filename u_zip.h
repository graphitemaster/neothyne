#ifndef U_ZIP_HDR
#define U_ZIP_HDR
#include <stdint.h>

#include "u_file.h"
#include "u_optional.h"
#include "u_map.h"

namespace u {

struct zip {
    zip() = default;
    ~zip() = default;

    bool open(const u::string &file);
    bool create(const u::string &file);

    // Returns the contents of `file' as a vector of unsigned bytes
    // or u::none if `file' is not found in the zip
    u::optional<u::vector<unsigned char>> read(const u::string &ile);

    // Check if `file' exists in ZIP
    bool exists(const u::string &file);

    // Check if the ZIP is opened
    bool opened() const;

    // Write `length' bytes of `data' into ZIP with name `file'
    // Note: will DEFLATE data if the size of compressed is less than
    // the uncompressed data length using a compression ratio of
    // factor `strength'. Larger strength values equate to smaller
    // files at the cost of more back tracking.
    bool write(const u::string &file, const unsigned char *const data, size_t length, int strength = 4);

    // Write `data' into the ZIP with the name `file'
    bool write(const u::string &file, const u::vector<unsigned char> &data, int stength = 4);

    // Remove the file `file' in ZIP
    bool remove(const u::string &file);

    // Rename file `search' to `replace' in ZIP. Returns false if the
    // `search' file is not found or if `replace' already exists
    bool rename(const u::string &search, const u::string &replace);

protected:
    // An entry can be a file or directory
    struct entry {
        u::string name;
        bool compressed;
        size_t offset;
        size_t csize;
        size_t usize;
        uint32_t crc;
    };

    bool findCentralDirectory(unsigned char *);

private:
    u::file m_file;
    u::map<u::string, entry> m_entries;
};

inline bool zip::opened() const {
    return m_file.get();
}

inline bool zip::write(const u::string &file, const u::vector<unsigned char> &data, int stength) {
    return write(file, &data[0], data.size(), stength);
}

inline bool zip::exists(const u::string &file) {
    return m_entries.find(file) != m_entries.end();
}

}

#endif
