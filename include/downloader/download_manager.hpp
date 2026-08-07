#pragma once

#include "common/types.hpp"
#include "downloader/simple_downloader.hpp"
#include <vector>
#include <string>
#include <memory>

namespace m3u8dl {

class DownloadManager {
public:
    explicit DownloadManager(const DownloadConfig& config);

    bool download_stream(const StreamSpec& stream, const std::string& track_suffix = "");
    std::string get_last_output_file() const { return last_output_file_; }

private:
    DownloadConfig config_;
    std::unique_ptr<SimpleDownloader> downloader_;
    std::string last_output_file_;

    void merge_segments(const std::vector<std::string>& segment_files, const std::string& output_file);
    void cleanup_temp_files(const std::vector<std::string>& files);
};

}
