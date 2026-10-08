#include "TextureAtlas.hpp"
#include <iostream>
#include <stdexcept>
using namespace source1ios;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 SourceTexture atlas;unsigned count=0;SourceTexture first{64,64,std::vector<std::uint8_t>(64*64*4,17)};
 check(appendTextureTile(atlas,count,first),"first slot");
 SourceTexture detailed{512,256,std::vector<std::uint8_t>(512*256*4)};
 for(unsigned y=0;y<256;++y)for(unsigned x=0;x<512;++x){auto* p=detailed.pixels.data()+(size_t(y)*512+x)*4;p[0]=x%256;p[1]=y;p[2]=x%2?255:0;p[3]=x%2?73:255;}
 check(appendTextureTile(atlas,count,detailed)&&count==2&&atlas.width==1024&&atlas.height==512,"high detail tile not retained");
 check(atlas.pixels[0]==17&&atlas.pixels[(size_t(511)*1024+511)*4]==17,"previous material changed");
 auto pixel=[&](unsigned slot,unsigned x,unsigned y){unsigned columns=std::min(16u,count),tile=atlas.width/columns;return atlas.pixels.data()+(size_t(slot/columns*tile+y)*atlas.width+slot%columns*tile+x)*4;};
 check(pixel(1,1,0)[2]==255&&pixel(1,2,0)[2]==0&&pixel(1,1,0)[3]==73,"fine detail or alpha lost");
 for(int i=0;i<15;++i)check(appendTextureTile(atlas,count,first),"append row");
 check(count==17&&atlas.width==8192&&atlas.height==1024&&pixel(1,1,0)[2]==255&&pixel(16,0,0)[0]==17,"row repacking corrupted materials");
 const auto before=atlas.pixels;auto bad=first;bad.pixels.pop_back();check(!appendTextureTile(atlas,count,bad)&&count==17&&atlas.pixels==before,"malformed image changed atlas");
 // The 65th slot forces 512 -> 256 pixels to stay within 64 MiB.
 atlas={8192,2048,std::vector<std::uint8_t>(8192*2048*4,41)};count=64;
 check(appendTextureTile(atlas,count,first)&&atlas.width==4096&&atlas.height==1280&&atlas.pixels[0]==41,"memory cap did not rescale retained tiles");
 std::cout<<"Texture atlas detail/alpha/rows/bounds/memory: PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
