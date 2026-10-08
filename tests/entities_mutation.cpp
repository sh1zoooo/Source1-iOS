extern "C" int __lsan_is_turned_off(){return 1;}
extern "C" const char* __asan_default_options(){return "detect_leaks=0";}
#include "SourceEntities.hpp"
#include <iostream>
#include <random>
int main(){
std::vector<source1ios::PreviewSpawn> skySpawns;std::string sky;
if(!source1ios::parsePreviewSpawns("{ classname worldspawn skyname sky_day01_01 }",skySpawns,nullptr,&sky)||sky!="sky_day01_01")return 10;
if(source1ios::parsePreviewSpawns("{ classname worldspawn skyname ../unsafe }",skySpawns,nullptr,&sky)||sky!="sky_day01_01")return 11;
std::mt19937 random(0x510522);unsigned accepted=0,rejected=0;const std::string fixture="{ classname worldspawn } { classname info_player_counterterrorist origin \"-190 -160 16\" angles \"8 45 0\" }";
for(unsigned i=0;i<10000;++i){auto text=fixture;if(i%5==0)text.resize(random()%(text.size()+1));else for(unsigned n=0;n<1+i%4;++n)text[random()%text.size()]^=1u<<(random()%8);std::vector<source1ios::PreviewSpawn> spawns(1);spawns[0].classname="unchanged";if(source1ios::parsePreviewSpawns(text,spawns)){++accepted;for(auto& s:spawns)for(float n:s.origin)if(!std::isfinite(n))return 1;}else{++rejected;if(spawns.size()!=1||spawns[0].classname!="unchanged")return 2;}}
const std::string model="{ classname prop_dynamic model models/test.mdl origin \"1 2 3\" angles \"0 90 0\" skin 1 body 2 modelscale 2 }";
std::vector<source1ios::PreviewSpawn> validSpawns;std::vector<source1ios::PreviewProp> validModels;
if(!source1ios::parsePreviewSpawns(model,validSpawns,&validModels)||validModels.size()!=1||validModels[0].skin!=1||validModels[0].scale!=2||validModels[0].solid!=0||validModels[0].body!=2)return 3;
for(unsigned i=0;i<20000;++i){auto text=model;if(i%5==0)text.resize(random()%(text.size()+1));else for(unsigned n=0;n<1+i%4;++n)text[random()%text.size()]^=1u<<(random()%8);
std::vector<source1ios::PreviewSpawn> spawns(1);spawns[0].classname="unchanged";std::vector<source1ios::PreviewProp> models(1);models[0].model="unchanged";
if(source1ios::parsePreviewSpawns(text,spawns,&models)){++accepted;for(const auto& p:models){if(!std::isfinite(p.scale)||p.scale<=0||p.scale>16||p.solid||p.skin<0||p.skin>255||p.body>65535)return 4;for(float n:p.origin)if(!std::isfinite(n))return 5;}}
else{++rejected;if(spawns.size()!=1||spawns[0].classname!="unchanged"||models.size()!=1||models[0].model!="unchanged")return 6;}}
for(const auto& bad:std::vector<std::string>{"models/../x.mdl","/models/x.mdl","models/x.dll","models//x.mdl","models/x:evil.mdl"}){
if(source1ios::parsePreviewSpawns("{ classname prop_dynamic model \""+bad+"\" }",validSpawns,&validModels))return 7;}
for(const auto& bad:std::vector<std::string>{"-1","1.5","65536","nan","inf","1x"}){
if(source1ios::parsePreviewSpawns("{ classname prop_dynamic model models/test.mdl body "+bad+" }",validSpawns,&validModels)||validModels.size()!=1||validModels[0].body!=2)return 9;}
std::string tooMany;for(unsigned i=0;i<513;++i)tooMany+=model;if(source1ios::parsePreviewSpawns(tooMany,validSpawns,&validModels))return 8;
std::cout<<accepted<<" accepted, "<<rejected<<" rejected; 30000 spawn/model entity mutations passed\n";}
