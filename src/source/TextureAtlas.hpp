#pragma once
#include "RenderTypes.hpp"
#include <algorithm>
#include <cstring>
namespace source1ios {
// Keep imported detail where possible, bounded by iOS texture and memory limits.
inline bool appendTextureTile(SourceTexture& atlas,unsigned& count,const SourceTexture& image){
    if(count>=512||!image.width||!image.height||image.width>2048||image.height>2048
       ||image.pixels.size()!=size_t(image.width)*image.height*4)return false;
    const unsigned oldColumns=std::min(16u,count),columns=std::min(16u,count+1),rows=(count+columns)/columns;
    const unsigned oldTile=count?atlas.width/oldColumns:0;
    if(count&&(oldTile<64||oldTile>512||(oldTile&(oldTile-1))||atlas.width!=oldColumns*oldTile||atlas.height!=((count+oldColumns-1)/oldColumns)*oldTile
       ||atlas.pixels.size()!=size_t(atlas.width)*atlas.height*4))return false;
    unsigned wanted=64;while(wanted<std::max(image.width,image.height)&&wanted<512)wanted*=2;
    unsigned tile=std::max(wanted,oldTile);
    while(tile>64&&(columns*tile>8192||rows*tile>8192||size_t(columns)*rows*tile*tile*4>64u*1024*1024))tile/=2;
    SourceTexture next{columns*tile,rows*tile,std::vector<std::uint8_t>(size_t(columns)*rows*tile*tile*4)};
    for(unsigned slot=0;slot<count;++slot)for(unsigned y=0;y<tile;++y)for(unsigned x=0;x<tile;++x){
        const size_t source=(size_t(slot/oldColumns*oldTile+y*oldTile/tile)*atlas.width+slot%oldColumns*oldTile+x*oldTile/tile)*4;
        const size_t target=(size_t(slot/columns*tile+y)*next.width+slot%columns*tile+x)*4;
        std::memcpy(next.pixels.data()+target,atlas.pixels.data()+source,4);
    }
    for(unsigned y=0;y<tile;++y)for(unsigned x=0;x<tile;++x){
        const size_t source=(size_t(y*image.height/tile)*image.width+x*image.width/tile)*4;
        const size_t target=(size_t(count/columns*tile+y)*next.width+count%columns*tile+x)*4;
        std::memcpy(next.pixels.data()+target,image.pixels.data()+source,4);
    }
    atlas=std::move(next);++count;return true;
}
}
