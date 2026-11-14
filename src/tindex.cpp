#include "index.h"

#include <chrono>
#include <cstdio>
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

bool ParseArguments(const vector<string_view> args, string_view& outputFile, vector<string_view>& inputFiles, bool& verbose)
{
    bool parsingFiles = false;
    //for (const string_view& v : args) printf("%s\n", v.data());
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i][0] == '-')  {
            if (parsingFiles)
                return false;

            if (args[i] == "-v" || args[i] == "--verbose") {
                verbose = true;
            } else if ((args[i] == "-o" || args[i] == "--output") && i + 1 < args.size() ) {
                outputFile = args[++i]; // Consume the next argument as the value
            } 
        }
        else {
            parsingFiles = true;
            inputFiles.push_back(args[i]);
        }
    }

    return inputFiles.size() > 0;
}

int main(int argc, const char** argv)
{
    // Skip argv[0] (the program name)
    const vector<string_view> args(argv + 1, argv + argc);

    bool verbose = true;
    string_view outputFile = ".tindex";
    vector<string_view> inputFiles;

    if (ParseArguments(args, outputFile, inputFiles, verbose)) {
        // printf("output: %s\n", outputFile.data());
        // printf("input: \n");
        // for (const string_view& f : inputFiles)
        // {
        //     printf("\t%s\n", f.data());
        // }

        auto start = steady_clock::now();

        Index newIndex {};

        const u32 totalFiles = inputFiles.size();
        for (int i=0; i<totalFiles; ++i) {
            const string_view& f = inputFiles[i];
            const u32 precount = newIndex.TrigramCount();
            newIndex.IndexFile(f);
            const u32 postcount = newIndex.TrigramCount();
            printf("[%d/%d] %s - %d new trigrams - total: %d\n", i+1, totalFiles, f.data(), (postcount-precount), postcount);
        }

        auto end = steady_clock::now();
        auto elapsed = end - start;

        printf("indexation finished in %.4f secs.\n", duration<float>(elapsed).count());

        start = steady_clock::now();
        ofstream indexFile { outputFile.data(), std::ios::binary };
        if (indexFile.is_open())
        {
            auto [data,in,out] = zpp::bits::data_in_out();
            std::ignore = out(newIndex);

            auto processed = out.processed_data();
            indexFile.write(reinterpret_cast<const char*>(processed.data()), processed.size());
        }
        end = steady_clock::now();
        elapsed = end - start;
        printf("index serialization finished in %.4f secs.\n", duration<float>(elapsed).count());
    } else {
        printf("usage: tindex <options> [files to index]\n");
    }

    return 0;
}
