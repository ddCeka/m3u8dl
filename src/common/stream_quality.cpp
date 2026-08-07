#include "common/stream_quality.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <cctype>

namespace m3u8dl {

int64_t StreamQuality::parse_resolution(const std::string& resolution) {
    if (resolution.empty()) {
        return 0;
    }

    size_t sep_pos = resolution.find('x');
    if (sep_pos == std::string::npos) {
        sep_pos = resolution.find('*');
    }
    if (sep_pos == std::string::npos) {
        return 0;
    }

    try {
        int64_t width = std::stoll(resolution.substr(0, sep_pos));
        int64_t height = std::stoll(resolution.substr(sep_pos + 1));
        return width * height;
    } catch (...) {
        return 0;
    }
}

int StreamQuality::get_codec_score(const std::string& codec) {
    std::string codec_lower = codec;
    std::transform(codec_lower.begin(), codec_lower.end(), codec_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (codec_lower.find("avc") != std::string::npos ||
        codec_lower.find("h264") != std::string::npos) {
        return 100;
    }

    if (codec_lower.find("hev") != std::string::npos ||
        codec_lower.find("h265") != std::string::npos) {
        return 90;
    }

    if (codec_lower.find("vp9") != std::string::npos ||
        codec_lower.find("vp09") != std::string::npos) {
        return 70;
    }

    if (codec_lower.find("vp8") != std::string::npos ||
        codec_lower.find("vp08") != std::string::npos) {
        return 85;
    }

    if (codec_lower.find("av01") != std::string::npos ||
        codec_lower.find("av1") != std::string::npos) {
        return 60;
    }

    if (codec_lower.find("mp4a") != std::string::npos) {
        return 80;
    }

    return 50;
}

int StreamQuality::get_video_range_score(const std::string& video_range) {
    std::string range_lower = video_range;
    std::transform(range_lower.begin(), range_lower.end(), range_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (range_lower.find("hdr10+") != std::string::npos ||
        range_lower.find("hdr10plus") != std::string::npos) {
        return 30;
    }

    if (range_lower.find("hdr") != std::string::npos ||
        range_lower.find("pq") != std::string::npos ||
        range_lower.find("hlg") != std::string::npos) {
        return 20;
    }

    return 10;
}

int64_t StreamQuality::calculate_score(const StreamSpec& stream) {
    int64_t score = 0;

    if (stream.resolution) {
        int64_t pixels = parse_resolution(*stream.resolution);
        score += pixels * 1000;
    }

    score += stream.bandwidth / 1000;

    if (stream.frame_rate) {
        score += static_cast<int64_t>(*stream.frame_rate * 100000);
    }

    if (stream.codecs) {
        score += get_codec_score(*stream.codecs) * 10000;
    }

    if (stream.video_range) {
        score += get_video_range_score(*stream.video_range) * 5000;
    }

    return score;
}

void StreamQuality::sort_by_quality(std::vector<StreamSpec>& streams) {
    std::sort(streams.begin(), streams.end(),
              [](const StreamSpec& a, const StreamSpec& b) {
                  return calculate_score(a) > calculate_score(b);
              });
}

int StreamQuality::get_best_stream_index(const std::vector<StreamSpec>& streams) {
    if (streams.empty()) {
        return -1;
    }

    int64_t best_score = calculate_score(streams[0]);
    int best_index = 0;

    for (size_t i = 1; i < streams.size(); ++i) {
        int64_t score = calculate_score(streams[i]);
        if (score > best_score) {
            best_score = score;
            best_index = static_cast<int>(i);
        }
    }

    return best_index;
}

}
