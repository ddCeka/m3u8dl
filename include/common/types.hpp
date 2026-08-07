#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>

namespace m3u8dl {

enum class MediaType {
    VIDEO,
    AUDIO,
    SUBTITLES
};

enum class EncryptMethod {
    NONE,
    AES_128,
    AES_128_ECB,
    CHACHA20,
    SAMPLE_AES, // TODO: Parsed but not yet implemented
    UNSUPPORTED_DRM
};

enum class ExtractorType {
    HLS,
    MPEG_DASH
};

struct EncryptInfo {
    EncryptMethod method = EncryptMethod::NONE;
    std::optional<std::vector<uint8_t>> key;
    std::optional<std::vector<uint8_t>> iv;
    std::optional<std::string> key_url;
    std::optional<std::string> kid;
};

struct MediaSegment {
    std::string url;
    double duration = 0.0;
    std::optional<int64_t> start_range;
    std::optional<int64_t> stop_range;
    EncryptInfo encrypt_info;
    int index = 0;
};

struct MediaPart {
    std::vector<MediaSegment> segments;
};

struct Playlist {
    std::vector<MediaPart> media_parts;
    std::optional<MediaSegment> media_init;
    double total_duration = 0.0;
    bool is_live = false;
};

struct StreamSpec {
    std::optional<MediaType> media_type;
    std::string url;
    std::optional<std::string> group_id;
    std::optional<std::string> language;
    std::optional<std::string> name;
    std::optional<std::string> codecs;
    std::optional<std::string> resolution;
    std::optional<double> frame_rate;
    std::optional<std::string> channels;
    std::optional<std::string> video_range;
    int64_t bandwidth = 0;
    std::optional<Playlist> playlist;
    std::optional<std::string> extension;
    int segments_count = 0;
    std::optional<std::string> audio_group_id;
    std::optional<std::string> subtitle_group_id;

    std::string to_short_string() const;
    std::string to_string() const;
};

struct DownloadResult {
    bool success = false;
    std::string actual_file_path;
};

}
