#pragma once
#include <filesystem>
#include <string>

namespace source1ios {
// Original dedicated host in its upstream -nogamedll mode. No game or renderer.
class SourceHost final {
public:
    bool start(const std::filesystem::path& root);
    void stop();
    void frame(float seconds);
    bool selfTest();
private:
    bool started_ = false;
    std::string base_, game_;
};
}
