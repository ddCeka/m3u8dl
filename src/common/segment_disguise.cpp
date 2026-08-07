#include "common/segment_disguise.hpp"
#include <zlib.h>
#include <stdexcept>

namespace m3u8dl {

bool SegmentDisguise::is_image_header(std::span<const uint8_t> d) {
    if (d.size() > 3 && d[0] == 137 && d[1] == 80 && d[2] == 78 && d[3] == 71) {
        return true; // PNG
    }
    if (d.size() > 3 && d[0] == 0x47 && d[1] == 0x49 && d[2] == 0x46 && d[3] == 0x38) {
        return true; // GIF
    }
    if (d.size() > 10 && d[0] == 0x42 && d[1] == 0x4D &&
        d[5] == 0 && d[6] == 0 && d[7] == 0 && d[8] == 0) {
        return true; // BMP
    }
    if (d.size() > 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) {
        return true; // JPEG
    }
    return false;
}

bool SegmentDisguise::is_gzip_header(std::span<const uint8_t> d) {
    return d.size() > 2 && d[0] == 0x1f && d[1] == 0x8b;
}

namespace {
size_t find_ts_sync(std::span<const uint8_t> d, size_t start) {
    if (d.size() < 188 * 2 + 4) {
        return 0;
    }
    for (size_t i = start; i < d.size() - 188 * 2 - 4; ++i) {
        if (d[i] == 0x47 && d[i + 188] == 0x47 && d[i + 188 + 188] == 0x47) {
            return i;
        }
    }
    return 0;
}
}

std::vector<uint8_t> SegmentDisguise::strip_image_header(std::span<const uint8_t> d) {
    if (d.size() > 3 && d[0] == 137 && d[1] == 80 && d[2] == 78 && d[3] == 71) {
        size_t skip = 0;
        if (d.size() > 120 && d[118] == 96 && d[119] == 130) {
            skip = 120;
        } else if (d.size() > 6102 && d[6100] == 96 && d[6101] == 130) {
            skip = 6102;
        } else if (d.size() > 69 && d[67] == 96 && d[68] == 130) {
            skip = 69;
        } else if (d.size() > 771 && d[769] == 96 && d[770] == 130) {
            skip = 771;
        } else {
            skip = find_ts_sync(d, 4);
        }
        return std::vector<uint8_t>(d.begin() + static_cast<ptrdiff_t>(skip), d.end());
    }
    if (d.size() > 3 && d[0] == 0x47 && d[1] == 0x49 && d[2] == 0x46 && d[3] == 0x38) {
        size_t skip = d.size() > 42 ? 42 : d.size();
        return std::vector<uint8_t>(d.begin() + static_cast<ptrdiff_t>(skip), d.end());
    }
    if (d.size() > 10 && d[0] == 0x42 && d[1] == 0x4D &&
        d[5] == 0 && d[6] == 0 && d[7] == 0 && d[8] == 0) {
        size_t skip = d.size() > 0x3E ? 0x3E : d.size();
        return std::vector<uint8_t>(d.begin() + static_cast<ptrdiff_t>(skip), d.end());
    }
    if (d.size() > 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) {
        size_t skip = find_ts_sync(d, 4);
        return std::vector<uint8_t>(d.begin() + static_cast<ptrdiff_t>(skip), d.end());
    }
    return std::vector<uint8_t>(d.begin(), d.end());
}

std::vector<uint8_t> SegmentDisguise::gunzip(std::span<const uint8_t> data) {
    z_stream strm{};
    if (inflateInit2(&strm, 15 + 16) != Z_OK) {
        throw std::runtime_error("Failed to initialize gzip decompression");
    }

    std::vector<uint8_t> out;
    std::vector<uint8_t> chunk(64 * 1024);

    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_in = static_cast<uInt>(data.size());

    int ret = Z_OK;
    do {
        strm.next_out = chunk.data();
        strm.avail_out = static_cast<uInt>(chunk.size());
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            throw std::runtime_error("gzip decompression failed");
        }
        size_t produced = chunk.size() - strm.avail_out;
        out.insert(out.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(produced));
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return out;
}

}
