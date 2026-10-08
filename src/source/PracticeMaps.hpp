#pragma once
#include <string>
namespace source1ios {
inline bool practiceMapSupported(const std::string& name){
    for(const auto* map:{"awp_lego_2","aim_map_csgo","de_mirage_go"})if(name==map)return true;return false;
}
}
