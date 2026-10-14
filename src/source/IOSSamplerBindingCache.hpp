#pragma once
#include <cstring>
template <unsigned Units = 16> class IOSSamplerBindingCache {
    unsigned names_[Units];
    bool valid_[Units]{};
public:
    void reset() { std::memset(valid_, 0, sizeof(valid_)); }
    template <class Bind> bool bind(unsigned unit, unsigned name, Bind call) {
        if (unit >= Units) { call(unit, name); return true; }
        if (valid_[unit] && names_[unit] == name) return false;
        call(unit, name);
        names_[unit] = name;
        valid_[unit] = true;
        return true;
    }
};
