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
};


void lruClear();
void lruReset();
void lruPrint();
void lruSetCapacity(std::size_t cap);

