#include <time.h>

#include "u_zip.h"
#include "u_misc.h"
#include "u_zlib.h"

#include "engine.h" // neoUserPath

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
    uint16_t version, flags, compression, time, date;
    uint32_t crc, csize, usize;
    uint16_t fileNameLength, extraFieldLength;
};

struct centralDirectoryHead {
    static constexpr uint32_t kSignature = 0x02014b50;
    void endianSwap();
    uint32_t signature, : 32;
    uint16_t flags, compression, time, date;
    uint32_t crc, csize, usize;
    uint16_t fileNameLength, extraFieldLength, fileCommentLength;
    uint16_t : 16, : 16;
    uint32_t : 32, offset;
};

struct centralDirectoryTail {
    static constexpr uint32_t kSignature = 0x06054b50;
    void endianSwap();
    uint32_t signature;
    uint16_t : 16, : 16;
    uint16_t entriesDisk, entries;
    uint32_t size, offset;
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
    if (fseek(m_file, 0, SEEK_END) != 0)
        return false;
    const size_t length = ftell(m_file);
    if (fseek(m_file, 0, SEEK_SET) != 0)
        return false;
    // Smallest legal ZIP file contains only the end of a central directory
    if (length < 22)
        return false;
    // Find the end of the central directory
    if (fseek(m_file, length - 22, SEEK_SET) != 0)
        return false;
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

bool zip::create(const u::string &fileName) {
    if (!(m_file = u::fopen(fileName, "wb")))
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
    m_file.~file();
    return open(fileName);
}

bool zip::open(const u::string &fileName) {
    if (!(m_file = u::fopen((m_fileName = fileName), "r+b")))
        return false;

    centralDirectoryTail tail;
    if (!findCentralDirectory((unsigned char *)&tail))
        return false;
    tail.endianSwap();
    // We don't support ZIP files across multiple "files"
    if (tail.entriesDisk != tail.entries)
        return false;
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
        e.crc = head.crc;

        // Calculate where to seek to for the next central directory
        if (fseek(m_file, head.extraFieldLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, head.fileCommentLength, SEEK_CUR) != 0)
            return false;
        const long next = ftell(m_file);
        if (next == -1)
            return false;

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

u::optional<u::vector<unsigned char>> zip::read(const u::string &file) {
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
            decompressed.reserve(it.usize);
            if (!u::zlib::inflator().inflate(decompressed, &contents[0], contents.size()))
                return u::none;
            if (u::crc32(&decompressed[0], decompressed.size()) != it.crc)
                return u::none;
            return decompressed;
        } else if (u::crc32(&contents[0], contents.size()) != it.crc) {
            return u::none;
        }
        return contents;
    }
    return u::none;
}

#define ERROR() \
    do { printf("%s %d\n", __FILE__, __LINE__); return false; } while (0)

