#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace m3u8dl {

struct CommandLineOptions {
    std::string input_url;
    std::optional<std::string> tmp_dir;
    std::optional<std::string> save_dir;
    std::optional<std::string> save_name;
    std::optional<std::string> save_pattern; // Filename template, e.g. "<SaveName>_<Resolution>_<Language>"
    std::optional<std::string> log_file;
    std::optional<std::string> base_url;
    int thread_count = 0; // 0 = auto
    int download_retry_count = 3;
    int http_request_timeout = 100;
    bool binary_merge = false;
    bool skip_merge = false;
    bool skip_download = false;
    bool del_after_done = true;
    bool no_log = false;
    std::string log_level = "INFO";
    std::map<std::string, std::string> headers;
    std::optional<std::string> custom_proxy;
    bool use_system_proxy = true;
    std::optional<int> select_stream;
    bool auto_select = false; // Auto-select first stream (old behavior)
    bool mux_to_mp4 = false;
    std::optional<std::string> ffmpeg_path;
};

CommandLineOptions parse_arguments(int argc, char** argv);

}
