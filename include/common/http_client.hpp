#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <curl/curl.h>

namespace m3u8dl {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string final_url;
    bool success = false;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&& other) noexcept;
    HttpClient& operator=(HttpClient&& other) noexcept;

    HttpClient& add_header(const std::string& key, const std::string& value);
    HttpClient& set_timeout(int seconds);
    HttpClient& set_proxy(const std::string& proxy_url);
    HttpClient& set_follow_redirects(bool follow);
    HttpClient& set_range(int64_t from, int64_t to);
    HttpClient& set_ssl_verify(bool verify);

    HttpResponse get(const std::string& url);
    HttpResponse get_with_redirect_tracking(const std::string& url);
    std::vector<uint8_t> get_bytes(const std::string& url);

    static void global_init();
    static void global_cleanup();

private:
    CURL* handle_ = nullptr;
    std::map<std::string, std::string> headers_;
    int timeout_ = 100;
    bool follow_redirects_ = false;
    bool ssl_verify_ = false;
    std::optional<std::string> proxy_;
    std::optional<std::pair<int64_t, int64_t>> range_;
    std::optional<std::string> range_str_;  // Keep it alive for CURL

    void setup_request();
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
};

}
