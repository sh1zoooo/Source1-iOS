#pragma once
#include "RenderTypes.hpp"
#include <string>
#include <vector>
namespace source1ios {
struct MobileButton { std::string name,command,icon;unsigned flags=0;float x1=0,y1=0,x2=0,y2=0;unsigned char color[4]{255,255,255,255};SourceTexture texture; };
struct BuyItem { std::string alias,label,category;int price=0; };
struct HudElement {std::string name,x,y;float width=0,height=0;};
struct MobileResources { float yaw=100,pitch=100,forwardZone=.09f,sideZone=.09f;std::vector<MobileButton> buttons;std::vector<BuyItem> buy;std::string config;std::vector<HudElement> hud; };
MobileResources loadMobileResources(int team);
}
