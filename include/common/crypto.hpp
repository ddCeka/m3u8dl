#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <span>

namespace m3u8dl {

class AESUtil {
public:
    static std::vector<uint8_t> aes128_decrypt_cbc(
        std::span<const uint8_t> data,
        std::span<const uint8_t> key,
        std::span<const uint8_t> iv
    );

    static std::vector<uint8_t> aes128_decrypt_ecb(
        std::span<const uint8_t> data,
        std::span<const uint8_t> key
    );

    static void decrypt_file_inplace(
        const std::string& file_path,
        std::span<const uint8_t> key,
        std::span<const uint8_t> iv,
        bool ecb_mode = false
    );
};

class ChaCha20Util {
public:
    static std::vector<uint8_t> decrypt_per_1024_bytes(
        std::span<const uint8_t> data,
        std::span<const uint8_t> key,
        std::span<const uint8_t> nonce
    );
};

}
