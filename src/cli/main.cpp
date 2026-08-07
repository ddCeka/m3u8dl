#include "cli/command_line.hpp"
#include "common/logger.hpp"
#include "common/http_client.hpp"
#include "common/stream_quality.hpp"
#include "parser/stream_extractor.hpp"
#include "downloader/download_manager.hpp"
#include <iostream>
#include <thread>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <optional>
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

extern char** environ;

using namespace m3u8dl;

static int run_process(const std::vector<std::string>& args, bool silent = false) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    if (silent) {
        posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    }

    pid_t pid = 0;
    int rc = posix_spawnp(&pid, argv[0], &file_actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&file_actions);
    if (rc != 0) {
        return -1;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

LogLevel parse_log_level(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "OFF") return LogLevel::OFF;
    return LogLevel::INFO;
}

void print_streams(const std::vector<StreamSpec>& streams) {
    std::cout << "\n=== Available Streams ===" << std::endl;
    for (size_t i = 0; i < streams.size(); ++i) {
        std::cout << "[" << i << "] " << streams[i].to_string() << std::endl;
    }
    std::cout << std::endl;
}

int select_stream_interactive(const std::vector<StreamSpec>& streams) {
    if (streams.empty()) {
        return -1;
    }

    if (streams.size() == 1) {
        Logger::info("Only one stream available, auto-selecting");
        return 0;
    }

    std::cout << "Select stream (0-" << (streams.size() - 1) << ", or q to quit): ";
    std::string input;
    std::getline(std::cin, input);

    if (input == "q" || input == "Q") {
        return -1;
    }

    try {
        int choice = std::stoi(input);
        if (choice >= 0 && choice < static_cast<int>(streams.size())) {
            return choice;
        } else {
            std::cerr << "Invalid selection. Please choose 0-" << (streams.size() - 1) << std::endl;
            return -1;
        }
    } catch (...) {
        std::cerr << "Invalid input. Please enter a number." << std::endl;
        return -1;
    }
}

bool mux_with_ffmpeg(const std::vector<std::string>& input_files, const std::string& output_file,
                     const std::string& ffmpeg_path) {
    Logger::info("Muxing with FFmpeg: " + std::to_string(input_files.size()) + " input(s) -> " + output_file);

    std::vector<std::string> args = {ffmpeg_path};
    for (const auto& input_file : input_files) {
        args.push_back("-i");
        args.push_back(input_file);
    }
    args.push_back("-c");
    args.push_back("copy");
    args.push_back(output_file);
    args.push_back("-y");

    int result = run_process(args);

    if (result == 0) {
        Logger::info("FFmpeg muxing completed successfully");
        return true;
    } else {
        Logger::error("FFmpeg muxing failed with exit code: " + std::to_string(result));
        return false;
    }
}

const StreamSpec* find_matching_audio(const std::vector<StreamSpec>& streams, const StreamSpec& video) {
    if (!video.audio_group_id) {
        return nullptr;
    }
    for (const auto& s : streams) {
        if (s.media_type == MediaType::AUDIO && s.group_id == video.audio_group_id) {
            return &s;
        }
    }
    return nullptr;
}

std::string find_ffmpeg_binary() {
    const char* paths[] = {
        "ffmpeg",                          // In PATH
        "/usr/bin/ffmpeg",                 // Linux
        "@TERMUX__PREFIX@/bin/ffmpeg",     // Termux
        "@TERMUX__HOME@/bin/ffmpeg",       // Termux home dir
        "/usr/local/bin/ffmpeg",           // Homebrew/custom install
    };

    for (const char* path : paths) {
        if (run_process({path, "-version"}, /*silent=*/true) == 0) {
            Logger::debug("Found FFmpeg at: " + std::string(path));
            return std::string(path);
        }
    }

    Logger::warn("FFmpeg not found in common locations");
    return "ffmpeg";
}

