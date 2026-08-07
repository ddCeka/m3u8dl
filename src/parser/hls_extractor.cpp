#include "parser/hls_extractor.hpp"
#include "common/util.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdint>

namespace m3u8dl {

namespace {
    constexpr const char* TAG_EXTM3U = "#EXTM3U";
    constexpr const char* TAG_STREAM_INF = "#EXT-X-STREAM-INF:";
    constexpr const char* TAG_MEDIA = "#EXT-X-MEDIA:";
    constexpr const char* TAG_TARGETDURATION = "#EXT-X-TARGETDURATION:";
    constexpr const char* TAG_MEDIA_SEQUENCE = "#EXT-X-MEDIA-SEQUENCE:";
    constexpr const char* TAG_EXTINF = "#EXTINF:";
    constexpr const char* TAG_KEY = "#EXT-X-KEY:";
    constexpr const char* TAG_MAP = "#EXT-X-MAP:";
    constexpr const char* TAG_BYTERANGE = "#EXT-X-BYTERANGE:";
    constexpr const char* TAG_ENDLIST = "#EXT-X-ENDLIST";
    constexpr const char* TAG_DISCONTINUITY = "#EXT-X-DISCONTINUITY";

    std::vector<uint8_t> default_iv_from_index(int index) {
        std::vector<uint8_t> iv(16, 0);
        for (int i = 0; i < 4; ++i) {
            iv[15 - i] = static_cast<uint8_t>(index >> (8 * i));
        }
        return iv;
    }
}

HLSExtractor::HLSExtractor(const std::string& url, const std::string& base_url)
    : m3u8_url_(url), base_url_(base_url.empty() ? url : base_url) {}

std::string HLSExtractor::get_attribute(const std::string& line, const std::string& attr_name) {
    auto attrs = parse_attributes(line);
    auto it = attrs.find(attr_name);
    return it != attrs.end() ? it->second : "";
}

std::map<std::string, std::string> HLSExtractor::parse_attributes(const std::string& line) {
    std::map<std::string, std::string> attrs;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return attrs;
    }

    std::string attrs_str = line.substr(colon_pos + 1);
    std::regex attr_regex(R"(([A-Z\-]+)=(?:\"([^\"]*)\"|([^,]+)))");
    std::sregex_iterator iter(attrs_str.begin(), attrs_str.end(), attr_regex);
    std::sregex_iterator end;

    while (iter != end) {
        std::smatch match = *iter;
        std::string key = match[1].str();
        std::string value = match[2].matched ? match[2].str() : match[3].str();
        attrs[key] = value;
        ++iter;
    }

    return attrs;
}

std::vector<StreamSpec> HLSExtractor::extract_streams(const std::string& content) {
    m3u8_content_ = content;

    // Trim and validate
    size_t start = m3u8_content_.find_first_not_of(" \t\r\n");
    if (start != std::string::npos) {
        m3u8_content_ = m3u8_content_.substr(start);
    }

    if (m3u8_content_.find(TAG_EXTM3U) != 0) {
        throw std::runtime_error("Invalid M3U8 file");
    }

    if (m3u8_content_.find(TAG_STREAM_INF) != std::string::npos ||
        m3u8_content_.find(TAG_MEDIA) != std::string::npos) {
        is_master_playlist_ = true;
        return parse_master_playlist();
    } else {
        StreamSpec spec;
        spec.url = m3u8_url_;
        spec.playlist = parse_media_playlist();
        return {spec};
    }
}

std::vector<StreamSpec> HLSExtractor::parse_master_playlist() {
    std::vector<StreamSpec> streams;
    std::istringstream iss(m3u8_content_);
    std::string line;
    bool expect_playlist = false;
    StreamSpec current_spec;

    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) {
            continue;
        }

        if (line.find(TAG_STREAM_INF) == 0) {
            current_spec = StreamSpec();

            auto bandwidth_str = get_attribute(line, "AVERAGE-BANDWIDTH");
            if (bandwidth_str.empty()) {
                bandwidth_str = get_attribute(line, "BANDWIDTH");
            }
            current_spec.bandwidth = bandwidth_str.empty() ? 0 : std::stoll(bandwidth_str);

            current_spec.codecs = get_attribute(line, "CODECS");
            current_spec.resolution = get_attribute(line, "RESOLUTION");

            auto frame_rate_str = get_attribute(line, "FRAME-RATE");
            if (!frame_rate_str.empty()) {
                current_spec.frame_rate = std::stod(frame_rate_str);
            }

            current_spec.video_range = get_attribute(line, "VIDEO-RANGE");

            auto audio_group = get_attribute(line, "AUDIO");
            if (!audio_group.empty()) {
                current_spec.audio_group_id = audio_group;
            }
            auto subtitle_group = get_attribute(line, "SUBTITLES");
            if (!subtitle_group.empty()) {
                current_spec.subtitle_group_id = subtitle_group;
            }

            expect_playlist = true;
        } else if (line.find(TAG_MEDIA) == 0) {
            current_spec = StreamSpec();

            auto type_str = get_attribute(line, "TYPE");
            if (type_str == "AUDIO") {
                current_spec.media_type = MediaType::AUDIO;
            } else if (type_str == "SUBTITLES") {
                current_spec.media_type = MediaType::SUBTITLES;
            } else if (type_str == "VIDEO") {
                current_spec.media_type = MediaType::VIDEO;
            }

            auto url = get_attribute(line, "URI");
            if (url.empty()) {
                continue;
            }

            current_spec.url = Util::combine_url(base_url_, url);
            current_spec.group_id = get_attribute(line, "GROUP-ID");
            current_spec.language = get_attribute(line, "LANGUAGE");
            current_spec.name = get_attribute(line, "NAME");
            current_spec.channels = get_attribute(line, "CHANNELS");

            streams.push_back(current_spec);
        } else if (line[0] == '#') {
            continue;
        } else if (expect_playlist) {
            current_spec.url = Util::combine_url(base_url_, line);
            expect_playlist = false;
            streams.push_back(current_spec);
        }
    }

    return streams;
}

