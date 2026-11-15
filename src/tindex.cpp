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

bool ParseArguments(
    const vector<string_view> args,
    string_view& outputFile,
    vector<string>& inputFiles,
    bool& verbose,
    vector<string_view>& extensions)
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
            } else if ((args[i] == "-e" || args[i] == "--extension") && i + 1 < args.size() ) {
                extensions.push_back(args[++i]); // Consume the next argument as the value
            } 
        }
        else {
            parsingFiles = true;
            inputFiles.push_back(args[i].data());
        }
    }

    return inputFiles.size() > 0;
}

void resolve_dirs(vector<string>& inputFiles, const vector<string_view>& extensions, auto& it)
{
    const std::filesystem::path filePath { it->data() };
    if (!std::filesystem::is_directory(filePath))
        return;

    it = inputFiles.erase(it);
    for (auto dirIt : std::filesystem::directory_iterator(filePath))
    {
        const string ext = dirIt.path().extension().c_str();
        if (extensions.empty() || std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
        {
            it = inputFiles.insert(it, std::filesystem::absolute(dirIt.path()).c_str());
        }
    }
}

int main(int argc, const char** argv)
{
    // Skip argv[0] (the program name)
    const vector<string_view> args(argv + 1, argv + argc);

    bool verbose = true;
    string_view outputFile = ".tindex";
    vector<string> inputFiles;
    vector<string_view> extensions;

    if (ParseArguments(args, outputFile, inputFiles, verbose, extensions)) {
        for (auto it=inputFiles.begin(); it !=inputFiles.end(); ++it) {
            resolve_dirs(inputFiles, extensions, it);
        }

        // printf("output: %s\n", outputFile.data());
        // printf("input: \n");
        // for (const string& f : inputFiles) {
        //     printf("\t%s\n", f.data());
        // }

        auto start = steady_clock::now();

        Index newIndex {};

        const u32 totalFiles = inputFiles.size();
        for (int i=0; i<totalFiles; ++i) {
            const string& f = inputFiles[i];
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
