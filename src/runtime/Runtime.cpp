#include "Runtime.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace source1ios {
bool Runtime::start(const std::filesystem::path& documents) {
    if (running_) return false;
    std::error_code error;
    std::filesystem::create_directories(documents / "Source1IOS", error);
    if (error) return false;
    logPath_ = documents / "Source1IOS" / "runtime.log";
    log_.clear();
    log_.open(logPath_, std::ios::out | std::ios::trunc);
    if (!log_) return false;
    frames_ = 0;
    elapsed_ = 0;
    running_ = active_ = true;
    log("Host started; Source engine modules are not linked.");
    log("Pointer width: " + std::to_string(sizeof(void*) * 8));
    return true;
}
void Runtime::setActive(bool active) {
    if (!running_ || active == active_) return;
    active_ = active;
    log(active ? "Host resumed" : "Host paused");
}
void Runtime::frame(double seconds) {
    if (!running_ || !active_ || !std::isfinite(seconds) || seconds < 0) return;
    // Do not replay a long background interval as a simulation step.
    elapsed_ += std::min(seconds, 0.1);
    ++frames_;
}
void Runtime::log(const std::string& message) {
    std::clog << "[Source1IOS] " << message << '\n';
    if (log_.is_open()) {
        log_ << message << '\n';
        log_.flush();
    }
}
void Runtime::stop() {
    if (!running_) return;
    log("Host stopped after " + std::to_string(frames_) + " frames");
    running_ = active_ = false;
    log_.close();
}
}
