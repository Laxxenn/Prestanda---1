#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>

#include "lru-cache.hh"

#include <string_view>
#include <charconv>
#include <random>
#include <iomanip>  // for std::setw, std::fixed, etc.

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
void uniformTest(std::vector<std::filesystem::path> &files);
void skewedTest(std::vector<std::filesystem::path> &files);

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
        lru.maxCapacity = cc.byteCapacity;
    }

    using clock = std::chrono::steady_clock;
    std::cout << std::fixed;
    std::cout << "Base 64 encoding all files in current directory\n\n";
    auto files = files_in_directory();
    size_t sz_tot = 0;
    size_t esz_tot = 0;
    double time_tot = 0;

    // Decide which tests to run
    bool runUniform = cc.unFlag;
    bool runSkewed  = cc.skFlag;
    if (!runUniform && !runSkewed) {
        // default: run both if none specified
        runUniform = true;
        runSkewed  = true;
    }

    // Helper: run a test twice — baseline (no-cache) and LRU (if -ec)
    auto run_test_variant = [&](auto&& testFn, char const* label) {
        // --- Baseline: no cache ---
        bool restoreEC = cc.enableCache;     // remember original setting
        cc.enableCache = false;
        lru.clearCache();
        lru.resetStats();

        auto t1 = clock::now();
        testFn(files);
        auto t2 = clock::now();
        double baseSecs = std::chrono::duration<double>(t2 - t1).count();

        std::cout << "--------------------\n\n" << label << " (no-cache) time: " << baseSecs << "\n";
        lru.printStats(); // expected zeros with no cache

        // --- LRU: only if user enabled -ec ---
        if (cc.ecFlag) {
            cc.enableCache = true;
            lru.clearCache();
            lru.resetStats();

            auto t3 = clock::now();
            testFn(files);
            auto t4 = clock::now();
            double lruSecs = std::chrono::duration<double>(t4 - t3).count();

            std::cout << label << " (LRU) time: " << lruSecs << "\n";
            lru.printStats();
        }

        // restore
        cc.enableCache = restoreEC;
    };

    // Run selected tests
    if (runUniform) run_test_variant(uniformTest, "Uniform test");
    if (runSkewed)  run_test_variant(skewedTest,  "Skewed test");

    // Regular per-file pass and throughput calculation (as before)
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
    std::cout << "------------------------------" << std::endl;
    std::cout << "Run info: " << std::endl;
    if(cc.enableCache){
        std::cout << "Program mode: LRU CACHE" << std::endl;
        std::cout << "BytesCapacity =" << cc.byteCapacity << std::endl;
    }else{
        std::cout << "Program mode: Normal" << std::endl;
    }
    std::cout << "------------------------------" << std::endl;
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
            if (i + 1 >= argc){
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
        else if(arg == "-un"){
            cc.unFlag = true;
        }
        else if(arg == "-sk"){
            cc.skFlag = true;
        }
        else {
            std::cerr << "Nu gick något snett! " << arg << std::endl;
            return false;
        }
    }
    return true;
}

void uniformTest(std::vector<std::filesystem::path> &files)
{
    int iterations = 10;

    std::size_t sink = 0; // consume outputs to prevent optimization
    for (int i = 1; i <= iterations; ++i){
        for(auto &f : files ){
            std::string encoded = base64encode(f);
            sink += encoded.size();
        }
    }
    volatile auto keep = sink; (void)keep;
}

void skewedTest(std::vector<std::filesystem::path> &files)
{
    if (files.empty()) return;

    constexpr int    avg_times = 10;
    constexpr double pick_prob = 0.20;

    // Fixed seed for reproducibility; change to std::random_device{}() if you want fresh runs
    std::mt19937_64 rng{0xC0FFEE};
    std::bernoulli_distribution pick(pick_prob);

    std::vector<int> counts(files.size(), 0);
    std::size_t total_target = files.size() * avg_times;
    std::size_t total_done   = 0;

    std::size_t sink = 0; // consume outputs to prevent optimization

    while (total_done < total_target) {
        for (std::size_t i = 0; i < files.size() && total_done < total_target; ++i) {
            if (pick(rng)) {
                std::string encoded = base64encode(files[i]);
                sink += encoded.size();
                ++counts[i];
                ++total_done;
            }
        }
    }

    volatile auto keep = sink; (void)keep;
}
