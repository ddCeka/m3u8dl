#include "common/crypto.hpp"
#include "common/util.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <stdexcept>
#include <fstream>

namespace m3u8dl {

std::vector<uint8_t> AESUtil::aes128_decrypt_cbc(
    std::span<const uint8_t> data,
    std::span<const uint8_t> key,
    std::span<const uint8_t> iv) {

    if (key.size() != 16 || iv.size() != 16) {
        throw std::invalid_argument("AES-128 requires 16-byte key and IV");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    std::vector<uint8_t> plaintext(data.size() + AES_BLOCK_SIZE);
    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, data.data(), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption failed");
    }
    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption finalization failed");
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    plaintext.resize(plaintext_len);
    return plaintext;
}

std::vector<uint8_t> AESUtil::aes128_decrypt_ecb(
    std::span<const uint8_t> data,
    std::span<const uint8_t> key) {

    if (key.size() != 16) {
        throw std::invalid_argument("AES-128 requires 16-byte key");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, key.data(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    std::vector<uint8_t> plaintext(data.size() + AES_BLOCK_SIZE);
    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, data.data(), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption failed");
    }
    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Decryption finalization failed");
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    plaintext.resize(plaintext_len);
    return plaintext;
}

void AESUtil::decrypt_file_inplace(
    const std::string& file_path,
    std::span<const uint8_t> key,
    std::span<const uint8_t> iv,
    bool ecb_mode) {

    auto file_data = Util::read_file_bytes(file_path);
    if (!file_data) {
        throw std::runtime_error("Failed to read file: " + file_path);
    }

    std::vector<uint8_t> decrypted;
    if (ecb_mode) {
        decrypted = aes128_decrypt_ecb(*file_data, key);
    } else {
        decrypted = aes128_decrypt_cbc(*file_data, key, iv);
    }

    if (!Util::write_file_bytes(file_path, decrypted)) {
        throw std::runtime_error("Failed to write decrypted file: " + file_path);
    }
}

std::vector<uint8_t> ChaCha20Util::decrypt_per_1024_bytes(
    std::span<const uint8_t> data,
    std::span<const uint8_t> key,
    std::span<const uint8_t> nonce) {

    if (key.size() != 32 || (nonce.size() != 12 && nonce.size() != 8)) {
        throw std::invalid_argument("ChaCha20 requires 32-byte key and 12- or 8-byte nonce");
    }

    uint8_t iv16[16] = {0};
    size_t nonce_offset = 4 + (12 - nonce.size());
    std::copy(nonce.begin(), nonce.end(), iv16 + nonce_offset);

    std::vector<uint8_t> output(data.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    const size_t chunk_size = 1024;
    size_t offset = 0;

    while (offset < data.size()) {
        size_t process_size = std::min(chunk_size, data.size() - offset);

        if (EVP_DecryptInit_ex(ctx, EVP_chacha20(), nullptr, key.data(), iv16) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize ChaCha20 decryption");
        }

        int len = 0;
        if (EVP_DecryptUpdate(ctx, output.data() + offset, &len,
                            data.data() + offset, static_cast<int>(process_size)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("ChaCha20 decryption failed");
        }

        offset += process_size;
    }

    EVP_CIPHER_CTX_free(ctx);
    return output;
}

}
