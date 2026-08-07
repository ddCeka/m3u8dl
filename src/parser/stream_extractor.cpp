#include "parser/stream_extractor.hpp"
#include "parser/hls_extractor.hpp"
#include "parser/dash_extractor.hpp"
#include "common/logger.hpp"
#include <cstdint>

namespace m3u8dl {

StreamExtractor::StreamExtractor(const std::string& url)
    : url_(url), base_url_(url), http_client_(std::make_unique<HttpClient>()) {}

ExtractorType StreamExtractor::detect_type(const std::string& content) {
    if (content.find("#EXTM3U") != std::string::npos) {
        return ExtractorType::HLS;
    } else if (content.find("<MPD") != std::string::npos) {
        return ExtractorType::MPEG_DASH;
    }

    return ExtractorType::HLS;
}

std::vector<StreamSpec> StreamExtractor::extract() {
    Logger::info("Fetching: " + url_);

    auto response = http_client_->get_with_redirect_tracking(url_);
    if (!response.success) {
        Logger::error("Failed to fetch URL");
        return {};
    }

    base_url_ = response.final_url;

    if (response.body.size() >= 3 &&
        static_cast<uint8_t>(response.body[0]) == 0xEF &&
        static_cast<uint8_t>(response.body[1]) == 0xBB &&
        static_cast<uint8_t>(response.body[2]) == 0xBF) {
        response.body.erase(0, 3);
    }

    auto type = detect_type(response.body);

    switch (type) {
        case ExtractorType::HLS: {
            Logger::info("Detected HLS playlist");
            HLSExtractor extractor(url_, base_url_);
            return extractor.extract_streams(response.body);
        }
        case ExtractorType::MPEG_DASH: {
            Logger::info("Detected DASH manifest");
            DASHExtractor extractor(url_, base_url_);
            return extractor.extract_streams(response.body);
        }
        default:
            Logger::error("Unsupported stream type");
            return {};
    }
}

}
