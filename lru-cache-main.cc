#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>

#include "lru-cache.hh"


#include <string_view>
#include <charconv>   



bool readArgv(CACHECONFIG &cc , int &argc ,char *argv[]);

LRU_CACHE lru{0};
CACHECONFIG cc;







namespace fs = std::filesystem;
// list all regular files in specified directory (default: current directory)
static std::vector<fs::path> files_in_directory(fs::path const& dirname = ".")
{
    std::vector<fs::path> files;
    for (auto const& entry : fs::directory_iterator(dirname)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    return files;
}

static std::string &truncate(std::string &s, size_t sz)
{
    if (s.size() > sz) { 
        s.resize(sz);
        s += "...";
    }
    return s;
}

int main(int argc, char *argv[])
{
    
    if(!readArgv(cc, argc, argv)){
        return 1;
    }
    
    if(cc.ecFlag){
        std::cout << "Program mode: LRU CACHE" << std::endl;
        std::cout << "BytesCapacity =" << cc.byteCapacity << std::endl;
        lru.maxCapacity = cc.byteCapacity;
        
        
    }
    else{
        std::cout << "Program mode: Normal" << std::endl;
    }
    
   
    
    
    
    
    using clock = std::chrono::steady_clock;
    std::cout << std::fixed;
    std::cout << "Base 64 encoding all files in current directory\n\n";
    auto files = files_in_directory();
    size_t sz_tot = 0;
    size_t esz_tot = 0;
    double time_tot = 0;
    
    
    for (auto &f : files) {
        
        auto sz = fs::file_size(f);
        sz_tot += sz;
        std::cout << std::setw(19) << sz << " bytes | name=" << f << '\n';
        auto const ts1 = clock::now();
                
        std::string encoded;

        encoded = base64encode(f);
        
        auto const ts2 = clock::now();
        time_tot += std::chrono::duration<double>(ts2 - ts1).count();

        auto const esz = encoded.size();
        esz_tot += esz;
        truncate(encoded, 64);
        std::cout << std::setw(19) << esz << " bytes | encoding=" << encoded << "\n";
        std::cout << '\n';
    }
    auto const throughput = sz_tot / time_tot * 1e-6;
    std::cout << "Statistics\n" << std::string(40, '-') << '\n';
    std::cout << files.size()   << " files encoded\n";
    std::cout << sz_tot         << " bytes of unencoded data\n";
    std::cout << esz_tot        << " bytes of encoded data\n";
    std::cout << time_tot       << " seconds to encode all files\n";
    std::cout << throughput     << " MB/s throughput\n";
}


bool readArgv(CACHECONFIG &cc , int &argc ,char *argv[])
{
    for(int i = 1; i < argc; i++){
        std::string_view arg{argv[i]};
        if(arg == "-ec"){
            cc.enableCache = true;
            cc.ecFlag = true;
        }
        else if(arg == "-cac"){
            if (i + i >= argc){
                std::cerr << "Error, integer must follow -cac flag. USAGE: -cac (integer)" << std::endl;
                return false;
            }
        
            std::string_view argValue{argv[++i]};
            int n = 0;
            auto result = std::from_chars(argValue.begin(), argValue.end(),n);
            if(result.ec != std::errc{}){
                std::cerr << "Error, value after -cac flag must be an integer! Got: '" << argValue << "'" << std::endl;
                return false;
            }
        
            cc.cacFlag = true;
            cc.byteCapacity = n;
        }
        else {
            std::cerr << "Nu gick något snett! " << arg << std::endl;
            return false;
        } 
    }
    return true;
}


