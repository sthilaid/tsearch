#pragma once

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
};

struct FileData
{
    u32 fileHash { 0 };
    string filePath;

    FileData(const char* path);
};

class Index 
{
public:
    bool IndexFile(const string_view& path);

private:
    void AccumulateTrigram(const TrigramData& tri);
    
    vector<TrigramData> m_trigrams;
    vector<FileData> m_files;
};
