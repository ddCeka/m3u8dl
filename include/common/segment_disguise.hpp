#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace m3u8dl {

// Some CDNs disguise segments as image files (ad-blocking evasion) or
// gzip-compress them (seen with DDP/E-AC-3 audio). These helpers detect
// and reverse both before the segment is decrypted/merged.
class SegmentDisguise {
public:
    static bool is_image_header(std::span<const uint8_t> data);
    static bool is_gzip_header(std::span<const uint8_t> data);

    static std::vector<uint8_t> strip_image_header(std::span<const uint8_t> data);
    static std::vector<uint8_t> gunzip(std::span<const uint8_t> data);
};

}
