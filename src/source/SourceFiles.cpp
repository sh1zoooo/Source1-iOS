#include "SourceFiles.hpp"
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <set>
#include "filesystem.h"
#include "tier0/dbg.h"
#include "tier1/checksum_crc.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"

namespace {
void report(const char* name, bool passed) {
    Msg("Source filesystem self-test %s: %s\n", name, passed ? "PASS" : "FAIL");
}
void little(std::vector<unsigned char>& out, unsigned value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) out.push_back((value >> (8*i)) & 255);
}
void string(std::vector<unsigned char>& out, const char* text) {
    out.insert(out.end(), text, text + std::strlen(text) + 1);
}
// A tiny VPK v1/v2 fixture with embedded data, independently constructed from the
// on-disk format. Reading and searching it uses upstream CPackedStore code.
std::vector<unsigned char> fixture(const char* payload, unsigned version) {
    std::vector<unsigned char> tree;
    string(tree, "txt"); string(tree, "fixture"); string(tree, "hello");
    CRC32_t crc; CRC32_Init(&crc);
    CRC32_ProcessBuffer(&crc, payload, std::strlen(payload)); CRC32_Final(&crc);
    little(tree, crc, 4); little(tree, 0, 2); little(tree, 0x7fff, 2);
    little(tree, 0, 4); little(tree, std::strlen(payload), 4); little(tree, 0xffff, 2);
    tree.insert(tree.end(), {0, 0, 0});
    std::vector<unsigned char> out;
    little(out, 0x55aa1234, 4); little(out, version, 4); little(out, tree.size(), 4);
    if (version == 2) {
        little(out, std::strlen(payload), 4); little(out, 0, 4); little(out, 0, 4); little(out, 0, 4);
    }
    out.insert(out.end(), tree.begin(), tree.end());
    out.insert(out.end(), payload, payload + std::strlen(payload));
    return out;
}
unsigned read32(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
unsigned read16(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);}
bool checkedVpk(const std::filesystem::path& directoryFile,std::vector<std::filesystem::path>& chunks,std::uint64_t& bytes){
    std::error_code error;const auto size=std::filesystem::file_size(directoryFile,error);
    if(error||size<12||size>64u*1024*1024||std::filesystem::is_symlink(directoryFile,error)||error)return false;
    std::ifstream input(directoryFile,std::ios::binary);std::vector<unsigned char> data(size);if(!input.read(reinterpret_cast<char*>(data.data()),data.size()))return false;
    if(read32(data.data())!=0x55aa1234)return false;const unsigned version=read32(data.data()+4),treeSize=read32(data.data()+8);if(version!=1&&version!=2)return false;
    const size_t header=version==1?12:28;if(header>data.size()||treeSize>data.size()-header)return false;
    const size_t embedded=version==1?data.size()-header-treeSize:read32(data.data()+12);
    if(header+size_t(treeSize)>data.size()||embedded>data.size()-header-treeSize)return false;
    const size_t end=header+treeSize;size_t cursor=header;std::set<unsigned> indices;
    auto word=[&](){const size_t begin=cursor;while(cursor<end&&data[cursor])++cursor;if(cursor>=end)return false;++cursor;return cursor>begin+1;};
    while(true){const size_t before=cursor;if(!word()){if(cursor==before+1)break;return false;}
        while(true){const size_t path=cursor;if(!word()){if(cursor==path+1)break;return false;}
            while(true){const size_t name=cursor;if(!word()){if(cursor==name+1)break;return false;}if(end-cursor<18)return false;
                const unsigned preload=read16(data.data()+cursor+4),archive=read16(data.data()+cursor+6),offset=read32(data.data()+cursor+8),length=read32(data.data()+cursor+12),terminator=read16(data.data()+cursor+16);cursor+=18;
                if(terminator!=0xffff||preload>end-cursor)return false;cursor+=preload;
                if(archive==0x7fff){if(std::uint64_t(offset)+length>embedded)return false;}else indices.insert(archive);
            }
        }
    }
    if(cursor!=end||indices.size()>512)return false;
    auto base=directoryFile.string();if(base.size()<8||base.substr(base.size()-8)!="_dir.vpk")return false;base.resize(base.size()-8);
    for(unsigned index:indices){char suffix[16];std::snprintf(suffix,sizeof(suffix),"_%03u.vpk",index);std::filesystem::path chunk=base+suffix;
        const auto chunkSize=std::filesystem::file_size(chunk,error);if(error||chunkSize>512u*1024*1024||std::filesystem::is_symlink(chunk,error)||error||std::uint64_t(chunkSize)>8ull*1024*1024*1024-bytes)return false;
        chunks.push_back(chunk);bytes+=chunkSize;}
    bytes+=size;return bytes<=8ull*1024*1024*1024;
}
}

