#pragma once

#include "zpp_bits.h"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using std::ifstream;
using std::ofstream;
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
    void Serialize(ofstream& file);
    static Trigram Unserialize(ifstream& file);
};

struct TrigramFilePos
{
    u32 fileId { 0 };
    u32 offset { 0 };
    void Serialize(ofstream& file);
    static TrigramFilePos Unserialize(ifstream& file);
};

struct TrigramData
{
    Trigram tri;
    vector<TrigramFilePos> positions;
    string ToString() const;

    bool operator==(const TrigramData& rhs) const;
    bool operator==(const Trigram& rhs) const;
    bool operator<(const TrigramData& rhs) const;
    bool operator<(const Trigram& rhs) const;
    void Serialize(ofstream& file);
    static TrigramData Unserialize(ifstream& file);
};

struct FileData
{
    u32 fileHash { 0 };
    string filePath;

    FileData() = default;
    FileData(const char* path);
    bool operator<(const FileData& rhs) const;
    void Serialize(ofstream& file);
    static FileData Unserialize(ifstream& file);
};

class Index 
{
public:
    void Init(string_view indexAbsolutePath);
    bool IndexFile(const string_view& path);
    u32 Search(string_view query);
    u32 TrigramCount() const { return m_trigrams.size(); }
    void Serialize(ofstream& file);
    static Index Unserialize(ifstream& file);

    friend zpp::bits::access;

private:
    void AccumulateTrigram(const Trigram& newTri, u32 fileHash, u32 offset);
    string ResultToString(const TrigramFilePos& fp) const;

    string m_indexFilePath;
    vector<TrigramData> m_trigrams;
    vector<FileData> m_files;
};
