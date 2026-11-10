#include "tindex.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

using std::ifstream;
using std::string;

//-----------------------------------------------------------------------------
// Trigram

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

//-----------------------------------------------------------------------------
// TrigramData

bool TrigramData::operator==(const TrigramData& rhs) const
{
    return tri.hash == rhs.tri.hash;
}

bool TrigramData::operator<(const TrigramData& rhs) const
{
    return tri.hash < rhs.tri.hash;
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

//-----------------------------------------------------------------------------
// FileData

FileData::FileData(const char* path)
    : filePath { path }
{
    fileHash = std::hash<string>{}(filePath);
}

bool FileData::operator<(const FileData& rhs) const
{
    return fileHash < rhs.fileHash;
}

//-----------------------------------------------------------------------------
// Index

void Index::AccumulateTrigram(const Trigram& newTri, u32 fileHash, u32 offset)
{
    // todo, optimize search
    for (TrigramData& t : m_trigrams)
    {
        if (t.tri.hash == newTri.hash)
        {
            t.positions.push_back({
                    .fileId=fileHash,
                    .offset=offset,
                });
            return;
        }
    }
    m_trigrams.push_back({.tri=newTri});
    m_trigrams.back().positions.push_back({.fileId = fileHash, .offset = offset});
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

    Trigram tri;
    const u32 fileHash = m_files.back().fileHash;

    char last = '\0';
    for (u32 i=0; i<size; ++i)
    {
        char current = content[i];
        if ((last == ' ' || last == '\t' || last == '\n') && (current == ' ' || current == '\t' || current == '\n')) continue;
        tri << current;
        if (i<3) continue;
        
        AccumulateTrigram(tri, fileHash, i);
        last = current;
    }

    printf("%s - %zu trigrams\n", path.data(), m_trigrams.size());
    // for (const TrigramData& t : m_trigrams)
    // {
    //     printf("%s", t.ToString().data());
    // }

    return true;
}

void Index::TidyIndex()
{
    std::sort(m_files.begin(), m_files.end());
    std::sort(m_trigrams.begin(), m_trigrams.end());

    // printf("--- files (%zu) ---\n", m_files.size());
    // for (const FileData& fd : m_files)
    // {
    //     printf("%s", std::format("<{}> - {}\n", fd.fileHash, fd.filePath).data());;
    // }

    // printf("--- trigrams (%zu) ---\n", m_trigrams.size());
    // for (const TrigramData& td : m_trigrams)
    // {
    //     printf("[%d] (%zu),", td.tri.hash, td.positions.size());
    // }
    // printf("\n");
}

string lineAt(ifstream& in, u32 pos)
{
    in.clear();  // clear eof/fail bits from earlier reads

    std::streamoff start = pos;

    // Walk backwards to find the start of the line
    while (start > 0) {
        in.seekg(start - 1);
        char c;
        if (!in.get(c)) return {};
        if (c == '\n') break;
        --start;
    }

    in.seekg(start);
    std::string line;
    std::getline(in, line);
    return line;
}

string Index::ResultToString(const TrigramFilePos& fp) const
{
    FileData fd { "" };
    fd.fileHash = fp.fileId;
    
    auto searchIt = std::lower_bound(m_files.begin(), m_files.end(), fd);
    if (searchIt != m_files.end())
    {
        const string& filepath = (*searchIt).filePath;
        
        ifstream file { filepath.data(), std::ios::binary };
        if (file.is_open())
        {
            string line = lineAt(file, fp.offset);
            return std::format("{}#{} : {}", filepath, fp.offset, line);
        }
    }

    return "<!UnknownFile!>";
}

bool Index::Search(string_view query)
{
    if (query.size() < 3) return false;

    printf("Search for \"%s\":\n", query.data());

    TrigramData queryTrigram;
    queryTrigram.tri << query[0];
    queryTrigram.tri << query[1];

    vector<TrigramFilePos> potentialResults;
    
    for (int i=2; i<query.size(); ++i)
    {
        queryTrigram.tri << query[i];
        auto searchIt = std::lower_bound(m_trigrams.begin(), m_trigrams.end(), queryTrigram);
        if (searchIt == m_trigrams.end() || (*searchIt) != queryTrigram)
            return false;

        const TrigramData& tdata = *searchIt;
        if (i==2)
        {
            potentialResults = tdata.positions;
        }
        else
        {
            std::erase_if(potentialResults, [&tdata, i](const TrigramFilePos& potentialFilePos) {
                return std::none_of(tdata.positions.begin(), tdata.positions.end(), [&potentialFilePos, i](const TrigramFilePos& fp) {
                    return fp.fileId == potentialFilePos.fileId  && fp.offset == potentialFilePos.offset + i-2;
                });
            });
        }
    }

    printf("-- %zu results --\n", potentialResults.size());
    for (const TrigramFilePos& res : potentialResults)
    {
        printf("%s\n", ResultToString(res).data());
    }
    
    return potentialResults.size() > 0;
}

//-----------------------------------------------------------------------------
// main

bool ParseArguments(const vector<string_view> args, string_view& outputFile, vector<string_view>& inputFiles, bool& verbose, string_view& search)
{
    bool parsingFiles = false;
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i][0] == '-')  {
            if (parsingFiles)
                return false;

            if (args[i] == "-v" || args[i] == "--verbose") {
                verbose = true;
            } else if ((args[i] == "-o" || args[i] == "--output") && i + 1 < args.size() ) {
                outputFile = args[++i]; // Consume the next argument as the value
            } else if ((args[i] == "-s" || args[i] == "--search") && i + 1 < args.size() ) {
                search = args[++i]; // Consume the next argument as the value
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
    string_view search = "";
    vector<string_view> inputFiles;

    if (ParseArguments(args, outputFile, inputFiles, verbose, search)) {
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
        newIndex.TidyIndex();
        newIndex.Search(search);
        
    } else {
        printf("usage: tindex <options> [files to index]\n");
    }

    return 0;
}
