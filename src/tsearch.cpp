#include "index.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std::chrono;
using std::ifstream;
using std::ofstream;
using std::string;
using std::string_view;
using std::vector;


//-----------------------------------------------------------------------------
// main

bool ParseArguments(const vector<string_view> args, string_view& indexFile, string_view& search, bool& incensitive, bool& verbose)
{
    static constexpr const char* usageStr = "Usage: tsearch [OPTION]..PATTERN\n";
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i][0] == '-')  {
            if (args[i] == "-v" || args[i] == "--verbose") {
                verbose = true;
            } else if (args[i] == "-i") {
                incensitive = true;
            } else if ((args[i] == "-f" || args[i] == "--index-file") && i + 1 < args.size() ) {
                indexFile = args[++i]; // Consume the next argument as the value
            } else if (args[i] == "--help") {
                printf("%s", usageStr);
                printf("Options:\n");
                printf("  -v, --verbose         more verbose output\n");
                printf("  -i,                   case insensitive search\n");
                printf("  -f, --index-file      index file to use for search (pass as next argument)\n");
                return false;
            }
        }
        else {
            search = args[i];
            return true; // only one search argument, last one
        }
    }

    printf("%s", usageStr);
    printf("Try 'tsearch --help' for more information.\n");
    return false;
}

int main(int argc, const char** argv)
{
    // Skip argv[0] (the program name)
    const vector<string> utf8Args = Utf8Arguments(argc, argv);
    const vector<string_view> args(utf8Args.begin(), utf8Args.end());

    bool verbose = false;
    bool incensitive = false;
    string_view indexPath = ".tindex";
    string_view search = "";

    if (ParseArguments(args, indexPath, search, incensitive, verbose)) {

        auto start = steady_clock::now();
        const std::filesystem::path indexFilePath = PathFromUtf8(indexPath);
        ifstream indexFile { indexFilePath, std::ios::binary };
        if (indexFile.is_open())
        {
            vector<std::byte> indexData(std::filesystem::file_size(indexFilePath));
            indexFile.read(reinterpret_cast<char*>(indexData.data()), static_cast<std::streamsize>(indexData.size()));
            if (!indexFile) {
                return 1; // short read or I/O error
            }
            
            Index index {};
            zpp::bits::in in { indexData };
            std::ignore = in(index);

            string absoluteIndexFilePath = Utf8FromPath(std::filesystem::absolute(indexFilePath));
            index.Init(absoluteIndexFilePath);

            printf("index file: %s\n", indexPath.data());
            if (verbose)
            {
                auto end = steady_clock::now();
                printf("%s deserialization finished in %.4f secs.\n", absoluteIndexFilePath.data(), duration<float>(end-start).count());
            }

            start = steady_clock::now();

            const u32 resultCount = index.Search(search, incensitive);
            auto end = steady_clock::now();
            printf("Search found %d results in %.4f secs.\n", resultCount, duration<float>(end-start).count());
        }
    } 

    return 0;
}

