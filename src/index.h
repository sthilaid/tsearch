#pragma once

#include "zpp_bits.h"

#include <string>
#include <string_view>
#include <vector>

using std::string;
using std::string_view;
using std::vector;

using u32 = unsigned int;

struct Trigram
{
    char tri[4];
    u32 hash;

    Trigram();
    Trigram& operator<<(char c);
    const char* ToStr() const;
};

struct TrigramFilePos
{
    u32 fileId { 0 };
    u32 offset { 0 };
};

struct TrigramData
{
    Trigram tri;
    vector<TrigramFilePos> positions;
    string ToString() const;

    bool operator==(const TrigramData& rhs) const;
    bool operator<(const TrigramData& rhs) const;
};

struct FileData
{
    u32 fileHash { 0 };
    string filePath;

    FileData() = default;
    FileData(const char* path);
    bool operator<(const FileData& rhs) const;
};

class Index 
{
public:
    bool IndexFile(const string_view& path);
    void TidyIndex();
    bool Search(string_view query);
    u32 TrigramCount() const { return m_trigrams.size(); }

    friend zpp::bits::access;

private:
    void AccumulateTrigram(const Trigram& newTri, u32 fileHash, u32 offset);
    string ResultToString(const TrigramFilePos& fp) const;
    
    vector<TrigramData> m_trigrams;
    vector<FileData> m_files;
};
