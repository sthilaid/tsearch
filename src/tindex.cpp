#include "tindex.h"

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

using std::ifstream;
using std::string;

Trigram::Trigram()
    : hash(0)
{
    tri[0] = '\0';
    tri[1] = '\0';
    tri[2] = '\0';
    tri[3] = '\0';
}

Trigram& Trigram::operator<<(char c) 
{
    tri[0] = tri[1];
    tri[1] = tri[2];
    tri[2] = c;

    hash = (tri[2] << 16) + (tri[1] << 8) + tri[0];

    return *this;
}

const char* Trigram::ToStr() const
{
    return tri;
}

string TrigramData::ToString() const
{
    string s = std::format("[{}] - \n", tri.ToStr());
    for (const TrigramFilePos& fp : positions)
    {
        s += std::format("\t{} : {}\n", fp.fileId, fp.offset);
    }
    s += "\n";
    return s;
}

FileData::FileData(const char* path)
    : filePath { path }
{
    fileHash = std::hash<string>{}(filePath);
}

void Index::AccumulateTrigram(const TrigramData& newTri)
{
    // todo, optimize search
    for (TrigramData& t : m_trigrams)
    {
        if (t.tri.hash == newTri.tri.hash)
        {
            t.positions.push_back({
                    .fileId=newTri.positions[0].fileId,
                    .offset=newTri.positions[0].offset,
                });
        }
    }
    m_trigrams.push_back(newTri);
}

bool Index::IndexFile(const string_view& path)
{
    ifstream file { path.data(), std::ios::binary | std::ios::ate };
    if (!file.is_open())
        return false;

    std::filesystem::path p { path.data() };
    m_files.emplace_back(std::filesystem::absolute(p).c_str());

    // printf("file: %s hash: %d\n", m_files.back().filePath.data(), m_files.back().fileHash);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content;
    content.resize(size); // Pre-allocate the memory block

    if (!file.read(&content[0], size)) {
        return false;
    }

    TrigramData t;
    t.positions.push_back({.fileId = m_files.back().fileHash});

    char last = '\0';
    for (u32 i=0; i<size; ++i)
    {
        char current = content[i];
        if ((last == ' ' || last == '\t' || last == '\n') && (current == ' ' || current == '\t' || current == '\n')) continue;
        t.tri << current;
        if (i<3) continue;
        
        t.positions.back().offset = i;
        AccumulateTrigram(t);
    }

    printf("%s - %zu trigrams\n", path.data(), m_trigrams.size());
    // for (const TrigramData& t : m_trigrams)
    // {
    //     printf("%s", t.ToString().data());
    // }

    return true;
}

bool ParseArguments(const vector<string_view> args, string_view& outputFile, vector<string_view>& inputFiles)
{
    bool verbose = false;
    bool parsingFiles = false;
    
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

    string_view outputFile = ".tindex";
    vector<string_view> inputFiles;

    if (ParseArguments(args, outputFile, inputFiles)) {
        // printf("output: %s\n", outputFile.data());
        // printf("input: \n");
        // for (const string_view& f : inputFiles)
        // {
        //     printf("\t%s\n", f.data());
        // }

        Index newIndex {};
        
        for (const string_view& f : inputFiles) {
            newIndex.IndexFile(f);
        }
    } else {
        printf("usagea: tindex <options> [files to index]\n");
    }

    return 0;
}
