#include "index.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

using std::ifstream;
using std::ofstream;
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

bool TrigramData::operator==(const Trigram& rhs) const
{
    return tri.hash == rhs.hash;
}

bool TrigramData::operator<(const TrigramData& rhs) const
{
    return tri.hash < rhs.tri.hash;
}

bool TrigramData::operator<(const Trigram& rhs) const
{
    return tri.hash < rhs.hash;
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
    auto end = m_trigrams.end();
    auto it = std::lower_bound(m_trigrams.begin(), end, newTri);
    if (it != end && *it == newTri)
    {
        it->positions.push_back({
                .fileId=fileHash,
                .offset=offset,
            });
        return;
    }
    else
    {
        m_trigrams.insert(it, TrigramData{.tri = newTri, .positions {{.fileId = fileHash, .offset = offset}}});
    }
}

bool Index::IndexFile(const string_view& path)
{
    ifstream file { path.data(), std::ios::binary | std::ios::ate };
    if (!file.is_open())
        return false;

    const std::filesystem::path p { path.data() };
    const FileData newFile { std::filesystem::absolute(p).c_str() };
    auto it = std::lower_bound(m_files.begin(), m_files.end(), newFile);
    m_files.insert(it, newFile);
    // auto newFileIt = 
    // m_files.emplace_back(std::filesystem::absolute(p).c_str());

    // printf("file: %s hash: %d\n", m_files.back().filePath.data(), m_files.back().fileHash);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content;
    content.resize(size); // Pre-allocate the memory block

    if (!file.read(&content[0], size)) {
        return false;
    }

    Trigram tri;
    const u32 fileHash = newFile.fileHash;

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

    //printf("%s - %zu trigrams\n", path.data(), m_trigrams.size());
    // for (const TrigramData& t : m_trigrams)
    // {
    //     printf("%s", t.ToString().data());
    // }

    return true;
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

u32 Index::Search(string_view query)
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
    
    return potentialResults.size();
}
