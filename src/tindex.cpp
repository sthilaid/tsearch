#include "index.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
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

void resolve_dirs(vector<string>& inputFiles, const vector<string_view>& extensions, auto& it, u32& totalBytes)
{
    const std::filesystem::path filePath { it->data() };
    if (!std::filesystem::exists(filePath))
    {
        it = inputFiles.erase(it); // remove links
    }
    else if (std::filesystem::is_directory(filePath))
    {
        it = inputFiles.erase(it);
        for (auto dirIt : std::filesystem::directory_iterator(filePath))
       {
            it = inputFiles.insert(it, std::filesystem::absolute(dirIt.path()).c_str());
        }        
    }
    else if (!extensions.empty())
    {
        const string ext = filePath.extension().c_str();
        auto extIt = std::find(extensions.begin(), extensions.end(), ext);
        if (extIt == extensions.end())
        {
            it = inputFiles.erase(it);
        }
        else
        {
            ++it; // keep it, extension valid
            totalBytes += std::filesystem::file_size(filePath);
        }
    }
    else
    {
        ++it; // keep all files
        totalBytes += std::filesystem::file_size(filePath);
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
        u32 totalBytes = 0;
        auto it=inputFiles.begin();
        while (it != inputFiles.end()) {
            resolve_dirs(inputFiles, extensions, it, totalBytes);
        }

        // printf("output: %s\n", outputFile.data());
        // printf("input: \n");
        // for (const string& f : inputFiles) {
        //     printf("\t%s\n", f.data());
        // }

        string absoluteOutputFile = std::filesystem::absolute(outputFile);

        auto start = steady_clock::now();

        // Index newIndex {};
        // newIndex.Init(absoluteOutputFile);

        // const u32 totalFiles = inputFiles.size();
        // for (int i=0; i<totalFiles; ++i) {
        //     const string& f = inputFiles[i];
        //     const u32 precount = newIndex.TrigramCount();

        //     printf("[%d/%d] %s", i+1, totalFiles, f.data());
        //     newIndex.IndexFile(f);
        //     const u32 postcount = newIndex.TrigramCount();
        //     printf(" - %d new trigrams - total: %d\n", (postcount-precount), postcount);
        // }

        const u32 totalFiles = inputFiles.size();
        auto indexFun = [&inputFiles, totalFiles] (Index& index, u32 threadID, u32 from, u32 to) {
            for (int i=from; i<to; ++i) {
                const string& f = inputFiles[i];
                const u32 precount = index.TrigramCount();
                index.IndexFile(f);
                const u32 postcount = index.TrigramCount();
                printf("[%d - %d/%d] %s - %d new trigrams - total: %d\n", threadID, i-from+1, to-from, f.data(), (postcount-precount), postcount);
            }};

        #define min(a,b) (a<b ? a : b)
        #define max(a,b) (a>=b ? a : b)
        const u32 threadCount = max(1, min(8, totalFiles));
        vector<Index> indexes;
        indexes.resize(threadCount);
        for (Index& i : indexes) i.Init(absoluteOutputFile);
        
        vector<std::jthread> threads;
        threads.resize(threadCount);
        
        const u32 filesPerThread = totalFiles / threadCount;
        for (u32 t=0; t<threadCount; ++t)
        {
            const u32 from = t*filesPerThread;
            const u32 to = (t+1)*filesPerThread;
            threads[t] = std::jthread(indexFun, std::ref(indexes[t]), t, from, to);
            printf("thread %d from %d to %d\n", t, from, to);
        }
        for (u32 t=0; t<threadCount; ++t) threads[t].join();

        auto end = steady_clock::now();
        auto elapsed = end - start;

        printf("Indexation finished in %.4f secs.\n", duration<float>(elapsed).count());
        
        for (u32 t=1; t<threadCount; ++t) indexes[0].Merge(std::move(indexes[t]));
        const Index& newIndex = indexes[0];

        start = end;
        end = steady_clock::now();
        elapsed = end - start;

        printf("Indexation Merging in %.4f secs. Indexed %d kB (avg: %.2f kB/sec)\n",
               duration<float>(elapsed).count(),
               totalBytes/1000, totalBytes / (1000*duration<float>(elapsed).count()));

        start = end;
        ofstream indexFile { outputFile.data(), std::ios::binary };
        if (indexFile.is_open())
        {
            auto [data,in,out] = zpp::bits::data_in_out();
            std::ignore = out(newIndex);

            auto processed = out.processed_data();
            indexFile.write(reinterpret_cast<const char*>(processed.data()), processed.size());
            //newIndex.Serialize(indexFile);
        }
        end = steady_clock::now();
        elapsed = end - start;
        const u32 indexFileSize = std::filesystem::file_size(outputFile);
        printf("index serialization (%.2u kB) finished in %.4f secs.\n", indexFileSize/1000, duration<float>(elapsed).count());
    } else {
        printf("usage: tindex <options> [files to index]\n");
    }

    return 0;
}
