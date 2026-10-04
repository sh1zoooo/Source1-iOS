#pragma once
#include <memory>

namespace source1ios {
class SourceAppSystems final {
public:
    SourceAppSystems();
    ~SourceAppSystems();
    SourceAppSystems(const SourceAppSystems&) = delete;
    SourceAppSystems& operator=(const SourceAppSystems&) = delete;
    bool start();
    void stop();
    void* find(const char* name) const;
    bool selfTest() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