namespace source1ios {
bool SourceFiles::start(const std::filesystem::path& root, void* filesystem) {
    if (interface_) return false;
    root_ = root;
    std::error_code error;
    std::filesystem::create_directories(root_ / "game", error);
    if (error) return false;
    std::filesystem::create_directories(root_ / "selftest", error);
    if (error) return false;
    std::filesystem::create_directories(root_ / "content", error);
    if (error) return false;
    auto* fs = static_cast<IFileSystem*>(filesystem);
    if (!fs) return false;
    interface_ = fs;
    initialized_ = true; // Lifecycle is owned by the original CAppSystemGroup.
    fs->AddSearchPath((root_ / "game").c_str(), "GAME");
    fs->AddSearchPath((root_ / "game").c_str(), "DEFAULT_WRITE_PATH");
    // Original encrypted weapon scripts are requested through MOD, while game
    // writes must stay in the app-owned directory rather than imported content.
    fs->AddSearchPath((root_ / "game").c_str(), "MOD");
    fs->AddSearchPath((root_ / "game").c_str(), "MOD_WRITE");
    fs->AddSearchPath((root_ / "game").c_str(), "GAME_WRITE");
    fs->MarkPathIDByRequestOnly("MOD", true);
    fs->MarkPathIDByRequestOnly("MOD_WRITE", true);
    fs->MarkPathIDByRequestOnly("GAME_WRITE", true);
    fs->AddSearchPath((root_ / "selftest").c_str(), "PORT_TEST");
    fs->MarkPathIDByRequestOnly("PORT_TEST", true);
    fs->MarkPathIDByRequestOnly("PORT_VPK", true);
    Msg("Source factory: %s initialized\n", FILESYSTEM_INTERFACE_VERSION);
    Msg("Source GAME search path: %s\n", (root_ / "game").c_str());
    if (!selfTest()) { stop(); return false; }
    for(const char* game:{"cm","cstrike_clientmod","cstrike"}){
        if(std::filesystem::is_directory(root_/"content"/game,error)&&!mountContent(game))Warning("Source content skipped: %s\n",game);error.clear();}
    for(const char* shared:{"hl2","platform"}){if(std::filesystem::is_directory(root_/"content"/shared,error)&&!mountContent(shared))Warning("Source content skipped: %s\n",shared);error.clear();}
    Msg("Source filesystem initialized: filesystem_stdio/vpklib\n");
    return true;
}
void SourceFiles::stop() {
    auto* fs = static_cast<IFileSystem*>(interface_);
    if (!fs) return;
    if (initialized_) {
        fs->AsyncFinishAll();
        fs->RemoveAllSearchPaths();
        Msg("Source filesystem search paths removed\n");
    }
    interface_ = nullptr;
    initialized_ = false;
    content_.clear();
}
bool SourceFiles::mountContent(const std::string& name){
    if(!initialized_||name.empty()||name.size()>120||name.front()=='/'||name.back()=='/')return false;
    // Only unpacked directories under our content root are accepted. Dots,
    // absolute paths and archive names never reach Source's mount dispatch.
    for(char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'))return false;
    if(name.find("//")!=std::string::npos)return false;
    if(std::any_of(content_.begin(),content_.end(),[&](const auto& entry){return entry.name==name;}))return true;
    if(content_.size()>=16)return false;
    std::error_code error;if(std::filesystem::is_symlink(root_/"content",error)||error)return false;
    const auto base=std::filesystem::canonical(root_/"content",error);if(error)return false;
    auto componentPath=base;for(const auto& component:std::filesystem::path(name)){componentPath/=component;if(std::filesystem::is_symlink(componentPath,error)||error)return false;}
    const auto path=std::filesystem::canonical(base/name,error);if(error||!std::filesystem::is_directory(path,error)||error)return false;
    const auto relative=path.lexically_relative(base);if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||path.string().size()>=MAX_PATH)return false;
    auto dispatchPath=path.string();for(char& c:dispatchPath)if(c>='A'&&c<='Z')c+=('a'-'A');
    if(dispatchPath.find(".bsp")!=std::string::npos||dispatchPath.find(".vpk")!=std::string::npos)return false;
    // Validate metadata and every VPK directory/chunk range before letting the
    // original CPackedStore parser see untrusted cache files.
    size_t entries=0;std::vector<std::filesystem::path> archives,chunks;std::uint64_t packedBytes=0;std::filesystem::recursive_directory_iterator it(path,error),end;
    while(!error&&it!=end){if(++entries>100000||it.depth()>32||it->is_symlink(error)||error)return false;
        auto filename=it->path().filename().string();for(char& c:filename)if(c>='A'&&c<='Z')c+=('a'-'A');if(filename=="zip0.zip"||filename=="zip0.360.zip")return false;
        if(filename.size()>8&&filename.substr(filename.size()-8)=="_dir.vpk"){if(archives.size()>=32||!checkedVpk(it->path(),chunks,packedBytes))return false;auto base=it->path().string();base.resize(base.size()-8);archives.emplace_back(base+".vpk");}it.increment(error);}
    if(error)return false;
    std::sort(archives.begin(),archives.end());
    auto* fs=static_cast<IFileSystem*>(interface_);fs->AddSearchPath(path.c_str(),"GAME",PATH_ADD_TO_TAIL);
    for(const auto& archive:archives)fs->AddSearchPath(archive.c_str(),"GAME",PATH_ADD_TO_TAIL);
    if(name=="cm"||name=="cstrike_clientmod"||name=="cstrike"){
        fs->AddSearchPath(path.c_str(),"MOD",PATH_ADD_TO_TAIL);
        for(const auto& archive:archives)fs->AddSearchPath(archive.c_str(),"MOD",PATH_ADD_TO_TAIL);
    }
    content_.push_back({name,path,archives});Msg("Source content mounted: %s; GAME search path: %s; %zu bounded VPK archives, %zu chunks, %llu bytes\n",name.c_str(),path.c_str(),archives.size(),chunks.size(),static_cast<unsigned long long>(packedBytes));return true;
}
std::vector<std::string> SourceFiles::maps(){
    std::vector<std::string> result;if(!initialized_)return result;
    // Refresh mounts when resources were copied in Files while the app was open.
    for(const auto* folder:{"hl2","platform","cstrike","cm"})mountContent(folder);
    auto* fs=static_cast<IFileSystem*>(interface_);FileFindHandle_t handle;
    const char* name=fs->FindFirstEx("maps/*.bsp","GAME",&handle);
    for(unsigned count=0;name&&count<512;++count,name=fs->FindNext(handle)){
        const std::string file(name);if(fs->FindIsDirectory(handle)||file.size()<5)continue;
        const auto stem=file.substr(0,file.size()-4);
        if(file.substr(file.size()-4)==".bsp"&&stem.size()<=64&&stem.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos)result.push_back(stem);
    }
    if(handle!=FILESYSTEM_INVALID_FIND_HANDLE)fs->FindClose(handle);
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
bool SourceFiles::unmountContent(const std::string& name){
    if(!initialized_)return false;const auto found=std::find_if(content_.begin(),content_.end(),[&](const auto& entry){return entry.name==name;});if(found==content_.end())return false;
    auto* fs=static_cast<IFileSystem*>(interface_);fs->AsyncFinishAll();bool removed=fs->RemoveSearchPath(found->path.c_str(),"GAME");for(auto archive=found->archives.rbegin();archive!=found->archives.rend();++archive)removed=fs->RemoveSearchPath(archive->c_str(),"GAME")&&removed;if(!removed)return false;
    if(name=="cm"||name=="cstrike_clientmod"||name=="cstrike"){
        removed=fs->RemoveSearchPath(found->path.c_str(),"MOD")&&removed;
        for(const auto& archive:found->archives)removed=fs->RemoveSearchPath(archive.c_str(),"MOD")&&removed;
        if(!removed)return false;
    }
    content_.erase(found);Msg("Source content unmounted: %s\n",name.c_str());return true;
}
bool SourceFiles::contentSelfTest(){
    if(!initialized_)return false;const char expected[]="Source filesystem on iOS\n";char bytes[sizeof(expected)]{};
    auto* fs=static_cast<IFileSystem*>(interface_);auto file=fs->Open("fixture/hello.txt","rb","GAME");
    const bool passed=file&&fs->Size(file)==int(sizeof(expected)-1)&&fs->Read(bytes,sizeof(expected)-1,file)==int(sizeof(expected)-1)&&!std::memcmp(bytes,expected,sizeof(expected)-1);
    if(file)fs->Close(file);report("mounted VPK v2 embedded file",passed);return passed;
}
bool SourceFiles::probeContent(const std::string& path){
    if(!initialized_||path.empty()||path.size()>=MAX_PATH||path.front()=='/'||path.back()=='/'||path.find("//")!=std::string::npos)return false;
    for(char c:path)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'))return false;
    for(const auto& component:std::filesystem::path(path))if(component=="."||component=="..")return false;
    auto* fs=static_cast<IFileSystem*>(interface_);auto file=fs->Open(path.c_str(),"rb","GAME");if(!file)return false;
    const auto size=fs->Size(file);std::array<unsigned char,16> prefix{};const int wanted=std::min<int>(prefix.size(),size);const bool passed=size>0&&size<=64*1024*1024&&fs->Read(prefix.data(),wanted,file)==wanted;fs->Close(file);
    if(passed)Msg("Source content probe: %s; %d bytes; prefix %02x%02x%02x%02x\n",path.c_str(),size,prefix[0],prefix[1],prefix[2],prefix[3]);return passed;
}
bool SourceFiles::selfTest() {
    if (!initialized_) return false;
    auto* fs = static_cast<IFileSystem*>(interface_);
    const char payload[] = "Source filesystem on iOS\n";
    constexpr int length = sizeof(payload) - 1;
    bool all = true;
    auto file = fs->Open("roundtrip.txt", "wb", "PORT_TEST");
    bool passed = file && fs->Write(payload, length, file) == length;
    if (file) fs->Close(file);
    report("write", passed); all &= passed;
    char bytes[64] = {};
    file = fs->Open("roundtrip.txt", "rb", "PORT_TEST");
    passed = file && fs->Size(file) == length && fs->Read(bytes, length, file) == length
        && !std::memcmp(bytes, payload, length);
    if (file) { fs->Seek(file, 7, FILESYSTEM_SEEK_HEAD); passed &= fs->Tell(file) == 7; fs->Close(file); }
    report("read/seek", passed); all &= passed;
    FileFindHandle_t find;
    const char* found = fs->FindFirstEx("roundtrip.*", "PORT_TEST", &find);
    passed = found && !std::strcmp(found, "roundtrip.txt");
    if (found) fs->FindClose(find);
    passed &= fs->FileExists("roundtrip.txt", "PORT_TEST")
        && !fs->FileExists("roundtrip.txt", "GAME")
        && !fs->FileExists("missing-port-file.bin", "PORT_TEST");
    report("search paths", passed); all &= passed;
    auto* kv = new KeyValues("SourcePort");
    kv->SetString("platform", "ios"); kv->SetInt("bits", 64);
    passed = kv->SaveToFile(fs, "config.kv", "PORT_TEST");
    kv->deleteThis(); kv = new KeyValues("SourcePort");
    passed &= kv->LoadFromFile(fs, "config.kv", "PORT_TEST")
        && !std::strcmp(kv->GetString("platform"), "ios") && kv->GetInt("bits") == 64;
    kv->deleteThis(); report("KeyValues file", passed); all &= passed;
    FileAsyncRequest_t request;
    request.pszFilename = "roundtrip.txt"; request.pszPathID = "PORT_TEST";
    request.pData = bytes; request.nBytes = length;
    FSAsyncControl_t control = nullptr;
    std::memset(bytes, 0, sizeof(bytes));
    passed = fs->AsyncReadMultiple(&request, 1, &control) >= FSASYNC_OK && control;
    if (control) {
        passed &= fs->AsyncFinish(control, true) == FSASYNC_OK
            && !std::memcmp(bytes, payload, length);
        fs->AsyncRelease(control);
    }
    report("async read", passed); all &= passed;
    for (unsigned version : {1u, 2u}) {
        const auto archive = fixture(payload, version);
        const std::string name = "fixture" + std::to_string(version);
        const std::string directoryFile = name + "_dir.vpk";
        file = fs->Open(directoryFile.c_str(), "wb", "PORT_TEST");
        passed = file && fs->Write(archive.data(), archive.size(), file) == int(archive.size());
        if (file) fs->Close(file);
        // Source canonicalizes the mounted path by stripping the _dir suffix.
        const auto path = (root_ / "selftest" / (name + ".vpk")).string();
        fs->AddSearchPath(path.c_str(), "PORT_VPK");
        std::memset(bytes, 0, sizeof(bytes));
        file = fs->Open("fixture/hello.txt", "rb", "PORT_VPK");
        passed &= file && fs->Read(bytes, length, file) == length && !std::memcmp(bytes, payload, length);
        if (file) fs->Close(file);
        passed &= fs->RemoveSearchPath(path.c_str(), "PORT_VPK");
        report(version == 1 ? "VPK v1 embedded file" : "VPK v2 embedded file", passed); all &= passed;
    }
    return all;
}
}
