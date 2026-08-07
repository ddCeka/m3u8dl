#include "common/types.hpp"
#include <sstream>

namespace m3u8dl {

std::string StreamSpec::to_short_string() const {
    std::ostringstream oss;

    if (media_type) {
        switch (*media_type) {
            case MediaType::VIDEO: oss << "[VIDEO] "; break;
            case MediaType::AUDIO: oss << "[AUDIO] "; break;
            case MediaType::SUBTITLES: oss << "[SUBTITLE] "; break;
        }
    }

    if (resolution) {
        oss << *resolution << " ";
    }

    if (codecs) {
        oss << *codecs << " ";
    }

    if (bandwidth > 0) {
        oss << bandwidth / 1000 << "kbps ";
    }

    if (language) {
        oss << "(" << *language << ")";
    }

    return oss.str();
}

std::string StreamSpec::to_string() const {
    std::ostringstream oss;
    oss << to_short_string();

    if (group_id) {
        oss << " GroupID:" << *group_id;
    }

    if (segments_count > 0) {
        oss << " Segments:" << segments_count;
    }

    return oss.str();
}

}
