#include "downloader/download_manager.hpp"
#include "common/logger.hpp"
#include "common/util.hpp"
#include "third_party/BS_thread_pool.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace m3u8dl {

DownloadManager::DownloadManager(const DownloadConfig& config)
    : config_(config), downloader_(std::make_unique<SimpleDownloader>(config)) {

    Util::create_directories(config_.tmp_dir);
    Util::create_directories(config_.save_dir);
}

void DownloadManager::merge_segments(const std::vector<std::string>& segment_files,
                                     const std::string& output_file) {
    Logger::info("Merging " + std::to_string(segment_files.size()) + " segments...");

    std::ofstream output(output_file, std::ios::binary);
    if (!output.is_open()) {
        Logger::error("Failed to create output file: " + output_file);
        return;
    }

    for (const auto& segment_file : segment_files) {
        auto data = Util::read_file_bytes(segment_file);
        if (!data) {
            Logger::warn("Failed to read segment: " + segment_file);
            continue;
        }

        output.write(reinterpret_cast<const char*>(data->data()), data->size());
    }

    output.close();
    Logger::info("Merge complete: " + output_file);
}

void DownloadManager::cleanup_temp_files(const std::vector<std::string>& files) {
    if (!config_.delete_after_done) {
        return;
    }

    Logger::info("Cleaning up temporary files...");
    for (const auto& tmp_dir : files) {
        try {
            std::filesystem::remove(tmp_dir);
        } catch (const std::exception& e) {
            Logger::warn("Failed to delete: " + tmp_dir);
        }
    }
}

bool DownloadManager::download_stream(const StreamSpec& stream, const std::string& track_suffix) {
    if (!stream.playlist) {
        Logger::error("Stream has no playlist information");
        return false;
    }

    const auto& playlist = *stream.playlist;
    std::vector<std::string> segment_files;
    std::string tmp_prefix = config_.tmp_dir + (track_suffix.empty() ? "" : "/" + track_suffix);
    Util::create_directories(tmp_prefix);

    if (playlist.media_init) {
        Logger::info("Downloading initialization segment...");
        std::string init_file = tmp_prefix + "/seg_init.tmp";
        auto init_result = downloader_->download_segment(*playlist.media_init, init_file);

        if (init_result.success) {
            segment_files.push_back(init_file);
        } else {
            Logger::error("Failed to download initialization segment");
            return false;
        }
    }

    std::vector<MediaSegment> all_segments;
    for (const auto& part : playlist.media_parts) {
        for (const auto& segment : part.segments) {
            all_segments.push_back(segment);
        }
    }

    if (all_segments.empty()) {
        Logger::error("No segments to download");
        return false;
    }

    Logger::info("Downloading " + std::to_string(all_segments.size()) + " segments...");

    uint32_t max_threads = config_.thread_count > 0
        ? static_cast<uint32_t>(config_.thread_count)
        : std::min(std::thread::hardware_concurrency(), 4u);
    BS::thread_pool pool(max_threads);
    std::vector<std::future<DownloadResult>> futures;

    for (size_t i = 0; i < all_segments.size(); ++i) {
        const auto& segment = all_segments[i];
        std::string segment_file = tmp_prefix + "/seg_" + std::to_string(i) + ".tmp";
        segment_files.push_back(segment_file);

        futures.push_back(pool.submit_task([this, segment, segment_file]() {
            return downloader_->download_segment(segment, segment_file);
        }));
    }

    int successful = 0;
    for (size_t i = 0; i < futures.size(); ++i) {
        auto result = futures[i].get();
        if (result.success) {
            successful++;
        } else {
            Logger::error("Failed to download segment " + std::to_string(i));
        }

        if ((i + 1) % 10 == 0 || (i + 1) == futures.size()) {
            std::cout << "\rProgress: " << (i + 1) << "/" << futures.size()
                     << " (" << successful << " successful)" << std::flush;
        }
    }
    std::cout << std::endl;

    if (successful == 0) {
        Logger::error("No segments downloaded successfully");
        return false;
    }

    if (config_.skip_merge) {
        Logger::info("Skipping merge as requested; segments kept in " + tmp_prefix);
        return true;
    }

    std::string base_name;
    if (!config_.save_pattern.empty()) {
        base_name = Util::format_save_pattern(config_.save_pattern, stream, config_.save_name, 0);
    } else {
        base_name = config_.save_name;
        if (!track_suffix.empty()) {
            base_name += "." + track_suffix;
        }
    }

    std::string output_file = config_.save_dir + "/" + base_name;
    if (stream.extension) {
        output_file += "." + *stream.extension;
    } else {
        output_file += ".ts";
    }

    merge_segments(segment_files, output_file);

    cleanup_temp_files(segment_files);

    last_output_file_ = output_file;

    Logger::info("Download complete: " + output_file);
    return true;
}

}
