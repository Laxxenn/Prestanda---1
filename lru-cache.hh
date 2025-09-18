#pragma once

#include <filesystem>
#include <string>
#include <iostream>
#include <unordered_map>
#include <list>

constexpr auto GIGABYTE = 1 << 30;

std::string base64encode(std::filesystem::path const& filename);

struct CACHECONFIG {
    int  byteCapacity = 0;
    bool enableCache  = false;

    bool cacFlag = false;
    bool ecFlag  = false;

    bool unFlag = false;  // -un => run uniformTest
    bool skFlag = false;  // -sk => run skewedTest
};

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
    explicit LRU_CACHE(std::size_t capacity)
        : maxCapacity(capacity), currentCapacity(0), isFull(false) {}

    ~LRU_CACHE() = default;

    const Value* get(const Key& key) {
        auto it = unMap.find(key);
        if (it == unMap.end()) {
            ++stats.misses;
            return nullptr;
        }

        // mark as most recently used
        cacheList.splice(cacheList.begin(), cacheList, it->second.second);

        ++stats.hits;
        return &it->second.first;   // pointer to encoded string
    }

    void insert(const Key& key, const Value& value) {
        if (maxCapacity == 0) return;

        const std::size_t new_sz = value.size();
        if (new_sz > maxCapacity) return;

        auto it = unMap.find(key);
        if (it != unMap.end()) {
            // update existing
            std::size_t old_sz = it->second.first.size();
            if (new_sz >= old_sz) {
                currentCapacity += (new_sz - old_sz);
            } else {
                currentCapacity -= (old_sz - new_sz);
            }
            it->second.first = value;
            cacheList.splice(cacheList.begin(), cacheList, it->second.second);
        } else {
            // insert new
            cacheList.emplace_front(key);
            unMap.emplace(key, std::make_pair(value, cacheList.begin()));
            currentCapacity += new_sz;
        }

        // evict LRU until within capacity
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
    std::size_t maxCapacity = 0;
    std::size_t currentCapacity = 0;
    bool        isFull = false;

private:
    std::list<Key>                                         cacheList; // keys only
    std::unordered_map<Key, std::pair<Value, Iter>>        unMap;     // key -> (encoded, list it)
    LRU_STATS                                              stats;
};

extern LRU_CACHE   lru;
extern CACHECONFIG cc;
