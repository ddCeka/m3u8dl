#pragma once

#include "common/types.hpp"
#include <string>
#include <vector>

namespace m3u8dl {

class DASHExtractor {
public:
    explicit DASHExtractor(const std::string& mpd_url, const std::string& base_url = "");

    std::vector<StreamSpec> extract_streams(const std::string& mpd_content);

private:
    std::string mpd_url_;
    std::string base_url_;
};

}
