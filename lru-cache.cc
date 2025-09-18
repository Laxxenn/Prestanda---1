#include <fstream>
#include <vector>

#include "lru-cache.hh"

static char const LUT[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static char byte2char(std::byte b)
{
    return LUT[std::to_integer<unsigned char>(b)];
}

static std::vector<std::byte> read_binary_file(std::filesystem::path const& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (not file) throw std::runtime_error("cannot open file: " + filename.string());
    std::streamsize const size = file.tellg();
    if (size > GIGABYTE) throw std::runtime_error("file too large: " + filename.string());
    file.seekg(0, std::ios::beg);
    std::vector<std::byte> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();
    return data;
}

static std::string base64encode(std::vector<std::byte> const& data)
{
    std::string encoded;

    int const nin = static_cast<int>(data.size());
    int const ngrp = nin / 3;
    int const nxtra = nin - 3 * ngrp;

    std::byte const mask6{0x3f};

    // loop over all full groups of 3 input bytes => 4 output bytes
    for (int i = 0; i < 3 * ngrp; i += 3) {
        auto const a = data[i + 0];
        auto const b = data[i + 1];
        auto const c = data[i + 2];
        encoded += byte2char(a >> 2);
        encoded += byte2char(((a << 4) | (b >> 4)) & mask6);
        encoded += byte2char(((b << 2) | (c >> 6)) & mask6);
        encoded += byte2char(c & mask6);
    }

    // take care of any left-over input bytes (0, 1, or 2)
    switch (nxtra) {
    case 1: // 1 input byte => 2 output bytes + 2 pad bytes
        {
            auto const a = data[3 * ngrp + 0];
            encoded += byte2char(a >> 2);
            encoded += byte2char((a << 4) & mask6);
            encoded += '=';
            encoded += '=';
        }
        break;
    case 2: // 2 input bytes => 3 output bytes + 1 pad byte
        {
            auto const a = data[3 * ngrp + 0];
            auto const b = data[3 * ngrp + 1];
            encoded += byte2char(a >> 2);
            encoded += byte2char(((a << 4) | (b >> 4)) & mask6);
            encoded += byte2char((b << 2) & mask6);
            encoded += '=';
        }
        break;
    }

    return encoded;
}

std::string base64encode(std::filesystem::path const& filename)
{
    if (cc.enableCache) {
        if (const std::string* cached = lru.get(filename)) {
            return *cached;  // use cached value
        }
    }

    auto const data = read_binary_file(filename);
    auto encoded = base64encode(data);

    if (cc.enableCache) {
        lru.insert(filename, encoded);
    }

    return encoded;
}
