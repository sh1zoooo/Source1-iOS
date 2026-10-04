#include "SourceFiles.hpp"
#include <cstring>
#include <vector>
#include <string>
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
    auto* fs = static_cast<IFileSystem*>(filesystem);
    if (!fs) return false;
    interface_ = fs;
    initialized_ = true; // Lifecycle is owned by the original CAppSystemGroup.
    fs->AddSearchPath((root_ / "game").c_str(), "GAME");
    fs->AddSearchPath((root_ / "game").c_str(), "DEFAULT_WRITE_PATH");
    fs->AddSearchPath((root_ / "selftest").c_str(), "PORT_TEST");
    fs->MarkPathIDByRequestOnly("PORT_TEST", true);
    fs->MarkPathIDByRequestOnly("PORT_VPK", true);
    Msg("Source factory: %s initialized\n", FILESYSTEM_INTERFACE_VERSION);
    Msg("Source GAME search path: %s\n", (root_ / "game").c_str());
    if (!selfTest()) { stop(); return false; }
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
