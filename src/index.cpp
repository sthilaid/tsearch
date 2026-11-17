#include "index.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <string>

using std::ifstream;
using std::ofstream;
using std::string;

template<typename T>
void read(ifstream& input, T* v, u32 size) {
    input.read(reinterpret_cast<char*>(v), size);
}

template<typename T>
void write(ofstream& output, T* v, u32 size) {
    output.write(reinterpret_cast<char*>(v), size);
}

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

void Trigram::Serialize(ofstream& file)
{
    write(file, tri, sizeof(tri));
    write(file, &hash, sizeof(hash));
}

Trigram Trigram::Unserialize(ifstream& file)
{
    Trigram out;
    read(file, &out.tri, sizeof(out.tri));
    read(file, &out.hash, sizeof(out.hash));
    return out;
}

//-----------------------------------------------------------------------------
// TrigramFilePos

void TrigramFilePos::Serialize(ofstream& file)
{
    write(file, &fileId, sizeof(fileId));
    write(file, &offset, sizeof(offset));
}

TrigramFilePos TrigramFilePos::Unserialize(ifstream& file)
{
    TrigramFilePos out;
    read(file, &out.fileId, sizeof(out.fileId));
    read(file, &out.offset, sizeof(out.offset));
    return out;
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

void TrigramData::Serialize(ofstream& file)
{
    tri.Serialize(file);

    u32 posCount = positions.size();
    write(file, &posCount, sizeof(posCount));
    for (TrigramFilePos& fp : positions) {
        fp.Serialize(file);
    }
}

TrigramData TrigramData::Unserialize(ifstream& file)
{
    TrigramData out;
    out.tri = Trigram::Unserialize(file);
    u32 posCount;
    read(file, &posCount, sizeof(posCount));

    out.positions.resize(posCount);
    for (u32 i=0; i<posCount; ++i)
    {
        out.positions[i] = TrigramFilePos::Unserialize(file);
    }

    return out;
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

void FileData::Serialize(ofstream& file)
{
    write(file, &fileHash, sizeof(fileHash));

    std::size_t filePathSize = filePath.size();
    write(file, &filePathSize, sizeof(filePathSize));
    write(file, filePath.data(), filePathSize);
}

FileData FileData::Unserialize(ifstream& file)
{
    FileData out;
    read(file, &out.fileHash, sizeof(out.fileHash));

    std::size_t filePathSize;
    read(file, &filePathSize, sizeof(filePathSize));
    out.filePath.resize(filePathSize);
    
    read(file, out.filePath.data(), filePathSize);
    return out;
}

//-----------------------------------------------------------------------------
// Index

void Index::Init(string_view indexAbsolutePath)
{
    m_indexFilePath = indexAbsolutePath;
}

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
    if (m_indexFilePath.empty())
        return false; // not init
    
    ifstream file { path.data(), std::ios::binary | std::ios::ate };
    if (!file.is_open())
        return false;

    std::filesystem::path parent { std::filesystem::path{m_indexFilePath}.parent_path() };
    std::filesystem::path relativePath { std::filesystem::relative(path.data(), parent) };
    const FileData newFile { relativePath.c_str() };
    auto it = std::lower_bound(m_files.begin(), m_files.end(), newFile);
    m_files.insert(it, newFile);

    // printf("file: %s hash: %u\n", m_files.back().filePath.data(), m_files.back().fileHash);

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

        std::filesystem::path parent { std::filesystem::path{m_indexFilePath}.parent_path() };
        const string absolutePath = (parent.string() + "/" + filepath).c_str();
        ifstream file { absolutePath, std::ios::binary };
        if (file.is_open())
        {
            string line = lineAt(file, fp.offset);
            return std::format("{}#{} : {}", absolutePath, fp.offset, line);
        }
    }

    return std::format("<!UnknownFile! ({})>", fd.fileHash);
}

u32 Index::Search(string_view query)
{
    if (m_indexFilePath.empty()) return false; // not init
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

void Index::Serialize(ofstream& outFile)
{
    u32 triSize = m_trigrams.size();
    write(outFile, &triSize, sizeof(triSize));

    u32 filesSize = m_files.size();
    write(outFile, &filesSize, sizeof(filesSize));

    for (TrigramData& tridata : m_trigrams)
        tridata.Serialize(outFile);

    for (FileData& file : m_files)
        file.Serialize(outFile);
}

Index Index::Unserialize(ifstream& inFile)
{
    Index index;
    
    u32 triSize;
    read(inFile, &triSize, sizeof(triSize));

    u32 filesSize;
    read(inFile, &filesSize, sizeof(filesSize));

    index.m_trigrams.resize(triSize);
    index.m_files.resize(filesSize);

    for (u32 i=0; i<triSize; ++i) {
        index.m_trigrams[i] = TrigramData::Unserialize(inFile);
    }

    for (u32 i=0; i<filesSize; ++i) {
        index.m_files[i] = FileData::Unserialize(inFile);
    }
    return index;
}
