#include "index.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using std::ifstream;
using std::ofstream;
using std::string;
using std::string_view;
using std::vector;


//-----------------------------------------------------------------------------
// main

bool ParseArguments(const vector<string_view> args, string_view& indexFile, string_view& search, bool& verbose)
{
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i][0] == '-')  {
            if (args[i] == "-v" || args[i] == "--verbose") {
                verbose = true;
            } else if ((args[i] == "-i" || args[i] == "--index") && i + 1 < args.size() ) {
                indexFile = args[++i]; // Consume the next argument as the value
            } 
        }
        else {
            search = args[i];
            return true; // only one search argument, last one
        }
    }

    return false;
}

int main(int argc, const char** argv)
{
    // Skip argv[0] (the program name)
    const vector<string_view> args(argv + 1, argv + argc);

    bool verbose = true;
    string_view indexPath = ".tindex";
    string_view search = "";

    if (ParseArguments(args, indexPath, search, verbose)) {

        ifstream indexFile { indexPath.data(), std::ios::binary };
        if (indexFile.is_open())
        {
            vector<std::byte> indexData(std::filesystem::file_size(indexPath));
            indexFile.read(reinterpret_cast<char*>(indexData.data()), static_cast<std::streamsize>(indexData.size()));
            if (!indexFile) {
                return 1; // short read or I/O error
            }
            
            Index index {};
            zpp::bits::in in { indexData };
            std::ignore = in(index);

            index.Search(search);
        }
    } else {
        printf("usage: tsearch <options> [search string]\n");
    }

    return 0;
}

