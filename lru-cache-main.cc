#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <string_view>
#include <charconv>
#include <random>
#include <iomanip>

#include "lru-cache.hh"   

bool readArgv(CACHECONFIG &cc , int &argc ,char *argv[]);
CACHECONFIG cc;

namespace fs = std::filesystem;
std::string base64encode(std::filesystem::path const& filename);

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
/*
    Function name: runAndTime

    Description:
        Runs the provided test function exactly once on the given file list,
        measuring the elapsed wall-clock time in seconds. Before running, this
        clears the LRU cache and resets its stats to ensure a clean measurement

    Parameters:
        testFn - Pointer to a function that takes a reference to a vector of paths and performs the test
        files  - Reference to the vector containing paths to the files in the directory

    Return:
        double - The elapsed time in seconds for a single invocation of testFn.
*/
static double runAndTime(void (*testFn)(std::vector<std::filesystem::path>&, std::uint64_t&),
                         std::vector<std::filesystem::path>& files,
                         std::uint64_t& bytes_out)
{
    lruClear();
    lruReset();

    auto t1 = std::chrono::steady_clock::now();
    testFn(files, bytes_out);                     
    auto t2 = std::chrono::steady_clock::now();

    return std::chrono::duration<double>(t2 - t1).count();
}

/*
    Function name: runTestVariant

    Description:
        Executes a test variant twice: first with the cache disabled (no-cache),
        then (if -ec was provided) with the LRU cache enabled. Prints timings
        for each run and, in the LRU case, also prints cache statistics.
        The previous cc.enableCache state is restored before returning.

    Parameters:
        label  - A label describing the test
        testFn - Pointer to a function that takes a reference to a vector of paths
                 and performs the test work.
        files  - Reference to the vector containing paths to the files in the directory.

    Return:
        void
*/
static void runTestVariant(const char* label,
                           void (*testFn)(std::vector<std::filesystem::path>&, std::uint64_t&),
                           std::vector<std::filesystem::path>& files)
{
    bool restoreEC = cc.enableCache;

    std::cout << "--------------------\n\n";

    cc.enableCache = false;
    std::uint64_t bytes_nc = 0;
    double secs_nc = runAndTime(testFn, files, bytes_nc);
    double thr_nc  = secs_nc > 0 ? (bytes_nc / secs_nc) * 1e-6 : 0.0; 
    std::cout << label << " (no-cache)  time: " << secs_nc
              << "  | bytes: " << bytes_nc
              << "  | throughput: " << thr_nc << " MB/s\n";

    if (cc.ecFlag) {
        cc.enableCache = true;
        std::uint64_t bytes_lru = 0;
        double secs_lru = runAndTime(testFn, files, bytes_lru);
        double thr_lru  = secs_lru > 0 ? (bytes_lru / secs_lru) * 1e-6 : 0.0; 
        std::cout << label << " (LRU)       time: " << secs_lru
                  << "  | bytes: " << bytes_lru
                  << "  | throughput: " << thr_lru << " MB/s\n";
        lruPrint();
    }

    cc.enableCache = restoreEC;
}





/*
    Function name UniformTest

    Parameters 
        files - A refrerence to the vector containing path to the files in the directory

    Return 
        void
*/
void uniformTest(std::vector<std::filesystem::path> &files, std::uint64_t& bytes_out)
{
    bytes_out = 0;
    int iterations = 10;

    for (int i = 1; i <= iterations; ++i){
        for (auto &f : files ){
            std::string encoded = base64encode(f);
            bytes_out += static_cast<std::uint64_t>(encoded.size());
        }
    }
    volatile auto keep = bytes_out; (void)keep;
}
/*
    Function name: SkewedTest

    Description: Tests the LRU-Cache by encoding files on average 10 times. 

    Parameters:
        files - A reference to the vector containing path to the files in the directory 

    Return
        void 

*/
void skewedTest(std::vector<std::filesystem::path> &files, std::uint64_t& bytes_out)
{
    bytes_out = 0;
    if (files.empty()) return;

    constexpr int    avg_times = 10;
    constexpr double pick_prob = 0.20;

    std::mt19937_64 rng{0xC0FFEE};
    std::bernoulli_distribution pick(pick_prob);

    std::vector<int> counts(files.size(), 0);
    std::size_t total_target = files.size() * avg_times;
    std::size_t total_done   = 0;

    while (total_done < total_target) {
        for (std::size_t i = 0; i < files.size() && total_done < total_target; ++i) {
            if (pick(rng)) {
                std::string encoded = base64encode(files[i]);
                bytes_out += static_cast<std::uint64_t>(encoded.size());
                ++counts[i];
                ++total_done;
            }
        }
    }

    volatile auto keep = bytes_out; (void)keep;
}

