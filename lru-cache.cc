#include <fstream>
#include <vector>
#include <list>
#include <unordered_map>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <iostream>

#include "lru-cache.hh"

class LRU_CACHE {
    using Key   = std::filesystem::path;
    using Value = std::string;
    using Iter  = std::list<Key>::iterator;

public:
    struct LRU_STATS {
        std::size_t hits      = 0;
        std::size_t misses    = 0;
        std::size_t evictions = 0;
        void reset() { hits = misses = evictions = 0; }
    };

public:
    explicit LRU_CACHE(std::size_t capacity) : maxCapacity(capacity), currentCapacity(0), isFull(false) {}

    ~LRU_CACHE() = default;

    const Value* get(const Key& key) {
        auto it = unMap.find(key);
        if (it == unMap.end()) {
            ++stats.misses;
            return nullptr;
        }
        
        cacheList.splice(cacheList.begin(), cacheList, it->second.second);
        ++stats.hits;
        return &it->second.first;
    }

    void insert(const Key& key, const Value& value) {
        if (maxCapacity == 0) return;

        const std::size_t new_sz = value.size();
        if (new_sz > maxCapacity) return; 

        auto it = unMap.find(key);
        if (it != unMap.end()) {
            std::size_t old_sz = it->second.first.size();
            currentCapacity += (new_sz >= old_sz) ? (new_sz - old_sz) : -(long long)(old_sz - new_sz);
            it->second.first = value;
            cacheList.splice(cacheList.begin(), cacheList, it->second.second);
        } else {
            cacheList.emplace_front(key);
            unMap.emplace(key, std::make_pair(value, cacheList.begin()));
            currentCapacity += new_sz;
        }

        while (currentCapacity > maxCapacity && !cacheList.empty()) {
            const Key& lastKey = cacheList.back();
            auto mit = unMap.find(lastKey);
            currentCapacity -= mit->second.first.size();
            unMap.erase(mit);
            cacheList.pop_back();
            ++stats.evictions;
        }

        isFull = (currentCapacity >= maxCapacity);
    }

    void printStats() const {
        std::cout << "LRU Cache Stats:\n"
                  << "  Hits:      " << stats.hits << '\n'
                  << "  Misses:    " << stats.misses << '\n'
                  << "  Evictions: " << stats.evictions << '\n';
    }

    void resetStats() { stats.reset(); }

    void clearCache() {
        cacheList.clear();
        unMap.clear();
        currentCapacity = 0;
        isFull = false;
    }

public:
    std::size_t maxCapacity     = 0;
    std::size_t currentCapacity = 0;
    bool        isFull          = false;

private:
    std::list<Key>                                  cacheList; 
    std::unordered_map<Key, std::pair<Value, Iter>> unMap;     
    LRU_STATS                                       stats;
};


static LRU_CACHE lru{0};

extern CACHECONFIG cc;


void lruClear()                { lru.clearCache(); }
void lruReset()                { lru.resetStats(); }
void lruPrint()                { lru.printStats(); }
void lruSetCapacity(size_t c) { lru.maxCapacity = c; }


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
    if (!file) throw std::runtime_error("cannot open file: " + filename.string());
    std::streamsize const size = file.tellg();
    if (size > static_cast<std::streamsize>(GIGABYTE))
        throw std::runtime_error("file too large: " + filename.string());
    file.seekg(0, std::ios::beg);
    std::vector<std::byte> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();
    return data;
}

static std::string base64encode(std::vector<std::byte> const& data)
{
    std::string encoded;

    int const nin   = static_cast<int>(data.size());
    int const ngrp  = nin / 3;
    int const nxtra = nin - 3 * ngrp;

    std::byte const mask6{0x3f};

    for (int i = 0; i < 3 * ngrp; i += 3) {
        auto const a = data[i + 0];
        auto const b = data[i + 1];
        auto const c = data[i + 2];
        encoded += byte2char(a >> 2);
        encoded += byte2char(((a << 4) | (b >> 4)) & mask6);
        encoded += byte2char(((b << 2) | (c >> 6)) & mask6);
        encoded += byte2char(c & mask6);
    }

    switch (nxtra) {
    case 1: {
        auto const a = data[3 * ngrp + 0];
        encoded += byte2char(a >> 2);
        encoded += byte2char((a << 4) & mask6);
        encoded += '=';
        encoded += '=';
    } break;
    case 2: {
        auto const a = data[3 * ngrp + 0];
        auto const b = data[3 * ngrp + 1];
        encoded += byte2char(a >> 2);
        encoded += byte2char(((a << 4) | (b >> 4)) & mask6);
        encoded += byte2char((b << 2) & mask6);
        encoded += '=';
    } break;
    default: break;
    }

    return encoded;
}

std::string base64encode(std::filesystem::path const& filename)
{
    if (cc.enableCache) {
        if (const std::string* cached = lru.get(filename)) {
            return *cached;
        }
    }

    auto const data   = read_binary_file(filename);
    auto encoded = base64encode(data);

    if (cc.enableCache) {
        lru.insert(filename, encoded);
    }

    return encoded;
}
