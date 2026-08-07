#pragma once

#include "common/types.hpp"
#include "common/http_client.hpp"
#include <string>
#include <vector>
#include <memory>

namespace m3u8dl {

class StreamExtractor {
public:
    explicit StreamExtractor(const std::string& url);

    std::vector<StreamSpec> extract();

    ExtractorType detect_type(const std::string& content);

private:
    std::string url_;
    std::string base_url_;
    std::unique_ptr<HttpClient> http_client_;
};

}
