#pragma once

#include "common/types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <span>

namespace m3u8dl {

class Util {
public:
    static std::string combine_url(const std::string& base, const std::string& relative);
    static std::string get_valid_filename(const std::string& name, const std::string& replacement = "-");
    static bool is_absolute_url(const std::string& url);

    static std::string format_save_pattern(const std::string& pattern, const StreamSpec& stream,
                                            const std::string& save_name, int task_id);

    static std::vector<uint8_t> hex_to_bytes(const std::string& hex);
    static std::string bytes_to_hex(std::span<const uint8_t> bytes);

    static std::vector<uint8_t> base64_decode(const std::string& encoded);
    static std::string base64_encode(std::span<const uint8_t> data);

    static bool file_exists(const std::string& path);
    static void create_directories(const std::string& path);

    static std::optional<std::vector<uint8_t>> read_file_bytes(const std::string& path);
    static bool write_file_bytes(const std::string& path, std::span<const uint8_t> data);

    static bool is_mpeg_ts_buffer(std::span<const uint8_t> buffer);
};

}
