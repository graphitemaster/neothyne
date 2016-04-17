#include <time.h>

#include "u_zip.h"
#include "u_misc.h"
#include "u_zlib.h"

namespace u {

// Copy contents from `src' to `dst' in block sized chunks
static bool copyFileContents(u::file &dst, u::file &src, size_t bytes) {
    if (bytes == 0)
        return true;
    // Copy bytes from `src' into `dst' in an efficient way
    unsigned char block[4096];
    for (size_t j = 0; j <= 12; j++) {
        const size_t blockCount = bytes / (sizeof block >> j);
        const size_t blockRemain = bytes % (sizeof block >> j);
        for (size_t i = 0; i < blockCount; i++) {
            if (fread(block, sizeof block >> j, 1, src) != 1)
                return false;
            if (fwrite(block, sizeof block >> j, 1, dst) != 1)
                return false;
            bytes -= sizeof block >> j;
        }
        if (blockRemain == 0)
            break;
    }
    return true;
}

static constexpr uint16_t kCompressNone = 0;
static constexpr uint16_t kCompressDeflate = 8;

#pragma pack(push, 1)
struct localFileHeader {
    static constexpr uint32_t kSignature = 0x04034b50;
    void endianSwap();
    uint32_t signature;
    uint16_t version;
    uint16_t flags;
    uint16_t compression;
    uint16_t time;
    uint16_t date;
    uint32_t crc;
    uint32_t csize;
    uint32_t usize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};

struct centralDirectoryHead {
    static constexpr uint32_t kSignature = 0x02014b50;
    void endianSwap();
    uint32_t signature;
    uint32_t : 32;
    uint16_t flags;
    uint16_t compression;
    uint16_t time;
    uint16_t date;
    uint32_t crc;
    uint32_t csize;
    uint32_t usize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t : 16;
    uint16_t : 16;
    uint32_t : 32;
    uint32_t offset;
};

struct centralDirectoryTail {
    static constexpr uint32_t kSignature = 0x06054b50;
    void endianSwap();
    uint32_t signature;
    uint16_t : 16;
    uint16_t : 16;
    uint16_t entriesDisk;
    uint16_t entries;
    uint32_t size;
    uint32_t offset;
    uint16_t commentLength;
};
#pragma pack(pop)

inline void localFileHeader::endianSwap() {
    u::endianSwap(&signature, 1);
    u::endianSwap(&version, 5);
    u::endianSwap(&crc, 3);
    u::endianSwap(&fileNameLength, 2);
}

inline void centralDirectoryHead::endianSwap() {
    u::endianSwap(&signature, 2);
    u::endianSwap(&flags, 4);
    u::endianSwap(&crc, 3);
    u::endianSwap(&fileNameLength, 5);
    u::endianSwap(&offset, 1);
}

inline void centralDirectoryTail::endianSwap() {
    u::endianSwap(&signature, 1);
    u::endianSwap(&entriesDisk, 2);
    u::endianSwap(&size, 2);
    u::endianSwap(&commentLength, 1);
}

bool zip::findCentralDirectory(unsigned char *store) {
    // Calculate size of ZIP
    fseek(m_file, 0, SEEK_END);
    const size_t length = ftell(m_file);
    fseek(m_file, 0, SEEK_SET);
    // Smallest legal ZIP file contains only the end of a central directory
    if (length < 22)
        return false;
    // Find the end of the central directory
    fseek(m_file, length - 22, SEEK_SET);
    for (;;) {
        long consumed = 0;
        uint32_t signature = 0;
        if (fread(&signature, sizeof signature, 1, m_file) != 1)
            return false;
        signature = u::endianSwap(signature);
        consumed += sizeof signature;
        if (signature == centralDirectoryTail::kSignature) {
            // Check to make sure this isn't the comment section or some other
            // field in the central directory that is trying to mimic the
            // signature
            if (fseek(m_file, 16, SEEK_CUR) != 0)
                return false;
            consumed += 16;
            uint16_t commentLength = 0;
            if (fread(&commentLength, sizeof commentLength, 1, m_file) != 1)
                return false;
            commentLength = u::endianSwap(commentLength);
            consumed += sizeof commentLength;
            if (commentLength == 0 && ftell(m_file) == long(length)) {
                // No comment and reached end of file: this is a valid central
                // directory: undo the consumed bytes
                if (fseek(m_file, -consumed, SEEK_CUR) != 0)
                    return false;
                goto found;
            }
            // This may be a comment: seek the comment length and check to
            // see if that brings us to the end of the file. If it does
            // than this is a valid central directory.
            if (fseek(m_file, commentLength, SEEK_CUR) != 0)
                return false;
            consumed += commentLength;
            if (ftell(m_file) == long(length)) {
                // This is a valid central directory: roll back
                if (fseek(m_file, -consumed, SEEK_CUR) != 0)
                    return false;
                goto found;
            }
            // Not a valid central directory: roll back
            if (fseek(m_file, -consumed, SEEK_CUR) != 0)
                return false;
        }
        // Shift back a byte and try again
        if (fseek(m_file, -1, SEEK_CUR) != 0)
            return false;
    }
    return false;
found:
    // Read the central directory tail end
    centralDirectoryTail *tail = (centralDirectoryTail *)store;
    if (fread(tail, sizeof *tail, 1, m_file) != 1)
        return false;
    tail->endianSwap();
    if (tail->signature != centralDirectoryTail::kSignature)
        return false;
    return true;
}


bool zip::create(const char *file) {
    if (!(m_file = u::fopen(file, "wb")))
        return false;
    centralDirectoryTail tail;
    memset(&tail, 0, sizeof tail);
    tail.signature = centralDirectoryTail::kSignature;
    tail.entriesDisk = 0;
    tail.entries = 0;
    tail.size = 0;
    tail.offset = 0;
    tail.endianSwap();
    if (fwrite(&tail, sizeof tail, 1, m_file) != 1)
        return false;
    fclose(m_file);
    return open(file);
}

bool zip::open(const char *file) {
    if (!(m_file = u::fopen(file, "r+b")))
        return false;

    centralDirectoryTail tail;
    if (!findCentralDirectory((unsigned char *)&tail))
        return false;
    tail.endianSwap();
    // We don't support ZIP files across multiple "files"
    if (tail.entriesDisk != tail.entries)
        return false;

    // Seek to the beginning of the central directory
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;

    // Read the central directory entries
    for (size_t i = 0; i < tail.entries; i++) {
        centralDirectoryHead head;
        if (fread(&head, sizeof head, 1, m_file) != 1)
            return false;
        head.endianSwap();
        if (head.signature != centralDirectoryHead::kSignature)
            return false;

        // Construct an entry
        entry e;
        e.name.resize(head.fileNameLength);
        if (fread(&e.name[0], head.fileNameLength, 1, m_file) != 1)
            return false;
        // Only support uncompressed files or DEFLATE
        if (head.compression != kCompressNone && head.compression != kCompressDeflate)
            return false;
        e.compressed = !!(head.compression == kCompressDeflate);
        e.csize = head.csize;
        e.usize = head.usize;

        // Calculate where to seek to for the next central directory
        if (fseek(m_file, head.extraFieldLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, head.fileCommentLength, SEEK_CUR) != 0)
            return false;
        const size_t next = ftell(m_file);

        // Seek to the local header
        localFileHeader local;
        if (fseek(m_file, head.offset, SEEK_SET) != 0)
            return false;
        if (fread(&local, sizeof local, 1, m_file) != 1)
            return false;
        local.endianSwap();
        if (local.signature != localFileHeader::kSignature)
            return false;
        // Check to make sure this local header corresponds to the same central
        // directory: if it does not then the file is corrupted
        if (local.csize != head.csize || local.usize != head.usize || local.crc != head.crc)
            return false;

        // Calculate offset of file contents
        if (fseek(m_file, local.fileNameLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, local.extraFieldLength, SEEK_CUR) != 0)
            return false;
        e.offset = ftell(m_file);

        // Add the entry and roll back to the next central directory
        if (e.name.end()[-1] != '/')
            m_entries[e.name] = e;
        if (fseek(m_file, next, SEEK_SET) != 0)
            return false;
    }
    return true;
}

u::optional<u::vector<unsigned char>> zip::read(const char *file) {
    for (const auto &e : m_entries) {
        const auto &it = e.second;
        if (it.name != file)
            continue;
        if (fseek(m_file, it.offset, SEEK_SET) != 0)
            return u::none;
        const size_t size = it.compressed ? it.csize : it.usize;
        u::vector<unsigned char> contents;
        contents.resize(size);
        if (fread(&contents[0], size, 1, m_file) != 1)
            return u::none;
        if (it.compressed) {
            u::vector<unsigned char> decompressed;
            if (!u::zlib::inflator().inflate(decompressed, &contents[0], contents.size()))
                return u::none;
            return decompressed;
        }
        return contents;
    }
    return u::none;
}

bool zip::write(const char *file, const unsigned char *const data, size_t length, int strength) {
    const size_t fileNameLength = strlen(file);

    // Find the end of the central directory
    centralDirectoryTail tail;
    if (!findCentralDirectory((unsigned char *)&tail))
        return false;

    // Seek to the beginning of the central directory
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;

    // Find the end of the central directory list
    for (size_t i = 0; i < tail.entries; i++) {
        centralDirectoryHead head;
        if (fread(&head, sizeof head, 1, m_file) != 1)
            return false;
        head.endianSwap();
        if (fseek(m_file, head.fileNameLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, head.extraFieldLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, head.fileCommentLength, SEEK_CUR) != 0)
            return false;
    }

    u::file temp = tmpfile();

    // Copy all the local file headers
    if (fseek(m_file, 0, SEEK_SET) != 0)
        return false;
    for (size_t i = 0; i < tail.entries; i++) {
        localFileHeader current;
        if (fread(&current, sizeof current, 1, m_file) != 1)
            return false;
        if (fwrite(&current, sizeof current, 1, temp) != 1)
            return false;
        current.endianSwap();
        // Copy file name
        if (!copyFileContents(temp, m_file, current.fileNameLength))
            return false;
        // Copy the extra field if there is any
        if (!copyFileContents(temp, m_file, current.extraFieldLength))
            return false;
        // Copy the contents of the file
        if (!copyFileContents(temp, m_file, current.compression ? current.csize : current.usize))
            return false;
    }

    const size_t localFileHeaderOffset = ftell(temp);

    // DEFLATE the data and check if it's smaller than what is supplied
    u::vector<unsigned char> compressed;
    u::zlib::deflator().deflate(compressed, data, length, strength);
    bool isCompressed = compressed.size() < length;

    // Create a local file header
    localFileHeader local;
    memset(&local, 0, sizeof local);
    local.signature = localFileHeader::kSignature;
    local.fileNameLength = fileNameLength;
    if (isCompressed) {
        local.compression = kCompressDeflate;
        local.crc = u::crc32(&compressed[0], compressed.size());
        local.csize = compressed.size();
        local.usize = length;
    } else {
        local.compression = kCompressNone;
        local.crc = u::crc32(data, length);
        local.csize = length;
        local.usize = length;
    }
    time_t currentTime = time(nullptr);
    struct tm *t = localtime(&currentTime);
    local.date = ((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
    local.time = (t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2);

    // Write the new local file header
    local.endianSwap();
    if (fwrite(&local, sizeof local, 1, temp) != 1)
        return false;
    // Write the file name
    if (fwrite(file, fileNameLength, 1, temp) != 1)
        return false;
    // Write the contents of the file
    const size_t localFileContentsOffset = ftell(temp);
    if (isCompressed) {
        if (fwrite(&compressed[0], compressed.size(), 1, temp) != 1)
            return false;
    } else {
        if (fwrite(data, length, 1, temp) != 1)
            return false;
    }

    // Copy the central directories
    const size_t centralDirectoryOffset = ftell(temp);
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;
    for (size_t i = 0; i < tail.entries; i++) {
        centralDirectoryHead entry;
        if (fread(&entry, sizeof entry, 1, m_file) != 1)
            return false;
        if (fwrite(&entry, sizeof entry, 1, temp) != 1)
            return false;
        entry.endianSwap();
        if (!copyFileContents(temp, m_file, entry.fileNameLength))
            return false;
        if (!copyFileContents(temp, m_file, entry.extraFieldLength))
            return false;
        if (!copyFileContents(temp, m_file, entry.fileCommentLength))
            return false;
    }

    // Write a new central directory
    local.endianSwap();
    centralDirectoryHead head;
    memset(&head, 0, sizeof head);
    head.signature = centralDirectoryHead::kSignature;
    head.compression = isCompressed ? 8 : 0;
    head.time = local.time;
    head.date = local.date;
    head.crc = local.crc;
    head.csize = local.csize;
    head.usize = local.usize;
    head.fileNameLength = local.fileNameLength;
    head.offset = localFileHeaderOffset;
    head.endianSwap();
    if (fwrite(&head, sizeof head, 1, temp) != 1)
        return false;
    if (fwrite(file, fileNameLength, 1, temp) != 1)
        return false;

    // Write the end of central directory
    tail.entries++;
    tail.entriesDisk++;
    tail.offset = centralDirectoryOffset;
    tail.size += sizeof(centralDirectoryHead) + fileNameLength;
    tail.endianSwap();
    if (fwrite(&tail, sizeof tail, 1, temp) != 1)
        return false;

    // Preserve the comment if any
    tail.endianSwap();
    if (!copyFileContents(temp, m_file, tail.commentLength))
        return false;

    // Now seek to the beginning of m_file and copy the contents of the temp
    // file into the zip file
    const size_t totalSize = ftell(temp);
    if (fseek(m_file, 0, SEEK_SET) != 0)
        return false;
    if (fseek(temp, 0, SEEK_SET) != 0)
        return false;
    if (!copyFileContents(m_file, temp, totalSize))
        return false;

    // Add it to the entries list
    entry e;
    e.name = file;
    e.usize = local.usize;
    e.csize = local.csize;
    e.offset = localFileContentsOffset;
    m_entries[e.name] = e;
    return true;
}

bool zip::remove(const char *file) {
    if (!exists(file))
        return false;
    centralDirectoryTail tail;
    if (!findCentralDirectory((unsigned char *)&tail))
        return false;
    if (tail.entriesDisk == 0)
        return false;

    // Read the first central directory to get the first local header
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;
    centralDirectoryHead head;
    if (fread(&head, sizeof head, 1, m_file) != 1)
        return false;
    head.endianSwap();
    if (fseek(m_file, head.offset, SEEK_SET) != 0)
        return false;

    // Copy all local file headers except the one we wish to remove
    u::map<u::string, size_t> localFileOffsets;
    u::file temp = tmpfile();
    for (size_t i = 0; i < tail.entries; i++) {
        localFileHeader current;
        if (fread(&current, sizeof current, 1, m_file) != 1)
            return false;
        current.endianSwap();
        // Copy file name
        u::string fileName;
        fileName.resize(current.fileNameLength);
        if (fread(&fileName[0], fileName.size(), 1, m_file) != 1)
            return false;
        if (fileName == file) {
            // Ignore this file since it's the one we're removing
            if (fseek(m_file, current.extraFieldLength, SEEK_CUR) != 0)
                return false;
            if (fseek(m_file, current.compression ? current.csize : current.usize, SEEK_CUR) != 0)
                return false;
            continue;
        }
        // Remember where in the stream this local file header will
        // be written for the central directory entry
        localFileOffsets[fileName] = ftell(temp);
        current.endianSwap();
        if (fwrite(&current, sizeof current, 1, temp) != 1)
            return false;
        current.endianSwap();
        if (fwrite(&fileName[0], fileName.size(), 1, temp) != 1)
            return false;
        if (!copyFileContents(temp, m_file, current.extraFieldLength))
            return false;
        if (!copyFileContents(temp, m_file, current.compression ? current.csize : current.usize))
            return false;
    }

    // Write the central directories
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;

    tail.offset = ftell(temp);
    for (size_t i = 0; i < tail.entries; i++) {
        centralDirectoryHead head;
        if (fread(&head, sizeof head, 1, m_file) != 1)
            return false;
        head.endianSwap();
        // Copy file name
        u::string fileName;
        fileName.resize(head.fileNameLength);
        if (fread(&fileName[0], fileName.size(), 1, m_file) != 1)
            return false;
        if (fileName == file) {
            // Ignore this file since it's the one we're removing
            if (fseek(m_file, head.extraFieldLength, SEEK_CUR) != 0)
                return false;
            if (fseek(m_file, head.fileCommentLength, SEEK_CUR) != 0)
                return false;
            continue;
        }
        head.offset = localFileOffsets[fileName];
        head.endianSwap();
        if (fwrite(&head, sizeof head, 1, temp) != 1)
            return false;
        head.endianSwap();
        if (fwrite(&fileName[0], fileName.size(), 1, temp) != 1)
            return false;
        if (!copyFileContents(temp, m_file, head.extraFieldLength))
            return false;
        if (!copyFileContents(temp, m_file, head.fileCommentLength))
            return false;
    }

    // Write the end of the central directory
    tail.entriesDisk--;
    tail.entries--;
    tail.size -= sizeof(centralDirectoryHead) + strlen(file);
    if (tail.size == 0)
        tail.offset = 0;
    tail.endianSwap();
    if (fwrite(&tail, sizeof tail, 1, temp) != 1)
        return false;

    // Preserve the comment if any
    tail.endianSwap();
    if (!copyFileContents(temp, m_file, tail.commentLength))
        return false;

    // Remove it from m_entries
    m_entries.erase(m_entries.find(file));

    // Now seek to the beginning of m_file and copy the contents of the temp
    // file into the zip file
    const size_t totalSize = ftell(temp);
    if (fseek(m_file, 0, SEEK_SET) != 0)
        return false;
    if (fseek(temp, 0, SEEK_SET) != 0)
        return false;
    if (!copyFileContents(m_file, temp, totalSize))
        return false;
    return u::truncate(m_file, totalSize);
}

}
