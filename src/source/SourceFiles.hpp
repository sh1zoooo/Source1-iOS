#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace source1ios {
class SourceFiles final {
public:
    bool start(const std::filesystem::path& root, void* filesystem);
    void stop();
    bool selfTest();
    bool contentSelfTest();
    bool probeContent(const std::string& path);
    bool mountContent(const std::string& name);
    bool unmountContent(const std::string& name);
    bool ready() const { return initialized_; }
private:
    void* interface_ = nullptr;
    bool initialized_ = false;
    std::filesystem::path root_;
    struct ContentPath {
        std::string name;
        std::filesystem::path path;
        std::vector<std::filesystem::path> archives;
    };
    std::vector<ContentPath> content_;
};
}
