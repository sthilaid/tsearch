#include "index.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

using std::ifstream;
using std::ofstream;
using std::string;

namespace fs = std::filesystem;

fs::path PathFromUtf8(string_view utf8)
{
    return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

string Utf8FromPath(const fs::path& path)
{
    const std::u8string utf8 = path.generic_u8string();
    return string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

vector<string> Utf8Arguments(int argc, const char** argv)
{
#ifdef _WIN32
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv != nullptr)
    {
        vector<string> args;
        for (int i = 1; i < wargc; ++i)
        {
            const int size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            string arg(size > 0 ? size - 1 : 0, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, arg.data(), size, nullptr, nullptr);
            args.push_back(std::move(arg));
        }
        LocalFree(wargv);
        return args;
    }
#endif
    return vector<string>(argv + 1, argv + argc);
}

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

FileData::FileData(string path)
    : filePath { std::move(path) }
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
    
    const fs::path filePath = PathFromUtf8(path);
    ifstream file { filePath, std::ios::binary | std::ios::ate };
    if (!file.is_open())
        return false;

    const fs::path parent = PathFromUtf8(m_indexFilePath).parent_path();
    const FileData newFile { Utf8FromPath(fs::relative(filePath, parent)) };
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

std::tuple<string,u32> lineAt(ifstream& in, u32 pos)
{
    in.clear();  // clear eof/fail bits from earlier reads

    u32 lineCount = 1;
    std::vector<char> buf(1 << 16);
    std::streamoff remaining = pos;

    while (remaining > 0 && in) {
        auto n = std::min<std::streamoff>(remaining, buf.size());
        in.read(buf.data(), n);
        auto got = in.gcount();
        lineCount += std::count(buf.data(), buf.data() + got, '\n');
        remaining -= got;
    }

    std::streamoff start = pos;
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
    return {line, lineCount};
}

string Index::ResultToString(const TrigramFilePos& fp) const
{
    FileData fd { "" };
    fd.fileHash = fp.fileId;
    
    auto searchIt = std::lower_bound(m_files.begin(), m_files.end(), fd);
    if (searchIt != m_files.end())
    {
        const string& filepath = (*searchIt).filePath;

        const fs::path absolutePath = PathFromUtf8(m_indexFilePath).parent_path() / PathFromUtf8(filepath);
        ifstream file { absolutePath, std::ios::binary };
        if (file.is_open())
        {
            auto [line,lineCount] = lineAt(file, fp.offset);
            return std::format("{}:{}:{}", Utf8FromPath(absolutePath), lineCount, line);
        }
    }

    return std::format("<!UnknownFile! ({})>", fd.fileHash);
}

u32 Index::Search(string_view query, bool incensitive)
{
    if (m_indexFilePath.empty()) return false; // not init
    if (query.size() < 3) return false; 

    printf("Search for \"%s\":\n", query.data());

    TrigramData queryTrigram;
    queryTrigram.tri << query[0];
    queryTrigram.tri << query[1];

    vector<TrigramFilePos> potentialResults;

    // todo implement case insensitive search
    
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

void Index::Merge(Index&& rhs)
{
    // todo multithread this too?
    for (TrigramData& rhsData : std::move(rhs.m_trigrams))
    {
        auto searchIt = std::lower_bound(m_trigrams.begin(), m_trigrams.end(), rhsData);
        if (searchIt == m_trigrams.end() || (*searchIt) != rhsData)
        {
            m_trigrams.insert(searchIt, std::move(rhsData));
        }
        else
        {
            (*searchIt).positions.insert((*searchIt).positions.end(), rhsData.positions.begin(), rhsData.positions.end());
        }
    }
    for (FileData& rhsFileData : std::move(rhs.m_files))
    {
        auto searchIt = std::lower_bound(m_files.begin(), m_files.end(), rhsFileData);
        m_files.insert(searchIt, std::move(rhsFileData));
    }
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
