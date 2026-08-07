#include "cli/command_line.hpp"
#include <CLI/CLI.hpp>
#include <sstream>
#include <cstdlib>

namespace m3u8dl {

CommandLineOptions parse_arguments(int argc, char** argv) {
    CLI::App app{"m3u8dl - DASH/HLS/MSS Downloader"};

    CommandLineOptions options;

    app.add_option("input", options.input_url, "Input URL or file")
        ->required();

    app.add_option("--tmp-dir", options.tmp_dir, "Temporary file directory");
    app.add_option("--save-dir", options.save_dir, "Output directory");
    app.add_option("--save-name", options.save_name, "Output filename");
    app.add_option("--save-pattern", options.save_pattern,
                   "Output filename template, e.g. "
                   "\"<SaveName>_<Resolution>_<Language>\". "
                   "Vars: <SaveName> <Id> <Codecs> <Language> "
                   "<Bandwidth> <Resolution> <FrameRate> <Channels> "
                   "<VideoRange> <MediaType> <GroupId>");
    app.add_option("--log-file-path", options.log_file, "Log file path");
    app.add_option("--base-url", options.base_url, "Base URL");

    app.add_option("--thread-count", options.thread_count, "Download thread count (0 = auto)")
        ->default_val(0);

    app.add_option("--download-retry-count", options.download_retry_count, "Retry count per segment")
        ->default_val(3);

    app.add_option("--http-request-timeout", options.http_request_timeout, "HTTP timeout in seconds")
        ->default_val(100);

    app.add_flag("--binary-merge", options.binary_merge, "Binary merge mode");
    app.add_flag("--skip-merge", options.skip_merge, "Skip merging segments");
    app.add_flag("--skip-download", options.skip_download, "Skip download");
    app.add_flag("--del-after-done{true},--no-del-after-done{false}",
                 options.del_after_done, "Delete temp files after completion")
        ->default_val(true);

    app.add_flag("--no-log", options.no_log, "Disable logging");

    app.add_option("--log-level", options.log_level, "Log (DEBUG|INFO|WARN|ERROR|OFF)")
        ->default_val("INFO");

    std::vector<std::string> header_strings;
    app.add_option("-H,--header", header_strings, "Custom HTTP headers");

    app.add_option("--custom-proxy", options.custom_proxy, "Custom proxy URL");
    app.add_flag("--use-system-proxy", options.use_system_proxy, "Use system proxy")
        ->default_val(true);

    app.add_option("--select-stream", options.select_stream, "Select stream by index (0-based)");
    app.add_flag("--auto-select", options.auto_select, "Automatically select first stream (no interactive prompt)")
        ->default_val(false);

    app.add_flag("--mux-to-mp4", options.mux_to_mp4, "Mux output to MP4 using FFmpeg");
    app.add_option("--ffmpeg-binary-path,--ffmpeg-path", options.ffmpeg_path,
                   "Custom FFmpeg binary path (default: searches PATH)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        std::exit(app.exit(e));
    }

    for (const auto& header : header_strings) {
        size_t colon = header.find(':');
        if (colon != std::string::npos) {
            std::string key = header.substr(0, colon);
            std::string value = header.substr(colon + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            options.headers[key] = value;
        }
    }

    return options;
}

}