int main(int argc, char** argv) {
    try {
        auto options = parse_arguments(argc, argv);

        if (!options.no_log) {
            Logger::init(parse_log_level(options.log_level),
                        options.log_file.value_or(""));
        }

        Logger::info("m3u8dl v0.1.0");
        Logger::info("Input: " + options.input_url);

        HttpClient::global_init();

        StreamExtractor extractor(options.input_url);
        auto streams = extractor.extract();

        if (streams.empty()) {
            Logger::error("No streams found");
            HttpClient::global_cleanup();
            return 1;
        }

        Logger::info("Found " + std::to_string(streams.size()) + " stream(s)");

        StreamQuality::sort_by_quality(streams);

        print_streams(streams);

        if (options.skip_download) {
            Logger::info("Skipping download as requested");
            HttpClient::global_cleanup();
            return 0;
        }

        int stream_index = 0;

        if (options.select_stream.has_value()) {
            stream_index = *options.select_stream;
            if (stream_index < 0 || stream_index >= static_cast<int>(streams.size())) {
                Logger::error("Invalid stream index: " + std::to_string(stream_index));
                Logger::error("Available: 0-" + std::to_string(streams.size() - 1));
                HttpClient::global_cleanup();
                return 1;
            }
            Logger::info("Selected stream [" + std::to_string(stream_index) + "] via command line");
        } else if (options.auto_select || streams.size() == 1) {
            stream_index = 0;
            Logger::info("Auto-selected best quality stream [0]");
        } else {
            stream_index = select_stream_interactive(streams);
            if (stream_index < 0) {
                Logger::info("Download cancelled by user");
                HttpClient::global_cleanup();
                return 0;
            }
        }

        auto& selected_stream = streams[stream_index];
        Logger::info("Downloading: " + selected_stream.to_short_string());

        auto fetch_playlist_if_needed = [](StreamSpec& spec) -> bool {
            if (spec.playlist || spec.url.empty()) {
                return true;
            }
            Logger::info("Fetching media playlist: " + spec.url);
            StreamExtractor media_extractor(spec.url);
            auto media_streams = media_extractor.extract();
            if (!media_streams.empty() && media_streams[0].playlist) {
                spec.playlist = media_streams[0].playlist;
                return true;
            }
            Logger::error("Failed to fetch media playlist");
            return false;
        };

        if (!fetch_playlist_if_needed(selected_stream)) {
            HttpClient::global_cleanup();
            return 1;
        }

        const StreamSpec* matched_audio_const = find_matching_audio(streams, selected_stream);
        std::optional<StreamSpec> audio_stream;
        if (matched_audio_const) {
            audio_stream = *matched_audio_const;
            Logger::info("Paired audio track: " + audio_stream->to_short_string());
            if (!fetch_playlist_if_needed(*audio_stream)) {
                HttpClient::global_cleanup();
                return 1;
            }
        }

        DownloadConfig download_config;
        download_config.tmp_dir = options.tmp_dir.value_or("./tmp");
        download_config.save_dir = options.save_dir.value_or(".");
        download_config.save_name = options.save_name.value_or("output");
        download_config.save_pattern = options.save_pattern.value_or("");
        download_config.headers = options.headers;
        download_config.download_retry_count = options.download_retry_count;
        download_config.timeout = options.http_request_timeout;
        download_config.thread_count = options.thread_count;
        download_config.binary_merge = options.binary_merge;
        download_config.delete_after_done = options.del_after_done;
        download_config.skip_merge = options.skip_merge;

        DownloadManager manager(download_config);

        bool success = manager.download_stream(selected_stream, audio_stream ? "video" : "");
        std::string video_output = manager.get_last_output_file();

        std::string audio_output;
        if (success && audio_stream) {
            DownloadManager audio_manager(download_config);
            success = audio_manager.download_stream(*audio_stream, "audio");
            audio_output = audio_manager.get_last_output_file();
        }

        if (!success) {
            Logger::error("Download failed");
            HttpClient::global_cleanup();
            return 1;
        }

        if (options.skip_merge) {
            HttpClient::global_cleanup();
            return 0;
        }

        std::vector<std::string> track_outputs = {video_output};
        if (!audio_output.empty()) {
            track_outputs.push_back(audio_output);
        }

        if (options.mux_to_mp4) {
            std::string output_file = video_output;

            size_t dot_pos = output_file.find_last_of('.');
            if (dot_pos != std::string::npos) {
                output_file = output_file.substr(0, dot_pos) + ".mp4";
            } else {
                output_file += ".mp4";
            }

            std::string ffmpeg_path = options.ffmpeg_path.value_or(find_ffmpeg_binary());

            bool mux_success = mux_with_ffmpeg(track_outputs, output_file, ffmpeg_path);

            if (mux_success) {
                Logger::info("All done! Output: " + output_file);

                if (options.del_after_done) {
                    for (const auto& f : track_outputs) {
                        Logger::info("Removing intermediate file: " + f);
                        std::filesystem::remove(f);
                    }
                }
            } else {
                Logger::warn("FFmpeg muxing failed, but download succeeded");
                for (const auto& f : track_outputs) {
                    Logger::info("Output: " + f);
                }
            }
        } else {
            Logger::info("All done!");
            for (const auto& f : track_outputs) {
                Logger::info("Output: " + f);
            }
        }

        HttpClient::global_cleanup();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        HttpClient::global_cleanup();
        return 1;
    }

    return 0;
}
