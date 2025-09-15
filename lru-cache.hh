#pragma once

#include <filesystem>
#include <string>

constexpr auto GIGABYTE = 1 << 30;

std::string base64encode(std::filesystem::path const& filename);


#include <unordered_map>
#include <list>

class LRU_CACHE 
{
    typedef std::pair<std::string, std::string> keyValPair;
    public: 
        int maxCapacity;
        bool isFull;

        std::list<keyValPair> cacheList;
        std::unordered_map<std::string, std::list<keyValPair>::iterator> unMap;
    public:
        explicit LRU_CACHE(int &capacity) : maxCapacity(capacity){};
        virtual ~LRU_CACHE() = default;       


        std::string get(const std::string &key){
            auto foundVal = unMap.find(key);
            if(foundVal != unMap.end()){
                return foundVal->second->second;
            }
            return nullptr;
        }


        void insert(std::string &key, std::string &value){
        
            auto foundVal = unMap.find(key);
            if(foundVal != unMap.end()){
                foundVal->second->second = value;
                cacheList.splice(cacheList.begin(), cacheList, foundVal->second);
            }
            else {
                if(cacheList.size() == maxCapacity){
                    auto lastElement = cacheList.back();
                    unMap.erase(lastElement.first);
                    cacheList.pop_back();
                }
                cacheList.push_front(std::make_pair(key, value));
                unMap.insert(std::make_pair(key,cacheList.begin()));
            }
            
        }

};








/*
   Utökning av programmet:

   *Cache
        - Fixed maximum capacity i number of bytes per encoded data. CHECK!
            - Configurable via command line argument CHECK!
        - Least recent entry = evicted 
        - Doubly-linked-list 
            - paths/keys - std::map - mappa file path till encoded string paired with linked list iteraotr


    lru-cache.cc
        - Configuration function CHECK!
        - Modify implementtion of base64code so it optionaly uses software cache 
    
    lru-cache-main.cc
        - Commandline argument som enablar/disablar cache usage CHECK!
        - Commandline argument sätter cachen capacitet CHECK!

        * TEST
            - UNIFORM 
                - Encoda alla files i current work dir one by one in sequence.
                repeat this process 10 times. If all files simultaneously fit in the cache, then all cache 
                accesses will be hits. As soon as one or more files do not fit, then we expect this test to 
                result in nothing but cache misses.
            - SKEWED
                - Encode alla filer i current working dir ten times on average. Select the next file to encode
                at random by traversing the list of files sequentially and for each file, select that file 
                with a 20% probability.This will skew the distribution in favor of files that 
                appear early in the file list , proivided that there are more than five files. Since some 
                files are re-encoded frequently and likely close in time, we expect the cache to have some effect
                even when it cant fit all files at the same time. 


*/