bool zip::write(const u::string &fileName, const unsigned char *const data, size_t length, int strength) {
    // Find the end of the central directory
    centralDirectoryTail tail;
    if (!findCentralDirectory((unsigned char *)&tail))
        return false;
    // Read the entire comment if any
    u::vector<unsigned char> savedComment;
    if (tail.commentLength) {
        savedComment.resize(tail.commentLength);
        if (fread(&savedComment[0], savedComment.size(), 1, m_file) != 1)
            return false;
    }

    // Seek to the first central directory entry
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;
    // Now skip all the central directory entries
    size_t firstLocalFileHeader = 0;
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
        firstLocalFileHeader = u::min(firstLocalFileHeader, size_t(head.offset));
    }

    // Skip all the local file headers
    if (fseek(m_file, firstLocalFileHeader, SEEK_SET) != 0)
        return false;
    for (size_t i = 0; i < tail.entries; i++) {
        localFileHeader current;
        if (fread(&current, sizeof current, 1, m_file) != 1)
            return false;
        current.endianSwap();
        if (fseek(m_file, current.fileNameLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, current.extraFieldLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, current.compression ? current.csize : current.usize, SEEK_CUR) != 0)
            return false;
    }

    // So we can seek here later to write the new local file header
    const size_t localFileHeaderEnd = ftell(m_file);

    // Read the entire central directory into memory
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;
    for (size_t i = 0; i < tail.entries; i++) {
        centralDirectoryHead entry;
        if (fread(&entry, sizeof entry, 1, m_file) != 1)
            return false;
        entry.endianSwap();
        if (fseek(m_file, entry.fileNameLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, entry.extraFieldLength, SEEK_CUR) != 0)
            return false;
        if (fseek(m_file, entry.fileCommentLength, SEEK_CUR) != 0)
            return false;
    }
    // Use the stream position to calculate length and read the whole thing
    // in in one read
    u::vector<unsigned char> entireCentralDirectory(ftell(m_file) - tail.offset);
    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;
    if (entireCentralDirectory.size() && fread(&entireCentralDirectory[0], entireCentralDirectory.size(), 1, m_file) != 1)
        return false;

    // Compress the data and see if that operation produces a smaller file
    // than the uncompressed contents

    u::vector<unsigned char> compressed;
    u::zlib::deflator().deflate(compressed, data, length, false, strength);
    bool isCompressed = compressed.size() < length;

    // The new local file header
    localFileHeader local;
    memset(&local, 0, sizeof local);
    local.signature = localFileHeader::kSignature;
    local.fileNameLength = fileName.size();
    local.crc = u::crc32(data, length);
    local.usize = length;
    local.csize = isCompressed ? compressed.size() : length;
    local.compression = isCompressed ? kCompressDeflate : kCompressNone;
    time_t currentTime = time(nullptr);
    struct tm *t = localtime(&currentTime);
    local.date = ((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
    local.time = (t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2);
    local.endianSwap();

    // Write the new local file header
    if (fseek(m_file, localFileHeaderEnd, SEEK_SET) != 0)
        return false;
    if (fwrite(&local, sizeof local, 1, m_file) != 1)
        return false;
    if (fwrite(&fileName[0], fileName.size(), 1, m_file) != 1)
        return false;
    // Write the contents of the file
    const size_t localFileContentsOffset = ftell(m_file);
    if (isCompressed) {
        if (fwrite(&compressed[0], compressed.size(), 1, m_file) != 1)
            return false;
    } else {
        if (fwrite(data, length, 1, m_file) != 1)
            return false;
    }

    // Write out the original central directories
    const size_t centralDirectoryOffset = ftell(m_file);
    if (entireCentralDirectory.size() && fwrite(&entireCentralDirectory[0], entireCentralDirectory.size(), 1, m_file) != 1)
        return false;

    // Write a new central directory
    local.endianSwap();
    centralDirectoryHead head;
    memset(&head, 0, sizeof head);
    head.signature = centralDirectoryHead::kSignature;
    head.compression = isCompressed ? kCompressDeflate : kCompressNone;
    head.time = local.time;
    head.date = local.date;
    head.crc = local.crc;
    head.csize = local.csize;
    head.usize = local.usize;
    head.fileNameLength = local.fileNameLength;
    head.offset = localFileHeaderEnd;
    head.endianSwap();
    if (fwrite(&head, sizeof head, 1, m_file) != 1)
        return false;
    if (fwrite(&fileName[0], fileName.size(), 1, m_file) != 1)
        return false;

    // Write the end of central directory
    tail.entries++;
    tail.entriesDisk++;
    tail.offset = centralDirectoryOffset;
    tail.size += sizeof(centralDirectoryHead) + fileName.size();
    tail.endianSwap();
    if (fwrite(&tail, sizeof tail, 1, m_file) != 1)
        return false;

    // Preserve the comment if any
    if (savedComment.size())
        if (fwrite(&savedComment[0], savedComment.size(), 1, m_file) != 1)
            return false;

    entry e;
    e.name = fileName;
    e.compressed = !!(head.compression == kCompressDeflate);
    e.usize = local.usize;
    e.csize = local.csize;
    e.offset = localFileContentsOffset;
    e.crc = local.crc;
    m_entries[e.name] = e;
    return true;
}

bool zip::remove(const u::string &file) {
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

    // Removal requires rebuilding the ZIP file basically
    u::string tempFileName;
    u::file tempFile;
    // Generate a temporary file name
    for (size_t i = 0; i < 128; i++) {
        tempFileName = u::fixPath(u::format("%s/tmp%zu", neoUserPath(), u::randu()));
        if (u::exists(tempFileName, kFile))
            continue;
        if (!(tempFile = u::fopen(tempFileName, "wb")))
            continue;
    }
    if (!tempFile)
        return false;

    for (size_t i = 0; i < tail.entries; i++) {
        localFileHeader current;
        if (fread(&current, sizeof current, 1, m_file) != 1)
            return false;
        current.endianSwap();
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
        localFileOffsets[fileName] = ftell(tempFile);
        current.endianSwap();
        if (fwrite(&current, sizeof current, 1, tempFile) != 1)
            return false;
        current.endianSwap();
        if (fwrite(&fileName[0], fileName.size(), 1, tempFile) != 1)
            return false;
        if (!copyFileContents(tempFile, m_file, current.extraFieldLength))
            return false;
        if (!copyFileContents(tempFile, m_file, current.compression ? current.csize : current.usize))
            return false;
    }

    if (fseek(m_file, tail.offset, SEEK_SET) != 0)
        return false;
    tail.offset = ftell(tempFile);
    for (size_t i = 0; i < tail.entries; i++) {
        centralDirectoryHead head;
        if (fread(&head, sizeof head, 1, m_file) != 1)
            return false;
        head.endianSwap();
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
        if (fwrite(&head, sizeof head, 1, tempFile) != 1)
            return false;
        head.endianSwap();
        if (fwrite(&fileName[0], fileName.size(), 1, tempFile) != 1)
            return false;
        if (!copyFileContents(tempFile, m_file, head.extraFieldLength))
            return false;
        if (!copyFileContents(tempFile, m_file, head.fileCommentLength))
            return false;
    }

    // Write the end of the central directory
    tail.entriesDisk--;
    tail.entries--;
    tail.size -= sizeof(centralDirectoryHead) + file.size();
    if (tail.size == 0)
        tail.offset = 0;
    tail.endianSwap();
    if (fwrite(&tail, sizeof tail, 1, tempFile) != 1)
        return false;

    // Preserve the comment if any
    tail.endianSwap();
    if (!copyFileContents(tempFile, m_file, tail.commentLength))
        return false;

    m_file.close();

    if (rename(tempFileName, m_fileName) != 0)
        return false;

    m_entries.erase(m_entries.find(file));

    return open(m_fileName);
}

bool zip::rename(const u::string &find, const u::string &replace) {
    if (exists(replace))
        return false;
    const auto &contents = read(find);
    if (!contents || !remove(find))
        return false;
    return write(replace, *contents);
}

}
