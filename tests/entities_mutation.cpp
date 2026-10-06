extern "C" int __lsan_is_turned_off(){return 1;}
extern "C" const char* __asan_default_options(){return "detect_leaks=0";}
#include "SourceEntities.hpp"
#include <iostream>
#include <random>
int main(){std::mt19937 random(0x510522);unsigned accepted=0,rejected=0;const std::string fixture="{ classname worldspawn } { classname info_player_counterterrorist origin \"-190 -160 16\" angles \"8 45 0\" }";
for(unsigned i=0;i<10000;++i){auto text=fixture;if(i%5==0)text.resize(random()%(text.size()+1));else for(unsigned n=0;n<1+i%4;++n)text[random()%text.size()]^=1u<<(random()%8);std::vector<source1ios::PreviewSpawn> spawns(1);spawns[0].classname="unchanged";if(source1ios::parsePreviewSpawns(text,spawns)){++accepted;for(auto& s:spawns)for(float n:s.origin)if(!std::isfinite(n))return 1;}else{++rejected;if(spawns.size()!=1||spawns[0].classname!="unchanged")return 2;}}
std::cout<<accepted<<" accepted, "<<rejected<<" rejected; 10000 entity mutations passed\n";}
