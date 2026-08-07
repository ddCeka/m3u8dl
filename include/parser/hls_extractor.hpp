#pragma once

#include "common/types.hpp"
#include <string>
#include <vector>
#include <map>

namespace m3u8dl {

class HLSExtractor {
public:
    explicit HLSExtractor(const std::string& url, const std::string& base_url = "");

    std::vector<StreamSpec> extract_streams(const std::string& content);

private:
    std::string m3u8_url_;
    std::string base_url_;
    std::string m3u8_content_;
    bool is_master_playlist_ = false;

    std::vector<StreamSpec> parse_master_playlist();
    Playlist parse_media_playlist();

    static std::string get_attribute(const std::string& line, const std::string& attr_name);
    static std::map<std::string, std::string> parse_attributes(const std::string& line);
};

}
