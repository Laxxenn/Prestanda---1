#pragma once

#include <filesystem>
#include <string>

constexpr auto GIGABYTE = 1 << 30;

std::string base64encode(std::filesystem::path const& filename);



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