#include "SourceMap.hpp"
#include "quakedef.h"
#include "bspfile.h"
#include "modelloader.h"
#include "filesystem_engine.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "vphysics_interface.h"
#include "gametrace.h"
#include "vtf/vtf.h"
#include "tier1/utlbuffer.h"
#include "tier0/dbg.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
constexpr size_t maximumFile = 128 * 1024 * 1024;
constexpr size_t maximumVertices = 300000;
bool report(const char* name, bool ok) {
    Msg("Source BSP self-test %s: %s\n",name,ok?"PASS":"FAIL");return ok;
}
struct MeshPoint { Vector position; float color[3]; float uv[2]; };
struct CollisionDelete {
    void operator()(CPhysCollide* p) const { if(p && g_pPhysicsCollision) g_pPhysicsCollision->DestroyCollide(p); }
};
using Collision = std::shared_ptr<CPhysCollide>;
// Only trusted, bounded sections reach the legacy Source loader. Validate raw
// disk ranges before it allocates buffers; unsupported compressed lumps fail.
bool headerValid(const dheader_t& h, size_t size) {
    if(h.ident!=IDBSPHEADER || h.version<MINBSPVERSION || h.version>BSPVERSION || size<sizeof(h) || size>maximumFile)return false;
    for(const auto& l:h.lumps) {
        if(l.fileofs<0 || l.filelen<0 || l.uncompressedSize!=0)return false;
        if(l.filelen && (size_t(l.fileofs)<sizeof(h) || size_t(l.fileofs)>size || size_t(l.filelen)>size-size_t(l.fileofs)))return false;
    }
    return h.lumps[LUMP_VERTEXES].filelen%sizeof(dvertex_t)==0
        && h.lumps[LUMP_EDGES].filelen%sizeof(dedge_t)==0
        && h.lumps[LUMP_SURFEDGES].filelen%sizeof(int)==0
        && h.lumps[LUMP_FACES].filelen%sizeof(dface_t)==0;
}
template<class T> std::vector<T> lump(int id) {
    CMapLoadHelper load(id);std::vector<T> out(load.LumpSize()/sizeof(T));
    if(!out.empty())std::memcpy(out.data(),load.LumpBase(),out.size()*sizeof(T));return out;
}
struct LoaderScope {
    LoaderScope(const char* path) { CMapLoadHelper::Init(nullptr,path); }
    ~LoaderScope(){ CMapLoadHelper::Shutdown(); }
};
void addBox(std::vector<dvertex_t>& vertices,std::vector<dedge_t>& edges,std::vector<int>& surfedges,std::vector<dface_t>& faces,const Vector& lo,const Vector& hi) {
    const Vector corners[]={ {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z} };
    const int quads[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
    const auto first=vertices.size();for(auto p:corners){dvertex_t v;v.point=p;vertices.push_back(v);}
    for(const auto& q:quads){dface_t f{};f.firstedge=surfedges.size();f.numedges=4;f.dispinfo=-1;f.texinfo=-1;f.lightofs=-1;
        for(unsigned i=0;i<4;++i){dedge_t e{{}};e.v[0]=first+q[i];e.v[1]=first+q[(i+1)%4];surfedges.push_back(edges.size());edges.push_back(e);}faces.push_back(f);}
}
std::vector<unsigned char> fixture() {
    std::vector<dvertex_t> vertices;std::vector<dedge_t> edges(1);std::vector<int> surfedges;std::vector<dface_t> faces;
    addBox(vertices,edges,surfedges,faces,Vector(-256,-256,-16),Vector(256,256,0));
    addBox(vertices,edges,surfedges,faces,Vector(256,-256,0),Vector(272,272,192));
    addBox(vertices,edges,surfedges,faces,Vector(-272,256,0),Vector(256,272,192));
    addBox(vertices,edges,surfedges,faces,Vector(-272,-272,0),Vector(-256,256,192));
    addBox(vertices,edges,surfedges,faces,Vector(-256,-272,0),Vector(256,-256,192));
    addBox(vertices,edges,surfedges,faces,Vector(16,-48,0),Vector(80,16,64));
    addBox(vertices,edges,surfedges,faces,Vector(-96,80,0),Vector(-32,144,96));
    dheader_t h{};h.ident=IDBSPHEADER;h.version=BSPVERSION;h.mapRevision=1;
    std::vector<unsigned char> bytes(sizeof(h));
    auto append=[&](int id,const void* data,size_t length){h.lumps[id].fileofs=bytes.size();h.lumps[id].filelen=length;auto p=static_cast<const unsigned char*>(data);bytes.insert(bytes.end(),p,p+length);};
    append(LUMP_VERTEXES,vertices.data(),vertices.size()*sizeof(dvertex_t));append(LUMP_EDGES,edges.data(),edges.size()*sizeof(dedge_t));
    append(LUMP_SURFEDGES,surfedges.data(),surfedges.size()*sizeof(int));append(LUMP_FACES,faces.data(),faces.size()*sizeof(dface_t));
    std::memcpy(bytes.data(),&h,sizeof(h));return bytes;
}
}
namespace source1ios {
struct SourceMap::Impl {
    std::vector<MeshPoint> mesh;Collision collision, fixtureCollision;
    std::string builtin;Vector camera;QAngle angles;SourceTexture texture;
};
SourceMap::SourceMap()=default;
SourceMap::~SourceMap(){stop();}
bool SourceMap::start(const std::filesystem::path& root) {
    if(impl_)return false;
    impl_=std::make_unique<Impl>();impl_->builtin=(root/"selftest"/"ios_geometry.bsp").string();
    // Serialize a genuine VTF, read it through Source filesystem, then decode
    // the texture for the native Metal adapter. No shaderapiempty drawing implied.
    using VTF=std::unique_ptr<IVTFTexture,decltype(&DestroyVTFTexture)>;
    VTF source(CreateVTFTexture(),DestroyVTFTexture), decoded(CreateVTFTexture(),DestroyVTFTexture);
    if(!source || !decoded || !source->Init(64,64,1,IMAGE_FORMAT_RGBA8888,0,1)){stop();return false;}
    for(int mip=0;mip<source->MipCount();++mip){int w=std::max(1,64>>mip),h=w;auto* pixels=source->ImageData(0,0,mip);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto* pixel=pixels+(y*w+x)*4;const bool alternate=((x*64/w)/8+(y*64/h)/8)%2;pixel[0]=pixel[1]=pixel[2]=alternate?210:120;pixel[3]=255;}}
    CUtlBuffer encoded;if(!source->Serialize(encoded)){stop();return false;}
    const auto texturePath=(root/"selftest"/"ios_checker.vtf").string();auto textureFile=g_pFullFileSystem->Open(texturePath.c_str(),"wb");
    bool textureOK=textureFile && g_pFullFileSystem->Write(encoded.Base(),encoded.TellPut(),textureFile)==encoded.TellPut();if(textureFile)g_pFullFileSystem->Close(textureFile);
    CUtlBuffer disk;if(!textureOK || !g_pFullFileSystem->ReadFile(texturePath.c_str(),nullptr,disk) || !decoded->Unserialize(disk)){stop();return false;}
    decoded->ConvertImageFormat(IMAGE_FORMAT_RGBA8888,false);impl_->texture.width=decoded->Width();impl_->texture.height=decoded->Height();
    impl_->texture.pixels.assign(decoded->ImageData(0,0,0),decoded->ImageData(0,0,0)+64*64*4);
    const auto bytes=fixture();auto file=g_pFullFileSystem->Open(impl_->builtin.c_str(),"wb");
    bool ok=file && g_pFullFileSystem->Write(bytes.data(),bytes.size(),file)==int(bytes.size());if(file)g_pFullFileSystem->Close(file);
    if(!ok || !load(impl_->builtin.c_str(),nullptr)){stop();return false;}
    impl_->fixtureCollision=impl_->collision;
    if(!selfTest()){stop();return false;}
    Msg("Source BSP preview ready: original lump loader + polygon collision + Metal adapter. Full engine world/material rendering remains pending.\n");return true;
}
void SourceMap::stop(){impl_.reset();}
bool SourceMap::load(const char* filename,const char* pathID) {
    if(!impl_ || !filename || std::strlen(filename)>=MAX_PATH || CMapLoadHelper::GetRefCount()!=0)return false;
    auto file=g_pFullFileSystem->Open(filename,"rb",pathID);if(!file){Warning("Source BSP: file not found: %s\n",filename);return false;}
    const auto size=g_pFullFileSystem->Size(file);dheader_t h{};
    const bool read=g_pFullFileSystem->Read(&h,sizeof(h),file)==sizeof(h);g_pFullFileSystem->Close(file);
    if(!read || !headerValid(h,size)){Warning("Source BSP: unsupported or invalid header/lump ranges\n");return false;}
    // External .lmp overlays bypass the validated on-disk header. Reject them.
    char overlay[MAX_PATH];V_StripExtension(filename,overlay,sizeof(overlay));V_strncat(overlay,"_l_0.lmp",sizeof(overlay));
    if(g_pFullFileSystem->FileExists(overlay,pathID)){Warning("Source BSP: external lump overlays unsupported\n");return false;}
    std::vector<MeshPoint> mesh;
    {
        LoaderScope scope(filename);
        auto points=lump<dvertex_t>(LUMP_VERTEXES);auto edges=lump<dedge_t>(LUMP_EDGES);
        auto surfedges=lump<int>(LUMP_SURFEDGES);auto faces=lump<dface_t>(LUMP_FACES);
        for(const auto& p:points)if(!p.point.IsValid() || std::abs(p.point.x)>32768 || std::abs(p.point.y)>32768 || std::abs(p.point.z)>32768)return false;
        for(size_t face=0;face<faces.size();++face){const auto& f=faces[face];
            if(f.numedges<3 || f.numedges>256 || f.firstedge<0 || size_t(f.firstedge)>surfedges.size() || size_t(f.numedges)>surfedges.size()-size_t(f.firstedge))return false;
            if(f.dispinfo>=0)continue; // Displacement topology needs its own reader.
            std::vector<Vector> polygon;
            for(int i=0;i<f.numedges;++i){const int se=surfedges[f.firstedge+i];if(se==std::numeric_limits<int>::min())return false;
                const size_t e=se<0?size_t(-se):size_t(se);if(e>=edges.size())return false;
                const size_t v=edges[e].v[se<0?1:0];if(v>=points.size())return false;polygon.push_back(points[v].point);}
            for(size_t i=1;i+1<polygon.size();++i){if(mesh.size()+3>maximumVertices)return false;
                Vector normal;CrossProduct(polygon[i]-polygon[0],polygon[i+1]-polygon[0],normal);if(normal.LengthSqr()<1e-8f)continue;VectorNormalize(normal);
                const float shade=.35f+.65f*std::abs(normal.z*.8f+normal.x*.3f+normal.y*.2f);
                const float palette[6][3]={{.3f,.7f,.9f},{.7f,.8f,.9f},{.9f,.5f,.2f},{.4f,.8f,.5f},{.65f,.45f,.85f},{.85f,.75f,.35f}};
                for(auto p:{polygon[0],polygon[i],polygon[i+1]}){
                    const float tx=std::abs(normal.z)>.5f?p.x:(std::abs(normal.x)>.5f?p.y:p.x);
                    const float ty=std::abs(normal.z)>.5f?p.y:p.z;
                    mesh.push_back({p,{palette[face%6][0]*shade,palette[face%6][1]*shade,palette[face%6][2]*shade},{tx/64,ty/64}});
                }
            }
        }
    }
    if(mesh.empty())return false;
    auto* soup=g_pPhysicsCollision->PolysoupCreate();if(!soup)return false;
    for(size_t i=0;i<mesh.size();i+=3)g_pPhysicsCollision->PolysoupAddTriangle(soup,mesh[i].position,mesh[i+1].position,mesh[i+2].position,0);
    Collision collision(g_pPhysicsCollision->ConvertPolysoupToCollide(soup,false),CollisionDelete{});g_pPhysicsCollision->PolysoupDestroy(soup);if(!collision)return false;
    impl_->mesh=std::move(mesh);impl_->collision=std::move(collision);resetCamera();
    Msg("Source BSP polygons loaded: %zu triangles from %s\n",impl_->mesh.size()/3,filename);return true;
}
void SourceMap::resetCamera(){if(impl_){impl_->camera=Vector(-190,-160,80);impl_->angles=QAngle(8,35,0);}}
void SourceMap::look(float yaw,float pitch){if(impl_ && std::isfinite(yaw)&&std::isfinite(pitch)){impl_->angles.y=std::remainder(impl_->angles.y+yaw,360.f);impl_->angles.x=std::max(-85.f,std::min(85.f,impl_->angles.x+pitch));}}
void SourceMap::move(float forward,float right,float seconds){
    if(!impl_ || !std::isfinite(forward)||!std::isfinite(right)||!std::isfinite(seconds)||seconds<=0)return;
    Vector f,r;AngleVectors(QAngle(0,impl_->angles.y,0),&f,&r,nullptr);Vector delta=f*forward+r*right;
    if(delta.LengthSqr()>1)VectorNormalize(delta);delta*=160*std::min(seconds,.1f);
    // Swept hull against the actual IVP polygon collision avoids wall tunneling.
    trace_t trace{};g_pPhysicsCollision->TraceBox(impl_->camera,impl_->camera+delta,Vector(-8,-8,-24),Vector(8,8,8),impl_->collision.get(),Vector(0,0,0),QAngle(0,0,0),&trace);
    if(!trace.startsolid)impl_->camera+=delta*std::max(0.f,trace.fraction-.001f);
}
std::vector<SourceVertex> SourceMap::vertices(float aspect) const {
    std::vector<SourceVertex> out;if(!impl_)return out;out.reserve(impl_->mesh.size());Vector f,r,u;AngleVectors(impl_->angles,&f,&r,&u);
    const float a=std::max(aspect,.01f),scale=1.3f,near=1,far=8192;
    for(const auto& v:impl_->mesh){Vector relative=v.position-impl_->camera;float depth=DotProduct(relative,f);
        out.push_back({{DotProduct(relative,r)*scale/a,DotProduct(relative,u)*scale,depth*far/(far-near)-near*far/(far-near),depth},{v.color[0],v.color[1],v.color[2],1},{v.uv[0],v.uv[1]}});}return out;
}
const SourceTexture& SourceMap::texture() const { static const SourceTexture empty; return impl_?impl_->texture:empty; }
bool SourceMap::selfTest(){
    if(!impl_)return false;bool all=report("original lump geometry",!impl_->mesh.empty()&&CMapLoadHelper::GetRefCount()==0);
    dheader_t bad{};bad.ident=IDBSPHEADER;bad.version=BSPVERSION;bad.lumps[LUMP_VERTEXES].fileofs=sizeof(bad);bad.lumps[LUMP_VERTEXES].filelen=12;
    all&=report("truncated lump range rejected",!headerValid(bad,sizeof(bad)));
    trace_t trace{};g_pPhysicsCollision->TraceBox(Vector(-190,-160,80),Vector(-190,-160,-80),Vector(0,0,0),Vector(0,0,0),impl_->fixtureCollision.get(),Vector(0,0,0),QAngle(0,0,0),&trace);
    all&=report("IVP polygon floor trace",trace.fraction>0 && trace.fraction<1 && !trace.startsolid);
    g_pPhysicsCollision->TraceBox(Vector(-190,-160,80),Vector(400,-160,80),Vector(-8,-8,-24),Vector(8,8,8),impl_->fixtureCollision.get(),Vector(0,0,0),QAngle(0,0,0),&trace);
    all&=report("IVP swept camera wall",trace.fraction>0 && trace.fraction<.8f && !trace.startsolid);
    all&=report("VTF preview texture decoded",impl_->texture.width==64 && impl_->texture.height==64 && impl_->texture.pixels.size()==64*64*4);
    auto before=vertices(1);const auto saved=impl_->angles;look(10,0);auto after=vertices(1);impl_->angles=saved;
    all&=report("Source camera projection",before.size()==after.size()&&!before.empty()&&before[0].position[0]!=after[0].position[0]);return all;
}
}
