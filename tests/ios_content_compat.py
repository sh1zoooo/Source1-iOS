#!/usr/bin/env python3
"""Startup ordering, phone/tablet layout and bounded built-in touch atlas assets."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parent.parent
source=r'''
#include "IOSStartupState.hpp"
#include "IOSClientModLayout.hpp"
#include "IOSTouchFallback.hpp"
#include <cassert>
#include <vector>
#include <array>
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
extern "C" const char *__asan_default_options() { return "detect_leaks=0"; }
int main() {
    IOSStartupState startup;
    // Background/loading frames cannot remove the overlay before menu init.
    for(int i=0;i<100;++i) assert(!startup.presented());
    assert(!startup.done()); startup.menuReady(); startup.menuReady();
    assert(startup.ready() && !startup.done());
    assert(startup.presented() && startup.done());
    for(int i=0;i<100;++i) { startup.menuReady(); assert(!startup.presented()); }
    const int sizes[][2]={{2532,1170},{2778,1284},{1334,750},{1792,828},
        {2732,2048},{2048,2732},{1024,768},{640,360}};
    for (auto &size:sizes) {
        auto l=IOSClientModLayout::fit(size[0],size[1]);
        assert(l.x>=0 && l.y>=0 && l.width+l.x<=size[0] && l.height+l.y<=size[1]);
        assert(l.font>=12 && l.row>l.font);
        const int sheetHeight=l.height-l.row*3/2-l.margin*3;
        const int pageRow=sheetHeight/6;
        assert(pageRow>l.font && pageRow>l.margin);
        assert(l.height-l.row-l.margin > l.margin+l.row/2+sheetHeight);
    }
    const char *names[]={"shoot","jump","shoot_alt","crouch","use","reload",
        "drop","slot1","slot2","slot3","slot4","slot5","voice_chat","menu",
        "settings","buymenu","back","chat","change_team","speed","scoreboard"};
    for (const char *name:names) {
        char path[128]; std::snprintf(path,sizeof(path),"vgui/touch/%s",name);
        std::vector<unsigned char> p(128*128*4+2,0xAB);
        assert(IOSTouchFallback::render(path,p.data()+1,p.size()-2));
        assert(p.front()==0xAB && p.back()==0xAB);
        assert(p[4]==0); // corner alpha is transparent, no full-screen pink squares.
        int white=0, translucent=0;
        for(int i=0;i<128*128;++i) {
            if(p[1+i*4+3]==255) ++white;
            if(p[1+i*4+3]==90) ++translucent;
        }
        assert(white>200 && translucent>1000);
        assert(!IOSTouchFallback::render(path,p.data(),1));
    }
    assert(!IOSTouchFallback::render(nullptr,nullptr,0));
    // Actual stb packer: an area-rounded atlas does not always pack all rects.
    // Use the same checked growing path as the production touch loader.
    const int counts[]={1,4,5,16,17,21,31,65};
    for(int count:counts) {
        std::vector<stbrp_rect> rects(count);
        for(auto &r:rects) r.w=r.h=128;
        int size=128; while(size*size<count*128*128) size*=2;
        for(;;) {
            std::vector<stbrp_node> nodes(size*2); stbrp_context ctx;
            stbrp_init_target(&ctx,size,size,nodes.data(),nodes.size());
            stbrp_pack_rects(&ctx,rects.data(),rects.size());
            bool packed=true; for(auto &r:rects) if(!r.was_packed) packed=false;
            if(packed) break;
            size*=2; assert(size<=8192);
        }
        for(int i=0;i<count;++i) {
            auto &a=rects[i]; assert(a.x+a.w<=size && a.y+a.h<=size);
            for(int j=0;j<i;++j) {
                auto &b=rects[j];
                assert(a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y);
            }
        }
    }
}
'''
source=source.replace('#include <array>','#include <array>\n#include <cstdio>')
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.cpp').write_text(source)
 subprocess.run(['c++','-std=c++14','-O1','-fsanitize=address,undefined',
    '-fno-omit-frame-pointer','-I'+str(root/'src/source'),
    '-I'+str(root/'third_party/clientmod/game/client'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**__import__('os').environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('iOS content compatibility: startup/menu order, 8 phone/tablet layouts, 21 bounded action assets and atlas packing PASS')
