#pragma once

#include "common/types.hpp"
#include "common/http_client.hpp"
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <cstdint>

namespace m3u8dl {

struct DownloadConfig {
    std::string tmp_dir = "./tmp";
    std::string save_dir = ".";
    std::string save_name = "output";
    std::string save_pattern;
    std::map<std::string, std::string> headers;
    int download_retry_count = 3;
    int timeout = 100;
    int thread_count = 0;
    bool binary_merge = false;
    bool delete_after_done = true;
    bool skip_merge = false;
};

class SimpleDownloader {
public:
    explicit SimpleDownloader(const DownloadConfig& config);

    DownloadResult download_segment(const MediaSegment& segment, const std::string& save_path);

private:
    DownloadConfig config_;
    std::mutex key_cache_mutex_;
    std::map<std::string, std::vector<uint8_t>> key_cache_;

    DownloadResult download_with_retry(const std::string& url, const std::string& path,
                                       int64_t start_range = -1, int64_t end_range = -1);

    std::vector<uint8_t> fetch_key(const std::string& key_url);
    void decrypt_segment(const std::string& file_path, EncryptInfo encrypt_info);
    void strip_disguise(const std::string& file_path);
};

}
