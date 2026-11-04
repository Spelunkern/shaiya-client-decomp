#include "archive.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif

using namespace shaiya::portable;
static void u32(std::ofstream& f, unsigned v) { for (int i=0;i<4;++i) f.put(char(v >> (i*8))); }
static void name(std::ofstream& f, const char* s) { std::string n(s); u32(f, unsigned(n.size()+1)); f.write(n.c_str(), n.size()+1); }
static void file(std::ofstream& f, const char* n, std::uint64_t offset, bool wide) { name(f,n); u32(f,unsigned(offset)); if(wide) u32(f,unsigned(offset>>32)); u32(f,3); u32(f,0); }
int main() {
    auto dir=std::filesystem::temp_directory_path()/"shaiya_native_archive_test";
    std::filesystem::create_directories(dir);
    for (bool wide: {false,true}) {
        auto index=dir/(wide?"wide.sah":"legacy.sah"), data=dir/"data.saf";
        { std::ofstream f(data,std::ios::binary); f << "abcdef"; }
        { std::ofstream f(index,std::ios::binary);
          if(wide) { f.write("SAH",3); for(int i=0;i<48;++i) f.put(0); }
          else { u32(f,0); u32(f,0); }
          name(f,""); u32(f,0); u32(f,1); name(f,"Dir"); u32(f,2);
          file(f,"A.txt",0,wide); file(f,"a.TXT",3,wide); u32(f,0);
        }
        auto a=Archive::open(index,data);
        if(a.paths()!=std::vector<std::string>{"data/Dir/a.TXT"}) return 4;
        auto bytes=a.read("DATA\\dir/A.TXT");
        if (std::string(bytes.begin(),bytes.end()) != "def" || a.find("bad/dir/a.txt")) return 1;
        { std::ofstream f(index,std::ios::binary|std::ios::trunc); f << "SAH"; }
        try { (void)Archive::open(index,data); return 2; } catch(const std::runtime_error&) {}
    }
    if(archive_name_hash(std::string("\xff",1)) != 177572u) return 3;
    // Sparse data checks a real seek past 4 GiB without allocating that space.
    const auto sparse=dir/"sparse.saf",wide=dir/"sparse.sah";
    constexpr std::uint64_t high_offset=(std::uint64_t(1)<<32)+17;
#ifdef _WIN32
    {
        HANDLE f=CreateFileW(sparse.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f==INVALID_HANDLE_VALUE) return 7;
        DWORD count=0;LARGE_INTEGER offset;offset.QuadPart=high_offset;
        bool ok=DeviceIoControl(f,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&count,nullptr) &&
            SetFilePointerEx(f,offset,nullptr,FILE_BEGIN) && WriteFile(f,"xyz",3,&count,nullptr) && count==3;
        CloseHandle(f);if(!ok) return 7;
    }
#else
    { std::ofstream f(sparse,std::ios::binary); f.seekp(static_cast<std::streamoff>(high_offset));f<<"xyz"; }
#endif
    { std::ofstream f(wide,std::ios::binary);f.write("SAH",3);for(int i=0;i<48;++i) f.put(0);
      name(f,"");u32(f,1);file(f,"high.bin",high_offset,true);u32(f,0); }
    auto a=Archive::open(wide,sparse);auto bytes=a.read("data/high.bin");
    if(std::string(bytes.begin(),bytes.end())!="xyz") return 5;
    std::filesystem::resize_file(sparse,high_offset+1);
    try { (void)a.read("data/high.bin");return 6; }catch(const std::runtime_error&) {}
    std::filesystem::remove_all(dir);
}