/*
    Function name: readArgv

    Description:
        Parses the commandline arguments for flags and capacity for the LRU cache 

    Parameters
        cc   - Reference to a cacheconfig struct that holds info regarding the cache
        argc - Amount of commands from terminal 
        argv - Array of strings containing etc flags
    
    Return:
        False if something fails to parse or missuse of flag
        True  if successfull parsing 

*/
bool readArgv(CACHECONFIG &cc , int &argc ,char *argv[])
{
    for (int i = 1; i < argc; i++) {
        std::string_view arg{argv[i]};
        if (arg == "-ec") {
            cc.enableCache = true;
            cc.ecFlag = true;
        }
        else if (arg == "-cac") {
            if (i + 1 >= argc) {
                std::cerr << "Error, integer must follow -cac flag. USAGE: -cac (integer)\n";
                return false;
            }

            std::string_view argValue{argv[++i]};
            int n = 0;
            auto result = std::from_chars(argValue.begin(), argValue.end(), n);
            if (result.ec != std::errc{}) {
                std::cerr << "Error, value after -cac flag must be an integer! Got: '" << argValue << "'\n";
                return false;
            }

            cc.cacFlag = true;
            cc.byteCapacity = n;
        }
        else {
            std::cerr << "Nu gick något snett! Okänd flagga: " << arg << "\n";
            return false;
        }
    }
    return true;
}



int main(int argc, char *argv[])
{
    if (!readArgv(cc, argc, argv)) {
        return 1;
    }

    if (cc.ecFlag) {
        lruSetCapacity(static_cast<std::size_t>(cc.byteCapacity));
    }

    using clock = std::chrono::steady_clock;
    std::cout << std::fixed;
    std::cout << "Base 64 encoding all files in current directory\n\n";

    auto files = files_in_directory();
    size_t sz_tot = 0;
    size_t esz_tot = 0;
    double time_tot = 0;

    runTestVariant("Uniform test", uniformTest, files);
    runTestVariant("Skewed test",  skewedTest,  files);


    for (auto &f : files) {
        auto sz = fs::file_size(f);
        sz_tot += sz;
        std::cout << std::setw(19) << sz << " bytes | name=" << f << '\n';

        auto const ts1 = clock::now();
        std::string encoded = base64encode(f);
        auto const ts2 = clock::now();

        time_tot += std::chrono::duration<double>(ts2 - ts1).count();

        auto const esz = encoded.size();
        esz_tot += esz;
        truncate(encoded, 64);
        std::cout << std::setw(19) << esz << " bytes | encoding=" << encoded << "\n\n";
    }

    auto const throughput = time_tot > 0.0 ? (sz_tot / time_tot * 1e-6) : 0.0;

    std::cout << "------------------------------\n";
    std::cout << "Run info:\n";
    if (cc.enableCache) {
        std::cout << "Program mode: LRU CACHE\n";
        std::cout << "BytesCapacity = " << cc.byteCapacity << "\n";
    } else {
        std::cout << "Program mode: Normal\n";
    }
    std::cout << "------------------------------\n";
    std::cout << "Statistics\n" << std::string(40, '-') << '\n';
    std::cout << files.size()   << " files encoded\n";
    std::cout << sz_tot         << " bytes of unencoded data\n";
    std::cout << esz_tot        << " bytes of encoded data\n";
    std::cout << time_tot       << " seconds to encode all files\n";
    std::cout << throughput     << " MB/s throughput\n";
    return 0;
}



