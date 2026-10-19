#pragma once
// Ready means both an initialized menu and a frame presented AFTER that signal.
// Repeated phase/ready notifications cannot reopen or prolong the startup UI.
class IOSStartupState {
    bool menu = false, finished = false;
public:
    bool ready() const { return menu; }
    bool done() const { return finished; }
    void menuReady() { menu = true; }
    bool presented() {
        if (!menu || finished) return false;
        finished = true;
        return true;
    }
};
