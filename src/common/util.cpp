#include "common/util.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <cctype>

namespace m3u8dl {

namespace {

std::string normalize_path(const std::string& path) {
    std::vector<std::string> segments;
    std::istringstream iss(path);
    std::string segment;
    bool leading_slash = !path.empty() && path.front() == '/';
    bool trailing_slash = !path.empty() && path.back() == '/';

    while (std::getline(iss, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            }
            continue;
        }
        segments.push_back(segment);
    }

    std::string result;
    if (leading_slash) {
        result += "/";
    }
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            result += "/";
        }
        result += segments[i];
    }
    if (trailing_slash && !result.empty() && result.back() != '/') {
        result += "/";
    }
    if (result.empty()) {
        result = "/";
    }
    return result;
}

}

std::string Util::combine_url(const std::string& base, const std::string& relative) {
    if (base.empty()) {
        return relative;
    }

    if (is_absolute_url(relative)) {
        return relative;
    }

    if (relative.empty()) {
        return base;
    }

    size_t scheme_end = base.find("://");
    if (scheme_end == std::string::npos) {
        return base + relative;
    }
    std::string scheme_and_authority_prefix = base.substr(0, scheme_end + 3);
    size_t authority_end = base.find('/', scheme_end + 3);
    std::string authority = authority_end != std::string::npos
        ? base.substr(scheme_end + 3, authority_end - (scheme_end + 3))
        : base.substr(scheme_end + 3);

    if (relative.size() >= 2 && relative[0] == '/' && relative[1] == '/') {
        return base.substr(0, scheme_end + 3) + relative.substr(2);
    }

    if (relative[0] == '/') {
        return scheme_and_authority_prefix + authority + normalize_path(relative);
    }

    if (relative[0] == '?' || relative[0] == '#') {
        return base + relative;
    }

    std::string base_path = authority_end != std::string::npos
        ? base.substr(authority_end)
        : "/";
    size_t last_slash = base_path.find_last_of('/');
    std::string base_dir = last_slash != std::string::npos
        ? base_path.substr(0, last_slash + 1)
        : "/";

    return scheme_and_authority_prefix + authority + normalize_path(base_dir + relative);
}

std::string Util::get_valid_filename(const std::string& name, const std::string& replacement) {
    std::string result = name;
    const std::string invalid_chars = R"(<>:"/\|?*)";

    for (char& c : result) {
        if (invalid_chars.find(c) != std::string::npos) {
            c = replacement[0];
        }
    }

    return result;
}

namespace {
void replace_all_inplace(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

const char* media_type_name(MediaType type) {
    switch (type) {
        case MediaType::VIDEO: return "VIDEO";
        case MediaType::AUDIO: return "AUDIO";
        case MediaType::SUBTITLES: return "SUBTITLES";
    }
    return "";
}
}

std::string Util::format_save_pattern(const std::string& pattern, const StreamSpec& stream,
                                       const std::string& save_name, int task_id) {
    std::string result = pattern;

    replace_all_inplace(result, "<SaveName>", save_name);
    replace_all_inplace(result, "<Id>", std::to_string(task_id));
    replace_all_inplace(result, "<Codecs>", stream.codecs.value_or(""));
    replace_all_inplace(result, "<Language>", stream.language.value_or(""));
    replace_all_inplace(result, "<Bandwidth>", stream.bandwidth > 0 ? std::to_string(stream.bandwidth) : "");
    replace_all_inplace(result, "<Resolution>", stream.resolution.value_or(""));
    replace_all_inplace(result, "<FrameRate>", stream.frame_rate ? std::to_string(*stream.frame_rate) : "");
    replace_all_inplace(result, "<Channels>", stream.channels.value_or(""));
    replace_all_inplace(result, "<VideoRange>", stream.video_range.value_or(""));
    replace_all_inplace(result, "<MediaType>", stream.media_type ? media_type_name(*stream.media_type) : "");
    replace_all_inplace(result, "<GroupId>", stream.group_id.value_or(""));

    replace_all_inplace(result, "__", "_");
    replace_all_inplace(result, "..", ".");

    size_t begin = result.find_first_not_of("_.");
    size_t end = result.find_last_not_of("_.");
    result = (begin == std::string::npos) ? "" : result.substr(begin, end - begin + 1);

    return get_valid_filename(result);
}

bool Util::is_absolute_url(const std::string& url) {
    return url.find("://") != std::string::npos;
}

std::vector<uint8_t> Util::hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::string clean_hex = hex;

    if (clean_hex.size() >= 2 && clean_hex[0] == '0' && (clean_hex[1] == 'x' || clean_hex[1] == 'X')) {
        clean_hex = clean_hex.substr(2);
    }

    for (size_t i = 0; i < clean_hex.length(); i += 2) {
        std::string byte_str = clean_hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

std::string Util::bytes_to_hex(std::span<const uint8_t> bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (uint8_t byte : bytes) {
        oss << std::setw(2) << static_cast<int>(byte);
    }

    return oss.str();
}

std::vector<uint8_t> Util::base64_decode(const std::string& encoded) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> decoded;
    std::vector<int> T(256, -1);

    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }

    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return decoded;
}

std::string Util::base64_encode(std::span<const uint8_t> data) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    int val = 0, valb = -6;

    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (encoded.size() % 4) {
        encoded.push_back('=');
    }

    return encoded;
}

bool Util::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

void Util::create_directories(const std::string& path) {
    std::filesystem::create_directories(path);
}

std::optional<std::vector<uint8_t>> Util::read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return std::nullopt;
    }

    return buffer;
}

bool Util::write_file_bytes(const std::string& path, std::span<const uint8_t> data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool Util::is_mpeg_ts_buffer(std::span<const uint8_t> buffer) {
    if (buffer.size() < 188) {
        return false;
    }

    for (size_t i = 0; i < std::min(buffer.size(), size_t(188 * 3)); i += 188) {
        if (buffer[i] != 0x47) {
            return false;
        }
    }

    return true;
}

}
