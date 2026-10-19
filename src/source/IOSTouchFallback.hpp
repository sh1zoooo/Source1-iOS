#pragma once
#include <cmath>
#include <cstddef>
#include <string.h>
// Resource-free, readable action buttons for packs without vgui/touch assets.
// Uploaded into the existing touch atlas once; no per-frame text or new draws.
namespace IOSTouchFallback {
inline const char *label(const char *path) {
    const char *n=::strrchr(path,'/'); n=n ? n+1 : path;
    struct Entry { const char *name, *text; };
    static const Entry entries[]={
        {"use","USE"},{"jump","JUMP"},{"shoot","FIRE"},{"shoot_alt","ALT"},
        {"crouch","DUCK"},{"tduck","DUCK"},{"zoom","ZOOM"},{"speed","WALK"},
        {"load","LOAD"},{"save","SAVE"},{"reload","RLD"},{"flash_light_filled","LIGHT"},
        {"next_weap","NEXT"},{"prev_weap","PREV"},{"settings","EDIT"},{"menu","MENU"},
        {"back","BACK"},{"drop","DROP"},{"inspect","LOOK"},{"buymenu","BUY"},
        {"change_team","TEAM"},{"scoreboard","SCORE"},{"chat","CHAT"},{"chat_team","TEAM"},
        {"spray","SPRAY"},{"voice_chat","MIC"},{"slot1","RIFLE"},{"slot2","PISTOL"},
        {"slot3","KNIFE"},{"slot4","NADE"},{"slot5","BOMB"}};
    for (const auto &e:entries) if (!::strcmp(e.name,n)) return e.text;
    return "KEY";
}
inline const unsigned char *glyph(char c) {
    static const unsigned char font[][7]={
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
        {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
    return font[(c>='A' && c<='Z') ? c-'A' : 10];
}
inline bool render(const char *path, unsigned char *rgba, std::size_t bytes, int size=128) {
    if (!path || !rgba || size<32 || size>512 || bytes<std::size_t(size)*size*4) return false;
    const float center=(size-1)*.5f, radius=size*.475f;
    for (int y=0;y<size;++y) for (int x=0;x<size;++x) {
        const float d=std::sqrt((x-center)*(x-center)+(y-center)*(y-center));
        unsigned char *p=rgba+(y*size+x)*4;
        const bool edge=std::fabs(d-radius)<size*.012f;
        p[0]=p[1]=p[2]=edge ? 255 : 8;
        p[3]=d>radius+size*.012f ? 0 : edge ? 255 : 90;
    }
    const char *text=label(path);const int n=int(::strlen(text));
    const int scale=size/(n*6+8), left=(size-(n*6-1)*scale)/2, top=(size-7*scale)/2;
    for (int c=0;c<n;++c) for (int y=0;y<7;++y) for (int x=0;x<5;++x)
        if (glyph(text[c])[y] & (1 << (4-x)))
            for (int yy=0;yy<scale;++yy) for (int xx=0;xx<scale;++xx) {
                const int at=((top+y*scale+yy)*size+left+(c*6+x)*scale+xx)*4;
                rgba[at]=rgba[at+1]=rgba[at+2]=rgba[at+3]=255;
            }
    return true;
}
}
