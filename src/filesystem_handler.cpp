// TODO(bwsq) simplify
// you know, this is a bunch of crap built on crap
// fuck istream and fuck sstream and fuck this.
#include <jinja2cpp/filesystem_handler.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <sstream>
#include <fstream>

namespace jinja2
{

struct FileContentConverter
{
    void operator() (const std::string& content, CharFileStreamPtr* sPtr) const
    {
        sPtr->reset(new std::istringstream(content));
    }
};

void MemoryFileSystem::AddFile(std::string fileName, std::string fileContent)
{
    m_filesMap[std::move(fileName)] = FileContent{std::move(fileContent)};
}

CharFileStreamPtr MemoryFileSystem::OpenStream(const std::string& name) const
{
    // WHAT THE FUCK IS THIS WHY
    // surely this cant be the right way of doing things
    // TODO(bwsq) grep for every static_cast, new, and delete in this project
    CharFileStreamPtr result(nullptr, [](std::istream* s) {delete static_cast<std::istringstream*>(s);});
    auto p = m_filesMap.find(name);
    if (p == m_filesMap.end())
        return result;

    auto& content = p->second;

    if (!content.narrowContent)
        return result;

    result.reset(new std::istringstream(content.narrowContent.value()));

    return result;
}

std::optional<std::chrono::system_clock::time_point> MemoryFileSystem::GetLastModificationDate(const std::string&) const
{
    return std::optional<std::chrono::system_clock::time_point>();
}

bool MemoryFileSystem::IsEqual(const IComparable& other) const
{
    auto* ptr = dynamic_cast<const MemoryFileSystem*>(&other);
    if (!ptr)
        return false;
    return m_filesMap == ptr->m_filesMap;
}

RealFileSystem::RealFileSystem(std::string rootFolder)
    : m_rootFolder(std::move(rootFolder))
{

}

std::string RealFileSystem::GetFullFilePath(const std::string& name) const
{
    boost::filesystem::path root(m_rootFolder);
    root /= name;
    return root.string();
}

CharFileStreamPtr RealFileSystem::OpenStream(const std::string& name) const
{
    auto filePath = GetFullFilePath(name);

    CharFileStreamPtr result(new std::ifstream(filePath), [](std::istream* s) {delete static_cast<std::ifstream*>(s);});
    if (result->good())
        return result;

    return CharFileStreamPtr(nullptr, [](std::istream*){});
}

std::optional<std::chrono::system_clock::time_point> RealFileSystem::GetLastModificationDate(const std::string& name) const
{
    boost::filesystem::path root(m_rootFolder);
    root /= name;

    auto modify_time = boost::filesystem::last_write_time(root);

    return std::chrono::system_clock::from_time_t(modify_time);
}
CharFileStreamPtr RealFileSystem::OpenByteStream(const std::string& name) const
{
    auto filePath = GetFullFilePath(name);

    CharFileStreamPtr result(new std::ifstream(filePath, std::ios_base::binary), [](std::istream* s) {delete static_cast<std::ifstream*>(s);});
    if (result->good())
        return result;

    return CharFileStreamPtr(nullptr, [](std::istream*){});
}

bool RealFileSystem::IsEqual(const IComparable& other) const
{
    auto* ptr = dynamic_cast<const RealFileSystem*>(&other);
    if (!ptr)
        return false;
    return m_rootFolder == ptr->m_rootFolder;
}

} // namespace jinja2
