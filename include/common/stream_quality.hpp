#pragma once

#include "common/types.hpp"
#include <vector>
#include <algorithm>
#include <cstdint>

namespace m3u8dl {

class StreamQuality {
public:
    static int64_t calculate_score(const StreamSpec& stream);

    static void sort_by_quality(std::vector<StreamSpec>& streams);

    static int get_best_stream_index(const std::vector<StreamSpec>& streams);

private:
    static int64_t parse_resolution(const std::string& resolution);

    static int get_codec_score(const std::string& codec);

    static int get_video_range_score(const std::string& video_range);
};

}
