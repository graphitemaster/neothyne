#include <string.h>
#include <zlib.h>

#include "util.h"

namespace u {
    vector<unsigned char> compress(const vector<unsigned char> &data) {
         std::vector<unsigned char> buffer;

         const size_t kBufferSize = 128 * 1024;
         unsigned char temp[kBufferSize];

         z_stream strm;
         strm.zalloc = nullptr;
         strm.zfree = nullptr;
         strm.next_in = (Bytef*)&*data.begin();
         strm.avail_in = data.size();
         strm.next_out = temp;
         strm.avail_out = kBufferSize;

         deflateInit(&strm, Z_BEST_COMPRESSION);

         while (strm.avail_in != 0) {
            deflate(&strm, Z_NO_FLUSH);
            if (strm.avail_out == 0) {
                buffer.insert(buffer.end(), temp, temp + kBufferSize);
                strm.next_out = temp;
                strm.avail_out = kBufferSize;
            }
         }

         int res = Z_OK;
         while (res == Z_OK) {
            if (strm.avail_out == 0) {
                buffer.insert(buffer.end(), temp, temp + kBufferSize);
                strm.next_out = temp;
                strm.avail_out = kBufferSize;
            }
            res = deflate(&strm, Z_FINISH);
         }

         buffer.insert(buffer.end(), temp, temp + kBufferSize - strm.avail_out);
         deflateEnd(&strm);

         return buffer;
    }

    vector<unsigned char> decompress(const vector<unsigned char> &data) {
         std::vector<unsigned char> buffer;

         const size_t kBufferSize = 128 * 1024;
         unsigned char temp[kBufferSize];

         z_stream strm;
         strm.zalloc = nullptr;
         strm.zfree = nullptr;
         strm.next_in = (Bytef*)&*data.begin();
         strm.avail_in = data.size();
         strm.next_out = temp;
         strm.avail_out = kBufferSize;

         inflateInit(&strm);

         while (strm.avail_in != 0) {
            inflate(&strm, Z_NO_FLUSH);
            if (strm.avail_out == 0) {
                buffer.insert(buffer.end(), temp, temp + kBufferSize);
                strm.next_out = temp;
                strm.avail_out = kBufferSize;
            }
         }

         int res = Z_OK;
         while (res == Z_OK) {
            if (strm.avail_out == 0) {
                buffer.insert(buffer.end(), temp, temp + kBufferSize);
                strm.next_out = temp;
                strm.avail_out = kBufferSize;
            }
            res = inflate(&strm, Z_FINISH);
         }

         buffer.insert(buffer.end(), temp, temp + kBufferSize - strm.avail_out);
         inflateEnd(&strm);

         return buffer;
    }
}
