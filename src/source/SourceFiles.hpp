#pragma once
#include <filesystem>

namespace source1ios {
class SourceFiles final {
public:
    bool start(const std::filesystem::path& root, void* filesystem);
    void stop();
    bool selfTest();
    bool ready() const { return initialized_; }
private:
    void* interface_ = nullptr;
    bool initialized_ = false;
    std::filesystem::path root_;
};
}
