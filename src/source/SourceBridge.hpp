#pragma once
#include <array>
#include <string>
#include "SourceFiles.hpp"
#include "SourceAppSystems.hpp"
#include "SourceHost.hpp"

namespace source1ios {
struct SourceVertex { float position[4]; float color[4]; };
class SourceBridge final {
public:
    using Logger = void (*)(void*, const char*);
    bool start(Logger logger, void* context, const std::filesystem::path& root);
    void stop();
    bool selfTest();
    bool execute(const std::string& command);
    void frame(double seconds);
    std::array<SourceVertex, 36> vertices(float aspect) const;
    bool ready() const { return ready_; }
private:
    bool ready_ = false;
    bool ownsCore_ = false;
    SourceAppSystems systems_;
    SourceFiles files_;
    SourceHost host_;
    double elapsed_ = 0;
};
}
