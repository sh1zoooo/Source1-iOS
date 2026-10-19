#!/usr/bin/env python3
"""Adapt old client UI/viewmodel presentation to the actual 2.0 content layout."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
def replace(path,old,new,count=1):
 p=root/path;s=p.read_text()
 if s.count(old)!=count:raise RuntimeError(f'iOS content compatibility context changed: {path}: {old[:80]!r}')
 p.write_text(s.replace(old,new))

# Engine modules never link UIKit or each other for startup notifications.
for path in ('engine/traceinit.cpp','gameui/BasePanel.cpp','appframework/sdlmgr.cpp','gameui/GameUI_Interface.cpp'):
 p=root/path
 if not p.exists() and path.startswith('appframework/'):
  # Keep the filename explicit and checked against the pinned upstream.
  raise RuntimeError('Missing SDL present adapter')
 p.write_text('#include "IOSStartupProgress.hpp"\n'+p.read_text())
replace('engine/traceinit.cpp','\tWarning("iOS host init: %s\\n", i);',
 '''\tWarning("iOS host init: %s\\n", i);
    // Human-readable stages; internal function names remain in the log.
    IOSStartupProgress("Загрузка ресурсов и игровых модулей…");''')
replace('gameui/GameUI_Interface.cpp','void CGameUI::Initialize( CreateInterfaceFn factory )\n{',
        'void CGameUI::Initialize( CreateInterfaceFn factory )\n{\n    IOSStartupProgress("Загрузка интерфейса и текстур…");')
replace('gameui/BasePanel.cpp','\tUpdateBackgroundState();',
 '\tUpdateBackgroundState();\n#ifdef SOURCE_IOS\n    // Standalone iOS has no Steam platform modules. The actual visible game\n    // menu, rather than the desktop module-loader mutex, defines readiness.\n    if (m_bEverActivated && IsVisible() && m_pGameMenu && m_pGameMenu->GetItemCount()>0)\n        IOSStartupProgress(NULL,IOSStartupMenuReady);\n#endif',count=1)
replace('appframework/sdlmgr.cpp','\tGLenum swapError = gGL->glGetError();',
 '''\tGLenum swapError = gGL->glGetError();
    if (readStatus == GL_FRAMEBUFFER_COMPLETE && drawStatus == GL_FRAMEBUFFER_COMPLETE &&
        width > 0 && height > 0 && blitError == GL_NO_ERROR && swapError == GL_NO_ERROR)
        IOSStartupProgress(NULL, IOSStartupPresented);''')

# The old settings resource isn't included in 2.0. Construct the small set of
# supported controls ourselves rather than loading an unrelated new resource.
for path in ('gameui/ModMenu/ClientModMenuWindow.h','gameui/ModMenu/ClientModMainMenu.h'):
 replace(path,'\npublic:\n','\npublic:\n#ifdef SOURCE_IOS\n    virtual void PerformLayout();\n#endif\n')
window='gameui/ModMenu/ClientModMenuWindow.cpp'
replace(window,'#include "BasePanel.h"','#include "BasePanel.h"\n#include "IOSClientModLayout.hpp"')
replace(window,'\tSetTitle("#GameUI_ClientModMenuTitle", true);',
 '''#ifdef SOURCE_IOS
    SetTitle("ClientMod settings", true);
    SetTitleBarVisible(false);
    int sw=0, sh=0; surface()->GetScreenSize(sw,sh);
    const IOSClientModLayout l=IOSClientModLayout::fit(sw,sh);
    SetBounds(l.x,l.y,l.width,l.height);
    InvalidateLayout(true);
#else
\tSetTitle("#GameUI_ClientModMenuTitle", true);
#endif''')
replace(window,'\tGetPropertySheet()->SetTabWidth(84);',
 '''\tGetPropertySheet()->SetTabWidth(84);
#ifdef SOURCE_IOS
    new Label(this, "IOSHeading", "ClientMod settings");
#endif''')
p=root/window;p.write_text(p.read_text()+r'''
#ifdef SOURCE_IOS
void ClientModMenuWindow::PerformLayout() {
    BaseClass::PerformLayout();
    int sw=0, sh=0; surface()->GetScreenSize(sw,sh);
    const IOSClientModLayout l=IOSClientModLayout::fit(sw,sh);
    static HFont font=0; static int fontSize=0;
    if (!font) font=surface()->CreateFont();
    if (fontSize != l.font) {
        surface()->SetFontGlyphSet(font,"Arial",l.font,500,0,0,ISurface::FONTFLAG_ANTIALIAS);
        fontSize=l.font;
    }
    const int w=GetWide(), h=GetTall(), m=l.margin, row=l.row;
    Label *heading=dynamic_cast<Label*>(FindChildByName("IOSHeading"));
    if (heading) { heading->SetBounds(m,m,w-2*m,row/2); heading->SetFont(font); }
    PropertySheet *sheet=GetPropertySheet();
    sheet->SetBounds(m,m+row/2,w-2*m,h-row*3/2-3*m);
    sheet->InvalidateLayout(true);
    Panel *tab=sheet->GetActiveTab();
    if (tab) tab->SetVisible(false); // One supported page, no redundant tiny tab.
    Panel *page=sheet->GetActivePage();
    if (page) { page->SetBounds(0,0,sheet->GetWide(),sheet->GetTall()); page->InvalidateLayout(true); }
    const char *names[]={"OKButton","CancelButton","ApplyButton"};
    const char *texts[]={"OK","Cancel","Apply"};
    const int bw=(w-4*m)/3;
    for (int i=0;i<3;++i) {
        Button *b=dynamic_cast<Button*>(FindChildByName(names[i]));
        if (b) { b->SetText(texts[i]); b->SetFont(font);
            b->SetBounds(m+i*(bw+m),h-row-m,bw,row); }
    }
}
#endif
''')
page='gameui/ModMenu/ClientModMainMenu.cpp'
replace(page,'#include "ClientModMainMenu.h"','#include "ClientModMainMenu.h"\n#include "IOSClientModLayout.hpp"')
replace(page,'\tLoadControlSettings( "Resource/ClientModMainMenu.res" );',
 '''#ifdef SOURCE_IOS
    cancel->SetVisible(false); ok->SetVisible(false); apply->SetVisible(false);
    const char *ids[]={"IOSOffsetX","IOSOffsetY","IOSOffsetZ","IOSRecoil","IOSHand"};
    const char *texts[]={"Horizontal offset","Forward offset","Vertical offset","Weapon recoil","Weapon hand"};
    for (int i=0;i<5;++i) new Label(this,ids[i],texts[i]);
    m_pNewBobbing->SetText("Weapon movement");
#else
\tLoadControlSettings( "Resource/ClientModMainMenu.res" );
#endif''')
# Localized handedness tokens are absent in the new pack.
replace(page,'m_pWeaponPos->AddItem( "#GameUI_Viewmodel_HandLeft", "cl_righthand 0" );',
 '''m_pWeaponPos->AddItem(
#ifdef SOURCE_IOS
        "Left",
#else
        "#GameUI_Viewmodel_HandLeft",
#endif
        "cl_righthand 0" );''')
replace(page,'m_pWeaponPos->AddItem( "#GameUI_Viewmodel_HandRight", "cl_righthand 1" );',
 '''m_pWeaponPos->AddItem(
#ifdef SOURCE_IOS
        "Right",
#else
        "#GameUI_Viewmodel_HandRight",
#endif
        "cl_righthand 1" );''')
p=root/page;p.write_text(p.read_text()+r'''
#ifdef SOURCE_IOS
void ClientModMainMenu::PerformLayout() {
    BaseClass::PerformLayout();
    int sw=0,sh=0; surface()->GetScreenSize(sw,sh);
    const IOSClientModLayout l=IOSClientModLayout::fit(sw,sh);
    static HFont font=0; static int fontSize=0;
    if (!font) font=surface()->CreateFont();
    if (fontSize != l.font) {
        surface()->SetFontGlyphSet(font,"Arial",l.font,500,0,0,ISurface::FONTFLAG_ANTIALIAS);
        fontSize=l.font;
    }
    const int w=GetWide(), h=GetTall(), m=l.margin, row=h/6;
    const char *ids[]={"IOSOffsetX","IOSOffsetY","IOSOffsetZ","IOSRecoil","IOSHand"};
    CCvarSlider *sliders[]={m_pViewmodelOffsetX,m_pViewmodelOffsetY,m_pViewmodelOffsetZ,m_pViewmodelRecoil};
    Label *values[]={m_pViewmodelOffsetXLabel,m_pViewmodelOffsetYLabel,m_pViewmodelOffsetZLabel,m_pViewmodelRecoilLabel};
    for (int i=0;i<5;++i) {
        Label *label=dynamic_cast<Label*>(FindChildByName(ids[i]));
        if (label) { label->SetBounds(m,i*row,w*40/100-m,row); label->SetFont(font); }
        if (i<4) {
            sliders[i]->SetBounds(w*40/100,i*row,w*45/100,row);
            sliders[i]->SetThumbWidth(row/2);
            values[i]->SetBounds(w*87/100,i*row,w*13/100-m,row);
            values[i]->SetFont(font);
        }
    }
    m_pWeaponPos->SetBounds(w*40/100,4*row,w*50/100,row-m/2);
    m_pWeaponPos->SetFont(font);
    m_pNewBobbing->SetBounds(m,5*row,w-2*m,row);
    m_pNewBobbing->SetFont(font);
}
#endif
''')
print('Prepared iOS startup presentation and resource-independent ClientMod settings')

# In 2.0 the weapon mesh no longer contains arms. Merge the separate model on
# its matching skeleton, and draw only with its owner in the viewmodel pass.
replace('game/shared/baseviewmodel_shared.h','\tvirtual int\t\t\t\tInternalDrawModel( int flags );',
 '''\tvirtual int\t\t\t\tInternalDrawModel( int flags );
#ifdef SOURCE_IOS
    void DrawIOSHands(int flags);
    CHandle<C_BaseAnimating> m_hIOSHands;
    const model_t *m_pIOSHandsWeapon;
#endif''')
replace('game/shared/baseviewmodel_shared.cpp','\tm_nOldAnimationParity = 0;',
 '''\tm_nOldAnimationParity = 0;
#ifdef SOURCE_IOS
    m_pIOSHandsWeapon = NULL;
#endif''')
replace('game/shared/baseviewmodel_shared.cpp','CBaseViewModel::~CBaseViewModel()\n{',
 '''CBaseViewModel::~CBaseViewModel()
{
#if defined(CLIENT_DLL) && defined(SOURCE_IOS)
    if (m_hIOSHands.Get()) m_hIOSHands->Release();
#endif''')
replace('game/shared/baseviewmodel_shared.cpp','void CBaseViewModel::UpdateOnRemove( void )\n{',
 '''void CBaseViewModel::UpdateOnRemove( void )
{
#if defined(CLIENT_DLL) && defined(SOURCE_IOS)
    if (m_hIOSHands.Get()) m_hIOSHands->Release();
    m_hIOSHands = NULL;
#endif''')
replace('game/client/c_baseviewmodel.cpp','#include \"c_baseviewmodel.h\"','#include \"c_baseviewmodel.h\"\n#include \"filesystem.h\"\n#include \"bone_setup.h\"')
replace('game/client/c_baseviewmodel.cpp','\t// Now that we\'ve rendered, reset the animation restart flag',
 '''#ifdef SOURCE_IOS
    if (ret && (flags & STUDIO_RENDER)) DrawIOSHands(flags);
#endif
\t// Now that we've rendered, reset the animation restart flag''')
p=root/'game/client/c_baseviewmodel.cpp';p.write_text(p.read_text()+r'''
#ifdef SOURCE_IOS
static ConVar ios_viewmodel_hands("ios_viewmodel_hands","1",FCVAR_ARCHIVE,
    "Draw separate ClientMod 2.0 arms for weapon-only viewmodels.");

class C_IOSViewmodelHands : public C_BaseAnimating {
public:
    virtual bool IsViewModel() const { return true; }
    // Never enter a world/viewmodel render list: the owning weapon draws us.
    virtual bool ShouldDraw() { return false; }
    virtual ShadowType_t ShadowCastType() { return SHADOWS_NONE; }
    virtual bool ShouldReceiveProjectedTextures(int) { return false; }
    int DrawWithOwner(int flags) {
        CreateModelInstance();
        // Do not use C_BaseAnimating::DrawModel's followed-entity path, which
        // re-enters the parent DrawModel. Bones are merged by SetupBones itself.
        return C_BaseAnimating::InternalDrawModel(flags);
    }
};

void C_BaseViewModel::DrawIOSHands(int flags) {
    if (!ios_viewmodel_hands.GetBool() || ViewModelIndex()!=0 || !GetModel()) return;
    if (m_pIOSHandsWeapon != GetModel()) {
        if (m_hIOSHands.Get()) m_hIOSHands->Release();
        m_hIOSHands = NULL;
        m_pIOSHandsWeapon = GetModel();
        MDLCACHE_CRITICAL_SECTION();
        CStudioHdr *weapon = GetModelPtr();
        // Older content/custom weapons can already have their own arms.
        if (!weapon || Studio_BoneIndexByName(weapon,"v_weapon.Bip01_L_Hand")<0 ||
            Studio_BoneIndexByName(weapon,"v_weapon.Bip01_R_Hand")<0) return;
        const studiohdr_t *raw=weapon->GetRenderHdr();
        for (int i=0;i<raw->numtextures;++i) {
            const char *name=raw->pTexture(i)->pszName();
            if (Q_stristr(name,"hand") || Q_stristr(name,"arm") ||
                Q_stristr(name,"glove") || Q_stristr(name,"sleeve")) return;
        }
        const char *path="models/weapons/arms/custom/gloves/v_bare_hands.mdl";
        if (!filesystem->FileExists(path,"GAME")) return;
        const model_t *handsModel=modelinfo->FindOrLoadModel(path);
        const studiohdr_t *handsHeader=handsModel ? modelinfo->GetStudiomodel(handsModel) : NULL;
        if (!handsHeader || handsHeader->numbones<1 || handsHeader->numbodyparts<1) {
            Warning("iOS separate hands: model unavailable: %s\n",path); return;
        }
        C_IOSViewmodelHands *hands=new C_IOSViewmodelHands;
        if (!hands->InitializeAsClientEntity(NULL,RENDER_GROUP_VIEW_MODEL_OPAQUE)) {
            hands->Release(); return;
        }
        hands->SetModelPointer(handsModel);
        hands->FollowEntity(this,true);
        hands->SetOwnerEntity(GetOwner());
        hands->RemoveFromLeafSystem();
        m_hIOSHands=hands;
        Warning("iOS separate hands: attached %s to %s\n",path,modelinfo->GetModelName(GetModel()));
    }
    C_IOSViewmodelHands *hands=static_cast<C_IOSViewmodelHands*>(m_hIOSHands.Get());
    if (!hands) return;
    hands->InvalidateBoneCache();
    CMatRenderContextPtr context(materials);
    if (ShouldFlipViewModel()) context->CullMode(MATERIAL_CULLMODE_CW);
    hands->DrawWithOwner(flags);
    context->CullMode(MATERIAL_CULLMODE_CCW);
}
#endif
''')
print('Prepared separate ClientMod arms with owner lifetime and viewmodel-only rendering')

replace('game/client/touch.cpp','#include "cbase.h"','#include "cbase.h"\n#include "IOSTouchFallback.hpp"')
# Missing assets get a real RGBA VTF and join the same existing atlas. Existing
# materials retain their old path, including compressed ASTC and custom VMTs.
replace('game/client/touch.cpp','''\t\tif( !fp )
\t\t{
\t\t\tt->textureID = vgui::surface()->CreateNewTextureID();
\t\t\tvgui::surface()->DrawSetTextureFile( t->textureID, t->szName, true, false );
\t\t\tcontinue;
\t\t}
''','''\t\tif( !fp )
\t\t{
#ifdef SOURCE_IOS
            char material[MAX_PATH];
            Q_snprintf(material,sizeof(material),"materials/%s.vmt",t->szName);
            if (!filesystem->FileExists(material,"GAME")) {
                t->vtf=CreateVTFTexture();
                if (!t->vtf->Init(128,128,1,IMAGE_FORMAT_RGBA8888,TEXTUREFLAGS_NOMIP,1))
                    Error("Unable to allocate iOS touch button");
                IOSTouchFallback::render(t->szName,t->vtf->ImageData(0,0,0),128*128*4);
                t->width=t->height=128;
                t->textureID=0; t->isInAtlas=true;
                atlasSize+=128*128;
                rects[rectCount].h=128; rects[rectCount].w=128; ++rectCount;
                Warning("iOS touch: built-in action button for missing %s\\n",t->szName);
                continue;
            }
#endif
\t\t\tt->textureID = vgui::surface()->CreateNewTextureID();
\t\t\tvgui::surface()->DrawSetTextureFile( t->textureID, t->szName, true, false );
\t\t\tcontinue;
\t\t}
''')
# Rect packing must grow until every rectangle fits; total area alone doesn't
# guarantee a fit (e.g. 17 equal controls need an 8x8, not a 4x4 atlas).
replace('game/client/touch.cpp','\tstbrp_pack_rects(&context, rects, rectCount);',
 '''\tstbrp_pack_rects(&context, rects, rectCount);
#ifdef SOURCE_IOS
    for (;;) {
        bool packed=true;
        for (int i=0;i<rectCount;++i) if (!rects[i].was_packed) packed=false;
        if (packed) break;
        if (atlasHeight>=8192) Error("Touch atlas exceeds the supported size");
        free(nodes); delete[] dest;
        atlasHeight*=2; nodesCount=atlasHeight*2;
        nodes=(stbrp_node*)malloc(nodesCount*sizeof(stbrp_node));
        sizeInBytes=atlasHeight*atlasHeight*4;
        dest=new unsigned char[sizeInBytes]; memset(dest,0,sizeInBytes);
        stbrp_init_target(&context,atlasHeight,atlasHeight,nodes,nodesCount);
        stbrp_pack_rects(&context,rects,rectCount);
    }
#endif''')
replace('game/client/touch.cpp','memset(rects, 0, sizeof(stbrp_node)*textureList.Count());',
        'memset(rects, 0, sizeof(stbrp_rect)*textureList.Count());')
print('Prepared missing touch assets as batched built-in action buttons')
