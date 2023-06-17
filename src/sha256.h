#pragma once

#include <filesystem>
#include <openssl/sha.h>
#include <openssl/evp.h>

struct Evp {
    EVP_MD_CTX* ctx{nullptr};

    Evp()
        : ctx { EVP_MD_CTX_new() }
    {
        if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
            throw std::runtime_error{"Error initializing sha256"};
        }
    }
    Evp(Evp const&) = delete;
    Evp(Evp&&) = delete;
    ~Evp() {
        EVP_MD_CTX_free(ctx);
    }

    void update(std::span<char const> data) {
        if (!EVP_DigestUpdate(ctx, data.data(), data.size())) {
            throw std::runtime_error{"Error update sha256"};
        }
    }
    auto finalize() {
        auto str = std::vector<uint8_t>{};
        str.resize(EVP_MAX_MD_SIZE);
        uint32_t len = str.size();
        if (!EVP_DigestFinal_ex(ctx, str.data(), &len)) {
            throw std::runtime_error{"Error finalizing sha256"};
        }
        str.resize(len);
        return str;
    }
};

inline auto sha256sum(std::filesystem::path path) -> std::vector<uint8_t> {

    auto evp = Evp{};

    auto ifs = std::ifstream{path};
    size_t totalSize = file_size(path);

    auto buffer = std::vector<char>{};
    buffer.resize(65536);
    while (totalSize > 0) {
        buffer.resize(std::min(buffer.size(), totalSize));
        ifs.read(buffer.data(), buffer.size());
        evp.update(buffer);
        totalSize -= buffer.size();
    }
    return evp.finalize();
}
