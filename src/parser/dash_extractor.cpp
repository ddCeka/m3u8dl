#include "parser/dash_extractor.hpp"
#include "common/util.hpp"
#include "common/logger.hpp"
#include <pugixml.hpp>
#include <sstream>
#include <regex>
#include <set>
#include <cmath>

namespace m3u8dl {

namespace {

std::string pad_left(int64_t value, int width) {
    std::string s = std::to_string(value);
    if (static_cast<int>(s.size()) < width) {
        s.insert(0, width - s.size(), '0');
    }
    return s;
}

double parse_iso8601_duration(const std::string& text) {
    static const std::regex duration_regex(
        R"(P(?:(\d+)D)?T?(?:(\d+)H)?(?:(\d+)M)?(?:([\d.]+)S)?)");
    std::smatch match;
    if (!std::regex_match(text, match, duration_regex)) {
        return 0.0;
    }
    double days = match[1].matched ? std::stod(match[1].str()) : 0.0;
    double hours = match[2].matched ? std::stod(match[2].str()) : 0.0;
    double minutes = match[3].matched ? std::stod(match[3].str()) : 0.0;
    double seconds = match[4].matched ? std::stod(match[4].str()) : 0.0;
    return days * 86400.0 + hours * 3600.0 + minutes * 60.0 + seconds;
}

std::string replace_template_vars(std::string text, const std::string& representation_id,
                                   int64_t bandwidth, int64_t number, int64_t time) {
    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    static const std::regex number_width_regex(R"(\$Number%([^$]+)d\$)");
    std::smatch match;
    while (std::regex_search(text, match, number_width_regex)) {
        int width = std::stoi(match[1].str());
        text.replace(match.position(0), match.length(0), pad_left(number, width));
    }

    replace_all(text, "$RepresentationID$", representation_id);
    replace_all(text, "$Bandwidth$", std::to_string(bandwidth));
    replace_all(text, "$Number$", std::to_string(number));
    replace_all(text, "$Time$", std::to_string(time));
    return text;
}

}

DASHExtractor::DASHExtractor(const std::string& mpd_url, const std::string& base_url)
    : mpd_url_(mpd_url), base_url_(base_url.empty() ? mpd_url : base_url) {}

std::vector<StreamSpec> DASHExtractor::extract_streams(const std::string& mpd_content) {
    std::vector<StreamSpec> streams;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(mpd_content.c_str());

    if (!result) {
        Logger::error("Failed to parse MPD: " + std::string(result.description()));
        return streams;
    }

    auto mpd_node = doc.child("MPD");
    if (!mpd_node) {
        Logger::error("No MPD root node found");
        return streams;
    }

    bool is_live = std::string(mpd_node.attribute("type").value()) == "dynamic";
    std::string mpd_duration_attr = mpd_node.attribute("mediaPresentationDuration").value();

    for (auto period : mpd_node.children("Period")) {
        std::string period_base_url = base_url_;

        std::string period_duration_attr = period.attribute("duration").value();
        double period_duration_seconds = parse_iso8601_duration(
            !period_duration_attr.empty() ? period_duration_attr : mpd_duration_attr);

        auto period_base_url_node = period.child("BaseURL");
        if (period_base_url_node) {
            period_base_url = Util::combine_url(period_base_url, period_base_url_node.child_value());
        }

        for (auto adaptation_set : period.children("AdaptationSet")) {
            std::string as_base_url = period_base_url;

            auto as_base_url_node = adaptation_set.child("BaseURL");
            if (as_base_url_node) {
                as_base_url = Util::combine_url(as_base_url, as_base_url_node.child_value());
            }

            std::string content_type = adaptation_set.attribute("contentType").value();
            std::string mime_type = adaptation_set.attribute("mimeType").value();

            MediaType media_type = MediaType::VIDEO;
            if (content_type == "audio" || mime_type.find("audio") != std::string::npos) {
                media_type = MediaType::AUDIO;
            } else if (content_type == "text" || mime_type.find("text") != std::string::npos) {
                media_type = MediaType::SUBTITLES;
            }

            for (auto representation : adaptation_set.children("Representation")) {
                StreamSpec spec;
                spec.media_type = media_type;

                std::string repr_base_url = as_base_url;
                auto repr_base_url_node = representation.child("BaseURL");
                if (repr_base_url_node) {
                    repr_base_url = Util::combine_url(repr_base_url, repr_base_url_node.child_value());
                }

                spec.url = repr_base_url;
                spec.bandwidth = representation.attribute("bandwidth").as_llong();
                spec.codecs = representation.attribute("codecs").value();

                auto width = representation.attribute("width").as_int();
                auto height = representation.attribute("height").as_int();
                if (width > 0 && height > 0) {
                    spec.resolution = std::to_string(width) + "x" + std::to_string(height);
                }

                auto frame_rate_attr = representation.attribute("frameRate");
                if (frame_rate_attr) {
                    std::string fr_str = frame_rate_attr.value();
                    size_t slash = fr_str.find('/');
                    if (slash != std::string::npos) {
                        double num = std::stod(fr_str.substr(0, slash));
                        double den = std::stod(fr_str.substr(slash + 1));
                        spec.frame_rate = num / den;
                    } else {
                        spec.frame_rate = std::stod(fr_str);
                    }
                }

                spec.language = adaptation_set.attribute("lang").value();

                auto seg_template_inner = representation.child("SegmentTemplate");
                auto seg_template_outer = adaptation_set.child("SegmentTemplate");
                auto seg_template = seg_template_inner ? seg_template_inner : seg_template_outer;

                if (seg_template) {
                    auto attr = [&](const char* name) -> pugi::xml_attribute {
                        auto a = seg_template_inner ? seg_template_inner.attribute(name) : pugi::xml_attribute();
                        if (!a && seg_template_outer) a = seg_template_outer.attribute(name);
                        return a;
                    };

                    Playlist playlist;
                    playlist.is_live = is_live;

                    std::string representation_id = representation.attribute("id").value();
                    int64_t timescale = attr("timescale") ? attr("timescale").as_llong() : 1;
                    int64_t start_number = attr("startNumber") ? attr("startNumber").as_llong() : 1;
                    std::string media_template = attr("media").value();

                    auto init_attr = attr("initialization");
                    if (init_attr) {
                        std::string init_url = replace_template_vars(
                            init_attr.value(), representation_id, spec.bandwidth, 0, 0);
                        MediaSegment init_seg;
                        init_seg.url = Util::combine_url(repr_base_url, init_url);
                        init_seg.index = -1;
                        playlist.media_init = init_seg;
                    }

                    MediaPart part;
                    auto timeline = seg_template.child("SegmentTimeline");
                    if (timeline && !media_template.empty()) {
                        int64_t seg_number = start_number;
                        int index = 0;
                        int64_t current_time = 0;

                        for (auto s : timeline.children("S")) {
                            auto t_attr = s.attribute("t");
                            if (t_attr) {
                                current_time = t_attr.as_llong();
                            }
                            int64_t duration = s.attribute("d").as_llong();
                            int64_t repeat_count = s.attribute("r").as_llong(0);
                            if (repeat_count < 0) {
                                if (period_duration_seconds > 0 && duration > 0) {
                                    repeat_count = static_cast<int64_t>(std::ceil(
                                        period_duration_seconds * static_cast<double>(timescale) /
                                        static_cast<double>(duration))) - 1;
                                    if (repeat_count < 0) repeat_count = 0;
                                } else {
                                    repeat_count = 0;
                                }
                            }

                            for (int64_t i = 0; i <= repeat_count; ++i) {
                                MediaSegment seg;
                                seg.duration = timescale > 0 ?
                                    static_cast<double>(duration) / static_cast<double>(timescale) : 0.0;
                                seg.index = index++;

                                std::string media_url = replace_template_vars(
                                    media_template, representation_id, spec.bandwidth,
                                    seg_number++, current_time);
                                seg.url = Util::combine_url(repr_base_url, media_url);

                                part.segments.push_back(seg);
                                playlist.total_duration += seg.duration;
                                current_time += duration;
                            }
                        }
                    } else if (!media_template.empty() && attr("duration")) {
                        int64_t seg_duration = attr("duration").as_llong();
                        if (seg_duration > 0 && timescale > 0 && period_duration_seconds > 0) {
                            int64_t total_segments = static_cast<int64_t>(std::ceil(
                                period_duration_seconds * static_cast<double>(timescale) /
                                static_cast<double>(seg_duration)));

                            for (int64_t i = 0; i < total_segments; ++i) {
                                MediaSegment seg;
                                seg.duration = static_cast<double>(seg_duration) / static_cast<double>(timescale);
                                seg.index = static_cast<int>(i);

                                std::string media_url = replace_template_vars(
                                    media_template, representation_id, spec.bandwidth,
                                    start_number + i, 0);
                                seg.url = Util::combine_url(repr_base_url, media_url);

                                part.segments.push_back(seg);
                                playlist.total_duration += seg.duration;
                            }
                        }
                    }

                    std::set<std::string> seen;
                    std::vector<MediaSegment> deduped;
                    deduped.reserve(part.segments.size());
                    for (auto& seg : part.segments) {
                        std::string identity = seg.url + "|" +
                            std::to_string(seg.start_range.value_or(-1)) + "|" +
                            std::to_string(seg.stop_range.value_or(-1));
                        if (seen.insert(identity).second) {
                            deduped.push_back(std::move(seg));
                        }
                    }
                    if (deduped.size() != part.segments.size()) {
                        Logger::debug("[DASH] removed " +
                            std::to_string(part.segments.size() - deduped.size()) +
                            " duplicate segment(s)");
                    }
                    part.segments = std::move(deduped);

                    if (!part.segments.empty()) {
                        playlist.media_parts.push_back(part);
                    }

                    spec.playlist = playlist;
                    spec.segments_count = spec.playlist->media_parts.size() > 0 ?
                        spec.playlist->media_parts[0].segments.size() : 0;
                }

                streams.push_back(spec);
            }
        }
    }

    return streams;
}

}
