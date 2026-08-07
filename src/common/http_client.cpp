#include "common/http_client.hpp"
#include "common/logger.hpp"
#include "common/util.hpp"
#include <sstream>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace m3u8dl {

namespace {
constexpr const char* kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/78.0.3904.108 Safari/537.36";

bool has_header_case_insensitive(const std::map<std::string, std::string>& headers, const std::string& name) {
    for (const auto& [key, value] : headers) {
        (void)value;
        if (key.size() == name.size() &&
            std::equal(key.begin(), key.end(), name.begin(), [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            })) {
            return true;
        }
    }
    return false;
}
}

void HttpClient::global_init() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void HttpClient::global_cleanup() {
    curl_global_cleanup();
}

HttpClient::HttpClient() : handle_(curl_easy_init()) {
    if (!handle_) {
        throw std::runtime_error("Failed to initialize CURL handle");
    }
}

HttpClient::~HttpClient() {
    if (handle_) {
        curl_easy_cleanup(handle_);
    }
}

HttpClient::HttpClient(HttpClient&& other) noexcept
    : handle_(other.handle_),
      headers_(std::move(other.headers_)),
      timeout_(other.timeout_),
      follow_redirects_(other.follow_redirects_),
      ssl_verify_(other.ssl_verify_),
      proxy_(std::move(other.proxy_)),
      range_(other.range_),
      range_str_(std::move(other.range_str_)) {
    other.handle_ = nullptr;
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        if (handle_) curl_easy_cleanup(handle_);
        handle_ = other.handle_;
        headers_ = std::move(other.headers_);
        timeout_ = other.timeout_;
        follow_redirects_ = other.follow_redirects_;
        ssl_verify_ = other.ssl_verify_;
        proxy_ = std::move(other.proxy_);
        range_ = other.range_;
        range_str_ = std::move(other.range_str_);
        other.handle_ = nullptr;
    }
    return *this;
}

HttpClient& HttpClient::add_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

HttpClient& HttpClient::set_timeout(int seconds) {
    timeout_ = seconds;
    return *this;
}

HttpClient& HttpClient::set_proxy(const std::string& proxy_url) {
    proxy_ = proxy_url;
    return *this;
}

HttpClient& HttpClient::set_follow_redirects(bool follow) {
    follow_redirects_ = follow;
    return *this;
}

HttpClient& HttpClient::set_range(int64_t from, int64_t to) {
    range_ = {from, to};
    return *this;
}

HttpClient& HttpClient::set_ssl_verify(bool verify) {
    ssl_verify_ = verify;
    return *this;
}

void HttpClient::setup_request() {
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION, follow_redirects_ ? 1L : 0L);
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, ssl_verify_ ? 1L : 0L);
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST, ssl_verify_ ? 2L : 0L);
    curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
    curl_easy_setopt(handle_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle_, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(handle_, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(handle_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);

    if (proxy_) {
        curl_easy_setopt(handle_, CURLOPT_PROXY, proxy_->c_str());
    }

    if (range_) {
        range_str_ = std::to_string(range_->first) + "-" + std::to_string(range_->second);
        curl_easy_setopt(handle_, CURLOPT_RANGE, range_str_->c_str());
    } else {
        range_str_.reset();
        curl_easy_setopt(handle_, CURLOPT_RANGE, nullptr);
    }
}

size_t HttpClient::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total_size = size * nmemb;
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, total_size);
    return total_size;
}

size_t HttpClient::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);

    std::string header(buffer, total_size);
    auto colon_pos = header.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = header.substr(0, colon_pos);
        std::string value = header.substr(colon_pos + 1);

        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        (*headers)[key] = value;
    }

    return total_size;
}

HttpResponse HttpClient::get(const std::string& url) {
    HttpResponse response;

    setup_request();
    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, &response.headers);

    struct curl_slist* header_list = nullptr;
    if (!has_header_case_insensitive(headers_, "User-Agent")) {
        header_list = curl_slist_append(header_list, (std::string("User-Agent: ") + kDefaultUserAgent).c_str());
    }
    for (const auto& [key, value] : headers_) {
        std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }
    if (header_list) {
        curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(handle_);

    if (header_list) {
        curl_slist_free_all(header_list);
    }

    if (res == CURLE_OK) {
        long status_code;
        curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &status_code);
        response.status_code = static_cast<int>(status_code);

        char* final_url = nullptr;
        curl_easy_getinfo(handle_, CURLINFO_EFFECTIVE_URL, &final_url);
        if (final_url) {
            response.final_url = final_url;
        }

        response.success = true;
    } else {
        Logger::error("HTTP request failed: " + std::string(curl_easy_strerror(res)));
        response.success = false;
    }

    return response;
}

HttpResponse HttpClient::get_with_redirect_tracking(const std::string& url) {
    bool original_follow = follow_redirects_;
    set_follow_redirects(false);

    std::string current_url = url;
    int redirect_count = 0;
    const int max_redirects = 10;

    while (redirect_count < max_redirects) {
        auto response = get(current_url);

        if (response.status_code >= 300 && response.status_code < 400) {
            auto it = response.headers.find("Location");
            if (it != response.headers.end()) {
                current_url = Util::combine_url(current_url, it->second);
                Logger::debug("Redirected => " + current_url);
                redirect_count++;
                continue;
            }
        }

        response.final_url = current_url;
        if (response.status_code >= 400) {
            Logger::warn("HTTP " + std::to_string(response.status_code) + " for: " + current_url);
            response.success = false;
        }
        set_follow_redirects(original_follow);
        return response;
    }

    set_follow_redirects(original_follow);
    HttpResponse error_response;
    error_response.success = false;
    return error_response;
}

std::vector<uint8_t> HttpClient::get_bytes(const std::string& url) {
    auto response = get_with_redirect_tracking(url);
    if (!response.success) {
        return {};
    }

    return std::vector<uint8_t>(response.body.begin(), response.body.end());
}

}