Playlist HLSExtractor::parse_media_playlist() {
    Playlist playlist;
    playlist.is_live = true;
    MediaPart current_part;
    MediaSegment current_segment;
    EncryptInfo current_encrypt_info;

    std::istringstream iss(m3u8_content_);
    std::string line;
    double segment_duration = 0.0;
    std::optional<std::pair<int64_t, int64_t>> next_range;
    int segment_index = 0;

    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) {
            continue;
        }

        if (line.find(TAG_EXTINF) == 0) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string duration_str = line.substr(colon + 1);
                size_t comma = duration_str.find(',');
                if (comma != std::string::npos) {
                    duration_str = duration_str.substr(0, comma);
                }
                segment_duration = std::stod(duration_str);
            }
        } else if (line.find(TAG_KEY) == 0) {
            current_encrypt_info = EncryptInfo();

            auto method_str = get_attribute(line, "METHOD");
            if (method_str == "AES-128") {
                current_encrypt_info.method = EncryptMethod::AES_128;
            } else if (method_str == "SAMPLE-AES") {
                current_encrypt_info.method = EncryptMethod::SAMPLE_AES;
            } else if (method_str == "NONE") {
                current_encrypt_info.method = EncryptMethod::NONE;
            }

            auto key_format = get_attribute(line, "KEYFORMAT");
            if (!key_format.empty() && key_format != "identity" &&
                current_encrypt_info.method != EncryptMethod::NONE) {
                current_encrypt_info.method = EncryptMethod::UNSUPPORTED_DRM;
            }

            auto key_uri = get_attribute(line, "URI");
            if (!key_uri.empty() && current_encrypt_info.method != EncryptMethod::UNSUPPORTED_DRM) {
                current_encrypt_info.key_url = Util::combine_url(base_url_, key_uri);
            }

            auto iv_str = get_attribute(line, "IV");
            if (!iv_str.empty()) {
                current_encrypt_info.iv = Util::hex_to_bytes(iv_str);
            }
        } else if (line.find(TAG_MAP) == 0) {
            auto uri = get_attribute(line, "URI");
            if (!uri.empty()) {
                MediaSegment init_seg;
                init_seg.url = Util::combine_url(base_url_, uri);

                auto byterange = get_attribute(line, "BYTERANGE");
                if (!byterange.empty()) {
                    size_t at_pos = byterange.find('@');
                    if (at_pos != std::string::npos) {
                        int64_t length = std::stoll(byterange.substr(0, at_pos));
                        int64_t offset = std::stoll(byterange.substr(at_pos + 1));
                        init_seg.start_range = offset;
                        init_seg.stop_range = offset + length - 1;
                    }
                }

                if (current_encrypt_info.method != EncryptMethod::NONE) {
                    init_seg.encrypt_info = current_encrypt_info;
                    if (!init_seg.encrypt_info.iv) {
                        init_seg.encrypt_info.iv = default_iv_from_index(segment_index);
                    }
                }

                playlist.media_init = init_seg;
            }
        } else if (line.find(TAG_BYTERANGE) == 0) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string range_str = line.substr(colon + 1);
                size_t at_pos = range_str.find('@');
                if (at_pos != std::string::npos) {
                    int64_t length = std::stoll(range_str.substr(0, at_pos));
                    int64_t offset = std::stoll(range_str.substr(at_pos + 1));
                    next_range = {offset, offset + length - 1};
                } else {
                    int64_t length = std::stoll(range_str);
                    int64_t offset = current_segment.stop_range.value_or(-1) + 1;
                    next_range = {offset, offset + length - 1};
                }
            }
        } else if (line.find(TAG_ENDLIST) == 0) {
            playlist.is_live = false;
        } else if (line == TAG_DISCONTINUITY) {
            if (!current_part.segments.empty()) {
                playlist.media_parts.push_back(current_part);
                current_part = MediaPart();
            }
        } else if (line[0] != '#') {
            current_segment = MediaSegment();
            current_segment.url = Util::combine_url(base_url_, line);
            current_segment.duration = segment_duration;
            current_segment.encrypt_info = current_encrypt_info;
            current_segment.index = segment_index++;

            if (current_segment.encrypt_info.method != EncryptMethod::NONE &&
                !current_segment.encrypt_info.iv) {
                current_segment.encrypt_info.iv = default_iv_from_index(current_segment.index);
            }

            if (next_range) {
                current_segment.start_range = next_range->first;
                current_segment.stop_range = next_range->second;
                next_range.reset();
            }

            current_part.segments.push_back(current_segment);
            playlist.total_duration += segment_duration;
            segment_duration = 0.0;
        }
    }

    if (!current_part.segments.empty()) {
        playlist.media_parts.push_back(current_part);
    }

    return playlist;
}

}
