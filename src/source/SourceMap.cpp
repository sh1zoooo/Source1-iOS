#include "SourceMap.hpp"
#include "BspLzmaFixture.hpp"
#include "SourceStudio.hpp"
#include "studio.h"
#include "optimize.h"
#include "quakedef.h"
#include "bspfile.h"
#include "builddisp.h"
#include "cmodel.h"
#include "dispcoll_preview.h"
#include "modelloader.h"
#include "filesystem_engine.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "vphysics_interface.h"
#include "vphysics/constraints.h"
#include "gametrace.h"
#include "cmodel_engine.h"
#include "gl_model_private.h"
#include "vtf/vtf.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzmaDecoder.h"
#include "tier0/dbg.h"
#include "datacache/imdlcache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
constexpr int idStudioHeader=(('T'<<24)+('S'<<16)+('D'<<8)+'I');
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
struct PhysicsScene {
    IPhysics* physics=nullptr;IPhysicsEnvironment* environment=nullptr;
    IPhysicsObject* level=nullptr;IPhysicsObject* anchor=nullptr;IPhysicsObject* body=nullptr;
    IPhysicsConstraint* joint=nullptr;Collision levelShape;
    ~PhysicsScene(){
        if(environment){if(joint)environment->DestroyConstraint(joint);if(body)environment->DestroyObject(body);
            if(anchor)environment->DestroyObject(anchor);if(level)environment->DestroyObject(level);physics->DestroyEnvironment(environment);}
    }
};
std::unique_ptr<PhysicsScene> physicsScene(Collision shape){
    auto* surfaces=static_cast<IPhysicsSurfaceProps*>(Sys_GetFactoryThis()(VPHYSICS_SURFACEPROPS_INTERFACE_VERSION,nullptr));
    if(!surfaces)return nullptr;
    if(surfaces->GetSurfaceIndex("default")<0)
        surfaces->ParseSurfaceData("source1ios_surface_defaults", "\"default\" { \"density\" \"1000\" \"elasticity\" \"0.2\" \"friction\" \"0.8\" }");
    const int material=surfaces->GetSurfaceIndex("default");if(material<0)return nullptr;
    auto scene=std::make_unique<PhysicsScene>();scene->levelShape=std::move(shape);
    scene->physics=static_cast<IPhysics*>(Sys_GetFactoryThis()(VPHYSICS_INTERFACE_VERSION,nullptr));
    scene->environment=scene->physics?scene->physics->CreateEnvironment():nullptr;if(!scene->environment)return nullptr;
    scene->environment->SetGravity(Vector(0,0,-600));
    objectparams_t params{nullptr,5,1,0,0,.05f,"Source iOS scene",nullptr,0,1,true};
    scene->level=scene->environment->CreatePolyObjectStatic(scene->levelShape.get(),material,Vector(0,0,0),QAngle(0,0,0),&params);
    if(!scene->level)return nullptr;
    scene->anchor=scene->environment->CreateSphereObject(8,material,Vector(-32,64,144),QAngle(0,0,0),&params,true);
    scene->body=scene->environment->CreateSphereObject(8,material,Vector(-32,64,96),QAngle(0,0,0),&params,false);
    if(!scene->anchor || !scene->body)return nullptr;
    constraint_ragdollparams_t joint;joint.Defaults();
    MatrixSetColumn(Vector(0,0,-24),3,joint.constraintToReference);MatrixSetColumn(Vector(0,0,24),3,joint.constraintToAttached);
    for(auto& axis:joint.axes)axis.SetAxisFriction(-45,45,0);
    scene->joint=scene->environment->CreateRagdollConstraint(scene->anchor,scene->body,nullptr,joint);if(!scene->joint)return nullptr;
    scene->body->Wake();scene->body->ApplyForceCenter(Vector(3500,0,0));return scene;
}
constexpr size_t maximumLump = 16 * 1024 * 1024;
constexpr int geometryLumps[]={LUMP_VERTEXES,LUMP_EDGES,LUMP_SURFEDGES,LUMP_FACES,LUMP_DISPINFO,LUMP_DISP_VERTS,LUMP_DISP_TRIS};
constexpr size_t geometryStrides[]={sizeof(dvertex_t),sizeof(dedge_t),sizeof(int),sizeof(dface_t),sizeof(ddispinfo_t),sizeof(CDispVert),sizeof(CDispTri)};
constexpr size_t geometryLumpCount=sizeof(geometryLumps)/sizeof(*geometryLumps);
bool headerValid(const dheader_t& h, size_t size, const char* file=nullptr) {
    auto fail=[&](const char* reason,int id=-1){if(file)Warning("Source BSP rejected %s: %s; version=%d, lump=%d, file bytes=%zu\n",file,reason,h.version,id,size);return false;};
    if(size<sizeof(h))return fail("truncated BSP header");
    if(size>maximumFile)return fail("file exceeds 128 MiB limit");
    if(h.ident!=IDBSPHEADER)return fail("expected VBSP signature (not an archive or GoldSrc BSP)");
    if(h.version<MINBSPVERSION || h.version>BSPVERSION)return fail("unsupported BSP version");
    for(int id=0;id<HEADER_LUMPS;++id){const auto& l=h.lumps[id];
        if(l.fileofs<0 || l.filelen<0 || (l.filelen && (size_t(l.fileofs)<sizeof(h) || size_t(l.fileofs)>size || size_t(l.filelen)>size-size_t(l.fileofs))))return fail("lump range outside file",id);
    }
    for(size_t i=0;i<geometryLumpCount;++i){const auto& l=h.lumps[geometryLumps[i]];
        const size_t decoded=l.uncompressedSize?l.uncompressedSize:l.filelen;
        if(decoded>maximumLump || size_t(l.filelen)>maximumLump)return fail("geometry lump exceeds 16 MiB limit",geometryLumps[i]);
        if(decoded%geometryStrides[i])return fail("geometry record size mismatch",geometryLumps[i]);
        const bool supportedVersion=l.version==0 || (geometryLumps[i]==LUMP_FACES && l.version==LUMP_FACES_VERSION);
        if(!supportedVersion)return fail("unsupported geometry lump version",geometryLumps[i]);
    }
    return true;
}
// Use the original bounded streaming decoder, not legacy Uncompress(), which
// has no input-length argument and is unsafe for arbitrary imported maps.
bool decodeLump(const std::vector<unsigned char>& raw,size_t expected,std::vector<unsigned char>& out,const char*& reason){
    auto fail=[&](const char* why){reason=why;return false;};
    if(raw.size()<sizeof(lzma_header_t))return fail("truncated LZMA header");
    lzma_header_t h;std::memcpy(&h,raw.data(),sizeof(h));
    if(h.id!=LZMA_ID)return fail("compressed lump lacks LZMA signature");
    if(!expected || expected>maximumLump || h.actualSize!=expected)return fail("LZMA decoded size mismatch or limit");
    if(h.lzmaSize!=raw.size()-sizeof(h))return fail("LZMA payload size mismatch");
    uint32 dictionary=0;std::memcpy(&dictionary,h.properties+1,sizeof(dictionary));
    if(h.properties[0]>=225 || dictionary>maximumLump)return fail("invalid LZMA properties or dictionary exceeds 16 MiB");
    out.resize(expected);CLZMAStream stream;unsigned consumed=0,written=0,remaining=0;
    if(!stream.Read(const_cast<unsigned char*>(raw.data()),raw.size(),out.data(),out.size(),consumed,written)
        || written!=expected || !stream.GetExpectedBytesRemaining(remaining) || remaining!=0 || consumed!=raw.size()){
        out.clear();return fail("LZMA stream incomplete or damaged");
    }
    return true;
}
template<class T> std::vector<T> lump(int id,const std::vector<unsigned char>& decoded) {
    if(!decoded.empty()){std::vector<T> out(decoded.size()/sizeof(T));std::memcpy(out.data(),decoded.data(),decoded.size());return out;}
    CMapLoadHelper load(id);std::vector<T> out(load.LumpSize()/sizeof(T));
    if(!out.empty())std::memcpy(out.data(),load.LumpBase(),out.size()*sizeof(T));return out;
}
struct LoaderScope {
    LoaderScope(const char* path) { CMapLoadHelper::Init(nullptr,path); }
    ~LoaderScope(){ CMapLoadHelper::Shutdown(); }
};
using Triangle=std::array<Vector,3>;
bool boundedPoint(const Vector& p){return p.IsValid() && std::abs(p.x)<=32768 && std::abs(p.y)<=32768 && std::abs(p.z)<=32768;}
bool displacementTriangles(const std::vector<Vector>& polygon,size_t face,const ddispinfo_t& d,
    const std::vector<CDispVert>& verts,const std::vector<CDispTri>& tris,std::vector<Triangle>& out,
    std::unique_ptr<PortCDispCollTree>* collisionTree=nullptr){
    if(polygon.size()!=4 || d.m_iMapFace!=face || d.power<MIN_MAP_DISP_POWER || d.power>MAX_MAP_DISP_POWER
        || !boundedPoint(d.startPosition) || !std::isfinite(d.smoothingAngle) || d.smoothingAngle<0 || d.smoothingAngle>180)return false;
    const size_t nv=d.NumVerts(),nt=d.NumTris();
    if(d.m_iDispVertStart<0 || size_t(d.m_iDispVertStart)>verts.size() || nv>verts.size()-d.m_iDispVertStart
        || d.m_iDispTriStart<0 || size_t(d.m_iDispTriStart)>tris.size() || nt>tris.size()-d.m_iDispTriStart)return false;
    bool startMatches=false;for(const auto& p:polygon){if(!boundedPoint(p))return false;startMatches|=(p-d.startPosition).LengthSqr()<.01f;}
    if(!startMatches)return false;
    Vector normal;CrossProduct(polygon[1]-polygon[0],polygon[2]-polygon[0],normal);
    if(normal.LengthSqr()<1e-4f)return false;VectorNormalize(normal);
    for(int i=0;i<4;++i){Vector cross;CrossProduct(polygon[(i+1)%4]-polygon[i],polygon[(i+2)%4]-polygon[(i+1)%4],cross);
        if(DotProduct(cross,normal)<=1e-4f || std::abs(DotProduct(polygon[i]-polygon[0],normal))>.1f)return false;}
    for(size_t i=0;i<nv;++i){const auto& v=verts[d.m_iDispVertStart+i];
        if(!v.m_vVector.IsValid() || v.m_vVector.LengthSqr()>4 || !std::isfinite(v.m_flDist) || std::abs(v.m_flDist)>32768
            || !std::isfinite(v.m_flAlpha) || v.m_flAlpha<0 || v.m_flAlpha>255)return false;}
    CCoreDispInfo core;auto* surface=core.GetSurface();surface->SetPointCount(4);surface->SetPointStart(d.startPosition);
    Vector axisS=polygon[1]-polygon[0],axisT=polygon[3]-polygon[0];VectorNormalize(axisS);VectorNormalize(axisT);
    surface->SetSAxis(axisS);surface->SetTAxis(axisT);surface->SetFlags(0);
    surface->SetContents(CONTENTS_SOLID);
    const Vector2D uv[]={Vector2D(0,0),Vector2D(0,1),Vector2D(1,1),Vector2D(1,0)};
    for(int i=0;i<4;++i){surface->SetPoint(i,polygon[i]);surface->SetPointNormal(i,normal);surface->SetTexCoord(i,uv[i]);
        for(int bump=0;bump<4;++bump)surface->SetLuxelCoord(bump,i,uv[i]);}
    surface->FindSurfPointStartIndex();surface->AdjustSurfPointData();
    // Preview builds full-resolution triangles, not the graphical engine's
    // neighbor-dependent LOD, lighting or surface-physics flag behavior.
    core.InitDispInfo(d.power,0,d.smoothingAngle,verts.data()+d.m_iDispVertStart,tris.data()+d.m_iDispTriStart);
    if(!core.CreateWithoutLOD())return false;
    for(size_t i=0;i<nt;++i){Triangle t;core.GetTriPos(i,t[0],t[1],t[2]);
        for(const auto& p:t)if(!boundedPoint(p))return false;
        out.push_back(t);
    }
    if(collisionTree){auto tree=std::make_unique<PortCDispCollTree>();if(!tree->Create(&core))return false;*collisionTree=std::move(tree);}
    return true;
}
void addBox(std::vector<dvertex_t>& vertices,std::vector<dedge_t>& edges,std::vector<int>& surfedges,std::vector<dface_t>& faces,const Vector& lo,const Vector& hi) {
    const Vector corners[]={ {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z} };
    const int quads[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
    const auto first=vertices.size();for(auto p:corners){dvertex_t v;v.point=p;vertices.push_back(v);}
    for(const auto& q:quads){dface_t f{};f.firstedge=surfedges.size();f.numedges=4;f.dispinfo=-1;f.texinfo=-1;f.lightofs=-1;
        for(unsigned i=0;i<4;++i){dedge_t e{{}};e.v[0]=first+q[i];e.v[1]=first+q[(i+1)%4];surfedges.push_back(edges.size());edges.push_back(e);}faces.push_back(f);}
}
std::vector<unsigned char> fixture(bool displaced=false) {
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
    const Vector lows[]={ {-256,-256,-16},{256,-256,0},{-272,256,0},{-272,-272,0},{-256,-272,0},{16,-48,0},{-96,80,0} };
    const Vector highs[]={ {256,256,0},{272,272,192},{256,272,192},{-256,256,192},{256,-256,192},{80,16,64},{-32,144,96} };
    std::vector<dplane_t> planes;std::vector<dbrushside_t> sides;std::vector<dbrush_t> brushes;
    const int facePlanes[]={5,4,3,2,1,0};
    for(unsigned box=0;box<7;++box){dbrush_t brush{};brush.firstside=sides.size();brush.numsides=6;brush.contents=CONTENTS_SOLID;brushes.push_back(brush);
        for(int axis=0;axis<3;++axis)for(int sign=0;sign<2;++sign){dplane_t plane{};plane.normal[axis]=sign?-1:1;plane.type=axis;plane.dist=sign?-lows[box][axis]:highs[box][axis];planes.push_back(plane);
            dbrushside_t side{};side.planenum=planes.size()-1;side.texinfo=0;side.dispinfo=-1;sides.push_back(side);}
        for(unsigned f=0;f<6;++f){faces[box*6+f].planenum=box*6+facePlanes[f];faces[box*6+f].texinfo=0;}
    }
    if(displaced){
        faces[1].dispinfo=0;ddispinfo_t disp{};disp.startPosition=vertices[4].point;disp.power=2;disp.m_iMapFace=1;disp.smoothingAngle=45;disp.contents=CONTENTS_SOLID;
        // Source displacement surfaces use clockwise parent-face corners.
        const int first=faces[1].firstedge;int old[4];for(int i=0;i<4;++i)old[i]=surfedges[first+i];
        for(int i=0;i<4;++i)surfedges[first+i]=-old[3-i];
        std::memcpy(bytes.data()+h.lumps[LUMP_SURFEDGES].fileofs,surfedges.data(),surfedges.size()*sizeof(int));
        std::vector<CDispVert> dv(25);for(int y=0;y<5;++y)for(int x=0;x<5;++x){auto& v=dv[y*5+x];v.m_vVector=Vector(0,0,1);v.m_flDist=(x==2&&y==2)?32:0;v.m_flAlpha=0;}
        std::vector<CDispTri> dt(32);for(auto& t:dt)t.m_uiTags=DISPTRI_TAG_SURFACE;
        append(LUMP_DISPINFO,&disp,sizeof(disp));append(LUMP_DISP_VERTS,dv.data(),dv.size()*sizeof(CDispVert));append(LUMP_DISP_TRIS,dt.data(),dt.size()*sizeof(CDispTri));
    }
    // The geometry section was appended above; replace it with updated planes/texinfo.
    std::memcpy(bytes.data()+h.lumps[LUMP_FACES].fileofs,faces.data(),faces.size()*sizeof(dface_t));
    dleaf_t leaves[2]{};leaves[0].contents=CONTENTS_SOLID;leaves[0].cluster=-1;leaves[1].cluster=0;
    for(auto& leaf:leaves){leaf.area=1;leaf.numleafbrushes=7;leaf.leafWaterDataID=-1;for(int i=0;i<3;++i){leaf.mins[i]=-272;leaf.maxs[i]=272;}}
    dnode_t node{};node.planenum=4;node.children[0]=-2;node.children[1]=-1;
    dmodel_t model{};model.mins=Vector(-272,-272,-16);model.maxs=Vector(272,272,192);model.headnode=0;model.numfaces=faces.size();
    texinfo_t info{};info.textureVecsTexelsPerWorldUnits[0][0]=info.textureVecsTexelsPerWorldUnits[1][1]=1;info.lightmapVecsLuxelsPerWorldUnits[0][0]=info.lightmapVecsLuxelsPerWorldUnits[1][1]=1;
    dtexdata_t tex{};tex.width=tex.height=tex.view_width=tex.view_height=64;tex.reflectivity=Vector(1,1,1);
    std::vector<unsigned short> leafFaces;for(unsigned i=0;i<faces.size();++i)leafFaces.push_back(i);leaves[1].numleaffaces=faces.size();
    append(LUMP_LEAFFACES,leafFaces.data(),leafFaces.size()*sizeof(unsigned short));
    darea_t areas[2]{};unsigned short leafbrush[]={0,1,2,3,4,5,6};int nameIndex=0;const char name[]="debug/debugempty";const char entities[]="{ \"classname\" \"worldspawn\" }\n";
    append(LUMP_PLANES,planes.data(),planes.size()*sizeof(dplane_t));append(LUMP_BRUSHSIDES,sides.data(),sides.size()*sizeof(dbrushside_t));append(LUMP_BRUSHES,brushes.data(),brushes.size()*sizeof(dbrush_t));
    append(LUMP_LEAFS,leaves,sizeof(leaves));h.lumps[LUMP_LEAFS].version=1;append(LUMP_LEAFBRUSHES,leafbrush,sizeof(leafbrush));
    append(LUMP_NODES,&node,sizeof(node));append(LUMP_MODELS,&model,sizeof(model));append(LUMP_TEXINFO,&info,sizeof(info));
    append(LUMP_TEXDATA,&tex,sizeof(tex));append(LUMP_TEXDATA_STRING_TABLE,&nameIndex,sizeof(nameIndex));append(LUMP_TEXDATA_STRING_DATA,name,sizeof(name));
    append(LUMP_AREAS,areas,sizeof(areas));append(LUMP_ENTITIES,entities,sizeof(entities));
    std::memcpy(bytes.data(),&h,sizeof(h));return bytes;
}
}
namespace source1ios {
struct SourceMap::Impl {
    std::vector<MeshPoint> mesh,modelMesh;Collision collision, fixtureCollision;std::unique_ptr<PhysicsScene> scene;
    std::vector<std::unique_ptr<PortCDispCollTree>> displacementCollision;
    std::string builtin,builtinModel;Vector camera;QAngle angles;SourceTexture texture;model_t* world=nullptr;bool builtinActive=false;
};
SourceMap::SourceMap()=default;
SourceMap::~SourceMap(){stop();}
bool SourceMap::start(const std::filesystem::path& root) {
    if(impl_)return false;
    Msg("Source BSP startup: creating texture\n");
    impl_=std::make_unique<Impl>();impl_->builtin="__source1ios_geometry.bsp";impl_->builtinModel="models/__source1ios_static_probe.mdl";
    g_pFullFileSystem->AddSearchPath((root/"selftest").c_str(),"PORT_BSP_PREVIEW",PATH_ADD_TO_HEAD);
    // Serialize a genuine VTF, read it through Source filesystem, then decode
    // the texture for the native Metal adapter. No shaderapiempty drawing implied.
    using VTF=std::unique_ptr<IVTFTexture,decltype(&DestroyVTFTexture)>;
    VTF source(CreateVTFTexture(),DestroyVTFTexture), decoded(CreateVTFTexture(),DestroyVTFTexture);
    if(!source || !decoded || !source->Init(64,64,1,IMAGE_FORMAT_RGBA8888,0,1)){stop();return false;}
    Msg("Source BSP startup: filling texture mipmaps\n");
    for(int mip=0;mip<source->MipCount();++mip){int w=std::max(1,64>>mip),h=w;auto* pixels=source->ImageData(0,0,mip);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto* pixel=pixels+(y*w+x)*4;const bool alternate=((x*64/w)/8+(y*64/h)/8)%2;pixel[0]=pixel[1]=pixel[2]=alternate?210:120;pixel[3]=255;}}
    Msg("Source BSP startup: serializing VTF\n");
    CUtlBuffer encoded;if(!source->Serialize(encoded)){stop();return false;}
    const std::string texturePath="__source1ios_checker.vtf";auto textureFile=g_pFullFileSystem->Open(texturePath.c_str(),"wb","PORT_BSP_PREVIEW");
    bool textureOK=textureFile && g_pFullFileSystem->Write(encoded.Base(),encoded.TellPut(),textureFile)==encoded.TellPut();if(textureFile)g_pFullFileSystem->Close(textureFile);
    Msg("Source BSP startup: reading VTF from filesystem\n");
    CUtlBuffer disk;if(!textureOK || !g_pFullFileSystem->ReadFile(texturePath.c_str(),"PORT_BSP_PREVIEW",disk) || !decoded->Unserialize(disk)){stop();return false;}
    Msg("Source BSP startup: converting VTF\n");
    decoded->ConvertImageFormat(IMAGE_FORMAT_RGBA8888,false);impl_->texture.width=decoded->Width();impl_->texture.height=decoded->Height();
    impl_->texture.pixels.assign(decoded->ImageData(0,0,0),decoded->ImageData(0,0,0)+64*64*4);
    Msg("Source BSP startup: generating BSP\n");
    const auto bytes=fixture();auto file=g_pFullFileSystem->Open(impl_->builtin.c_str(),"wb","PORT_BSP_PREVIEW");
    bool ok=file && g_pFullFileSystem->Write(bytes.data(),bytes.size(),file)==int(bytes.size());if(file)g_pFullFileSystem->Close(file);
    const auto terrain=fixture(true);auto terrainFile=g_pFullFileSystem->Open("__source1ios_displacement.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok && terrainFile && g_pFullFileSystem->Write(terrain.data(),terrain.size(),terrainFile)==int(terrain.size());if(terrainFile)g_pFullFileSystem->Close(terrainFile);
    std::filesystem::create_directories(root/"game/models");const auto studio=makeStudioFixture();
    auto writeModel=[&](const char* path,const std::vector<std::uint8_t>& data){auto out=g_pFullFileSystem->Open(path,"wb","DEFAULT_WRITE_PATH");const bool written=out&&g_pFullFileSystem->Write(data.data(),data.size(),out)==int(data.size());if(out)g_pFullFileSystem->Close(out);return written;};
    ok=ok&&writeModel(impl_->builtinModel.c_str(),studio.mdl)&&writeModel("models/__source1ios_static_probe.vvd",studio.vvd)&&writeModel("models/__source1ios_static_probe.dx90.vtx",studio.vtx);
    if(!ok || !load(impl_->builtin.c_str(),nullptr) || !loadModel(impl_->builtinModel.c_str())){stop();return false;}
    impl_->world=modelloader->GetModelForName(impl_->builtin.c_str(),IModelLoader::FMODELLOADER_SERVER);
    if(!impl_->world || !modelloader->IsLoaded(impl_->world) || impl_->world->type!=mod_brush){stop();return false;}
    Msg("Source engine brush world loaded: %d vertices, %d surfaces, %d leaves\n",impl_->world->brush.pShared->numvertexes,impl_->world->brush.pShared->numsurfaces,impl_->world->brush.pShared->numleafs);
    impl_->fixtureCollision=impl_->collision;
    if(!selfTest()){stop();return false;}
    Msg("Source BSP preview ready: original lump loader + polygon collision + Metal adapter. Engine brush world loaded; original graphical materialsystem and game remain pending.\n");return true;
}
void SourceMap::stop(){impl_.reset();}
bool SourceMap::resetMap(){return impl_ && load(impl_->builtin.c_str(),nullptr);}
bool SourceMap::demoTerrain(){return impl_ && load("__source1ios_displacement.bsp",nullptr);}
bool SourceMap::resetModel(){return impl_&&loadModel(impl_->builtinModel.c_str());}
bool SourceMap::loadModel(const char* filename,const char* pathID){
    if(!impl_||!filename||std::strlen(filename)>=MAX_PATH)return false;std::string mdlPath=filename;
    if(mdlPath.size()<5||mdlPath.substr(mdlPath.size()-4)!=".mdl"){Warning("Source studio: expected .mdl path: %s\n",filename);return false;}
    const auto base=mdlPath.substr(0,mdlPath.size()-4);auto read=[&](const std::string& path,std::vector<std::uint8_t>& bytes){CUtlBuffer data;if(!g_pFullFileSystem->ReadFile(path.c_str(),pathID,data)||data.TellPut()<=0)return false;bytes.assign(static_cast<std::uint8_t*>(data.Base()),static_cast<std::uint8_t*>(data.Base())+data.TellPut());return true;};
    std::vector<std::uint8_t> mdl,vvd,vtx;if(!read(mdlPath,mdl)||!read(base+".vvd",vvd)||!read(base+".dx90.vtx",vtx)){Warning("Source studio: missing MDL/VVD/DX90.VTX companion for %s\n",filename);return false;}
    StudioMesh parsed;std::string error;if(!parseStudioModel(mdl,vvd,vtx,parsed,error)){Warning("Source studio rejected %s: %s\n",filename,error.c_str());return false;}
    std::vector<MeshPoint> staged;staged.reserve(parsed.triangles.size());const Vector origin(0,64,0);
    for(const auto& v:parsed.triangles){const float light=.35f+.65f*std::abs(v.normal.z*.8f+v.normal.x*.3f+v.normal.y*.2f);staged.push_back({v.position+origin,{.2f*light,.85f*light,.35f*light},{v.uv.x,v.uv.y}});}
    impl_->modelMesh=std::move(staged);Msg("Source studio model loaded: %u source vertices, %zu triangles, %u meshes from %s\n",parsed.sourceVertices,parsed.triangles.size()/3,parsed.meshes,filename);return true;
}
bool SourceMap::load(const char* filename,const char* pathID) {
    if(!impl_ || !filename || std::strlen(filename)>=MAX_PATH || CMapLoadHelper::GetRefCount()!=0)return false;
    auto file=g_pFullFileSystem->Open(filename,"rb",pathID);if(!file){Warning("Source BSP: file not found: %s\n",filename);return false;}
    const auto size=g_pFullFileSystem->Size(file);dheader_t h{};
    const bool read=g_pFullFileSystem->Read(&h,sizeof(h),file)==sizeof(h);
    if(!read || !headerValid(h,size,filename)){if(!read)Warning("Source BSP rejected %s: truncated BSP header (%u bytes)\n",filename,size);g_pFullFileSystem->Close(file);return false;}
    std::array<std::vector<unsigned char>,geometryLumpCount> decoded;
    for(size_t i=0;i<geometryLumpCount;++i){const auto& l=h.lumps[geometryLumps[i]];if(!l.uncompressedSize)continue;
        std::vector<unsigned char> raw(l.filelen);g_pFullFileSystem->Seek(file,l.fileofs,FILESYSTEM_SEEK_HEAD);
        const char* reason="short compressed lump read";
        if(g_pFullFileSystem->Read(raw.data(),raw.size(),file)!=int(raw.size()) || !decodeLump(raw,l.uncompressedSize,decoded[i],reason)){
            Warning("Source BSP rejected %s: %s; lump=%d\n",filename,reason,geometryLumps[i]);g_pFullFileSystem->Close(file);return false;
        }
        Msg("Source BSP LZMA lump %d decoded: %d -> %zu bytes\n",geometryLumps[i],l.filelen,decoded[i].size());
    }
    g_pFullFileSystem->Close(file);
    // External .lmp overlays bypass the validated on-disk header. Reject them.
    char overlay[MAX_PATH];V_StripExtension(filename,overlay,sizeof(overlay));V_strncat(overlay,"_l_0.lmp",sizeof(overlay));
    if(g_pFullFileSystem->FileExists(overlay,pathID)){Warning("Source BSP: external lump overlays unsupported\n");return false;}
    std::vector<MeshPoint> mesh;
    std::vector<std::unique_ptr<PortCDispCollTree>> displacementCollision;
    {
        LoaderScope scope(filename);
        auto points=lump<dvertex_t>(LUMP_VERTEXES,decoded[0]);auto edges=lump<dedge_t>(LUMP_EDGES,decoded[1]);
        auto surfedges=lump<int>(LUMP_SURFEDGES,decoded[2]);auto faces=lump<dface_t>(LUMP_FACES,decoded[3]);
        auto disps=lump<ddispinfo_t>(LUMP_DISPINFO,decoded[4]);auto dispVerts=lump<CDispVert>(LUMP_DISP_VERTS,decoded[5]);auto dispTris=lump<CDispTri>(LUMP_DISP_TRIS,decoded[6]);
        for(const auto& p:points)if(!p.point.IsValid() || std::abs(p.point.x)>32768 || std::abs(p.point.y)>32768 || std::abs(p.point.z)>32768)return false;
        for(size_t face=0;face<faces.size();++face){const auto& f=faces[face];
            if(f.numedges<3 || f.numedges>256 || f.firstedge<0 || size_t(f.firstedge)>surfedges.size() || size_t(f.numedges)>surfedges.size()-size_t(f.firstedge))return false;
            std::vector<Vector> polygon;
            for(int i=0;i<f.numedges;++i){const int se=surfedges[f.firstedge+i];if(se==std::numeric_limits<int>::min())return false;
                const size_t e=se<0?size_t(-se):size_t(se);if(e>=edges.size())return false;
                const size_t v=edges[e].v[se<0?1:0];if(v>=points.size())return false;polygon.push_back(points[v].point);}
            std::vector<Triangle> triangles;
            if(f.dispinfo>=0){
                std::unique_ptr<PortCDispCollTree> tree;
                if(size_t(f.dispinfo)>=disps.size() || !displacementTriangles(polygon,face,disps[f.dispinfo],dispVerts,dispTris,triangles,&tree)){
                    Warning("Source BSP rejected %s: invalid displacement; face=%zu, disp=%d\n",filename,face,f.dispinfo);return false;}
                displacementCollision.push_back(std::move(tree));
            }else for(size_t i=1;i+1<polygon.size();++i)triangles.push_back({polygon[0],polygon[i],polygon[i+1]});
            for(const auto& triangle:triangles){if(mesh.size()+3>maximumVertices)return false;
                Vector normal;CrossProduct(triangle[1]-triangle[0],triangle[2]-triangle[0],normal);if(normal.LengthSqr()<1e-8f)continue;VectorNormalize(normal);
                const float shade=.35f+.65f*std::abs(normal.z*.8f+normal.x*.3f+normal.y*.2f);
                const float palette[6][3]={{.3f,.7f,.9f},{.7f,.8f,.9f},{.9f,.5f,.2f},{.4f,.8f,.5f},{.65f,.45f,.85f},{.85f,.75f,.35f}};
                for(auto p:triangle){
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
    auto live=physicsScene(collision);if(!live)return false;
    impl_->scene=std::move(live);
    impl_->displacementCollision=std::move(displacementCollision);
    impl_->mesh=std::move(mesh);impl_->collision=std::move(collision);impl_->builtinActive=impl_->builtin==filename;resetCamera();
    Msg("Source BSP polygons loaded: %zu triangles from %s\n",impl_->mesh.size()/3,filename);return true;
}
void SourceMap::resetCamera(){if(impl_){impl_->camera=Vector(-190,-160,80);impl_->angles=QAngle(8,45,0);}}
bool SourceMap::resetPhysics(){
    if(!impl_)return false;auto scene=physicsScene(impl_->collision);if(!scene)return false;
    impl_->scene=std::move(scene);Msg("Source live physics scene reset: two bodies and original ragdoll joint\n");return true;
}
bool SourceMap::impulsePhysics(){
    if(!impl_ || !impl_->scene)return false;
    impl_->scene->body->Wake();impl_->scene->body->ApplyForceCenter(Vector(6000,2000,0));
    Msg("Source live physics: impulse applied to constrained body\n");return true;
}
void SourceMap::look(float yaw,float pitch){if(impl_ && std::isfinite(yaw)&&std::isfinite(pitch)){impl_->angles.y=std::remainder(impl_->angles.y+yaw,360.f);impl_->angles.x=std::max(-85.f,std::min(85.f,impl_->angles.x+pitch));}}
void SourceMap::move(float forward,float right,float seconds){
    if(!impl_ || !std::isfinite(forward)||!std::isfinite(right)||!std::isfinite(seconds)||seconds<=0)return;
    Vector f,r;AngleVectors(QAngle(0,impl_->angles.y,0),&f,&r,nullptr);Vector delta=f*forward+r*right;
    if(delta.LengthSqr()>1)VectorNormalize(delta);delta*=160*std::min(seconds,.1f);
    // Swept hull against the actual IVP polygon collision avoids wall tunneling.
    trace_t trace{};
    if(impl_->builtinActive && impl_->world){Ray_t hull;hull.Init(impl_->camera,impl_->camera+delta,Vector(-8,-8,-24),Vector(8,8,8));CM_BoxTrace(hull,0,MASK_PLAYERSOLID,true,trace);}
    else g_pPhysicsCollision->TraceBox(impl_->camera,impl_->camera+delta,Vector(-8,-8,-24),Vector(8,8,8),impl_->collision.get(),Vector(0,0,0),QAngle(0,0,0),&trace);
    if(!impl_->displacementCollision.empty()){
        Ray_t hull;hull.Init(impl_->camera,impl_->camera+delta,Vector(-8,-8,-24),Vector(8,8,8));const auto inv=hull.InvDelta();
        for(const auto& tree:impl_->displacementCollision)tree->AABBTree_SweepAABB(hull,inv,&trace);
    }
    if(!trace.startsolid)impl_->camera+=delta*std::max(0.f,trace.fraction-.001f);
}
void SourceMap::frame(float seconds){if(impl_ && impl_->scene && seconds>0 && std::isfinite(seconds))impl_->scene->environment->Simulate(std::min(seconds,.05f));}
std::vector<SourceVertex> SourceMap::vertices(float aspect) const {
    std::vector<SourceVertex> out;if(!impl_)return out;out.reserve(impl_->mesh.size()+impl_->modelMesh.size());Vector f,r,u;AngleVectors(impl_->angles,&f,&r,&u);
    const float a=std::max(aspect,.01f),scale=1.3f,near=1,far=8192;
    auto append=[&](const MeshPoint& v){Vector relative=v.position-impl_->camera;float depth=DotProduct(relative,f);
        out.push_back({{DotProduct(relative,r)*scale/a,DotProduct(relative,u)*scale,depth*far/(far-near)-near*far/(far-near),depth},{v.color[0],v.color[1],v.color[2],1},{v.uv[0],v.uv[1]}});};
    for(const auto& v:impl_->mesh)append(v);
    for(const auto& v:impl_->modelMesh)append(v);
    if(impl_->scene){
        constexpr int rings=6,slices=12;
        constexpr float pi=3.14159265358979323846f;
        auto point=[&](int ring,int slice){float latitude=pi*ring/rings,longitude=2*pi*slice/slices;
            return Vector(8*std::sin(latitude)*std::cos(longitude),8*std::sin(latitude)*std::sin(longitude),8*std::cos(latitude));};
        for(auto* object:{impl_->scene->anchor,impl_->scene->body}){matrix3x4_t pose;object->GetPositionMatrix(&pose);
            for(int ring=0;ring<rings;++ring)for(int slice=0;slice<slices;++slice){
                const Vector quad[]={point(ring,slice),point(ring+1,slice),point(ring+1,slice+1),point(ring,slice+1)};
                for(int index:{0,1,2,0,2,3}){Vector p;VectorTransform(quad[index],pose,p);
                    const float shade=.55f+.45f*(quad[index].z/8+1)/2;
                    append({p,{shade,.8f*shade,.2f*shade},{float(slice)/slices,float(ring)/rings}});}
            }
        }
    }
    return out;
}
const SourceTexture& SourceMap::texture() const { static const SourceTexture empty; return impl_?impl_->texture:empty; }
bool SourceMap::selfTest(){
    if(!impl_)return false;bool all=report("original lump geometry",!impl_->mesh.empty()&&CMapLoadHelper::GetRefCount()==0);
    dheader_t bad{};bad.ident=IDBSPHEADER;bad.version=BSPVERSION;bad.lumps[LUMP_VERTEXES].fileofs=sizeof(bad);bad.lumps[LUMP_VERTEXES].filelen=12;
    all&=report("truncated lump range rejected",!headerValid(bad,sizeof(bad)));
    auto encoded=bspLzmaVertices();std::vector<unsigned char> decoded;const char* reason=nullptr;
    const auto original=fixture();dheader_t originalHeader;std::memcpy(&originalHeader,original.data(),sizeof(originalHeader));
    const auto& originalVertices=originalHeader.lumps[LUMP_VERTEXES];
    all&=report("bounded Source LZMA vertex decode",decodeLump(encoded,originalVertices.filelen,decoded,reason)
        && decoded.size()==size_t(originalVertices.filelen) && !std::memcmp(decoded.data(),original.data()+originalVertices.fileofs,decoded.size()));
    bool rejects=true;auto broken=encoded;broken.pop_back();rejects&=!decodeLump(broken,672,decoded,reason);
    broken=encoded;broken[12]=255;rejects&=!decodeLump(broken,672,decoded,reason);
    broken=encoded;broken[16]=127;rejects&=!decodeLump(broken,672,decoded,reason);
    rejects&=!decodeLump(encoded,maximumLump+1,decoded,reason);
    broken=encoded;broken[17]=255;rejects&=!decodeLump(broken,672,decoded,reason);
    all&=report("malformed LZMA sizes/properties/stream rejected",rejects);
    const auto studio=makeStudioFixture();StudioMesh model;std::string modelError;
    all&=report("MDL/VVD/VTX static mesh",parseStudioModel(studio.mdl,studio.vvd,studio.vtx,model,modelError)&&model.sourceVertices==8&&model.triangles.size()==36&&model.meshes==1);
    auto fixedVvd=studio.vvd;const auto originalVvd=*reinterpret_cast<const vertexFileHeader_t*>(studio.vvd.data());
    fixedVvd.resize(sizeof(vertexFileHeader_t)+sizeof(vertexFileFixup_t)+8*sizeof(mstudiovertex_t));
    auto* fixedHeader=reinterpret_cast<vertexFileHeader_t*>(fixedVvd.data());*fixedHeader=originalVvd;fixedHeader->numFixups=1;fixedHeader->fixupTableStart=sizeof(vertexFileHeader_t);fixedHeader->vertexDataStart=sizeof(vertexFileHeader_t)+sizeof(vertexFileFixup_t);
    auto* fixup=reinterpret_cast<vertexFileFixup_t*>(fixedVvd.data()+fixedHeader->fixupTableStart);fixup->lod=0;fixup->sourceVertexID=0;fixup->numVertexes=8;
    std::memcpy(fixedVvd.data()+fixedHeader->vertexDataStart,studio.vvd.data()+originalVvd.vertexDataStart,8*sizeof(mstudiovertex_t));
    auto stripVtx=studio.vtx;auto* stripHeader=reinterpret_cast<OptimizedModel::FileHeader_t*>(stripVtx.data());auto* strip=stripHeader->pBodyPart(0)->pModel(0)->pLOD(0)->pMesh(0)->pStripGroup(0)->pStrip(0);strip->flags=OptimizedModel::STRIP_IS_TRISTRIP;strip->numIndices=4;
    all&=report("VVD fixups and VTX triangle strip",parseStudioModel(studio.mdl,fixedVvd,stripVtx,model,modelError)&&model.sourceVertices==8&&model.triangles.size()==6);
    auto wrongVvd=studio.vvd;reinterpret_cast<vertexFileHeader_t*>(wrongVvd.data())->checksum^=1;
    bool modelRejects=!parseStudioModel(studio.mdl,wrongVvd,studio.vtx,model,modelError);
    auto wrongVtx=studio.vtx;auto* header=reinterpret_cast<OptimizedModel::FileHeader_t*>(wrongVtx.data());header->bodyPartOffset=std::numeric_limits<int>::max();modelRejects&=!parseStudioModel(studio.mdl,studio.vvd,wrongVtx,model,modelError);
    auto wrongMdl=studio.mdl;reinterpret_cast<studiohdr_t*>(wrongMdl.data())->length=std::numeric_limits<int>::max();modelRejects&=!parseStudioModel(wrongMdl,studio.vvd,studio.vtx,model,modelError);
    all&=report("MDL companion mismatch/ranges rejected",modelRejects);
    const MDLHandle_t handle=g_pMDLCache->FindMDL(impl_->builtinModel.c_str());const auto* cached=handle==MDLHANDLE_INVALID?nullptr:g_pMDLCache->GetStudioHdr(handle);
    all&=report("original MDLCache studio header",cached&&cached->id==idStudioHeader&&cached->version==STUDIO_VERSION&&cached->checksum==0x510510);
    if(handle!=MDLHANDLE_INVALID)g_pMDLCache->Release(handle);
    all&=report("Metal static model geometry staged",impl_->modelMesh.size()==36);
    const std::vector<Vector> quad={Vector(-64,-64,0),Vector(-64,64,0),Vector(64,64,0),Vector(64,-64,0)};
    ddispinfo_t disp{};disp.startPosition=quad[0];disp.power=2;disp.smoothingAngle=45;
    std::vector<CDispVert> dv(25);for(auto& v:dv){v.m_vVector=Vector(0,0,1);v.m_flDist=0;v.m_flAlpha=0;}dv[12].m_flDist=32;
    std::vector<CDispTri> dt(32);for(auto& t:dt)t.m_uiTags=DISPTRI_TAG_SURFACE;
    std::vector<Triangle> terrain;std::unique_ptr<PortCDispCollTree> terrainTree;
    const bool terrainBuilt=displacementTriangles(quad,0,disp,dv,dt,terrain,&terrainTree);
    float height=0;for(const auto& t:terrain)for(const auto& p:t)height=std::max(height,p.z);
    bool allPowers=terrainBuilt && terrain.size()==32 && std::abs(height-32)<.001f;
    for(int power=3;power<=4;++power){auto large=disp;large.power=power;std::vector<CDispVert> lv(large.NumVerts());
        for(auto& v:lv){v.m_vVector=Vector(0,0,1);v.m_flDist=0;v.m_flAlpha=0;}lv[lv.size()/2].m_flDist=32;
        std::vector<CDispTri> lt(large.NumTris());for(auto& t:lt)t.m_uiTags=DISPTRI_TAG_SURFACE;
        std::vector<Triangle> built;allPowers&=displacementTriangles(quad,0,large,lv,lt,built) && built.size()==size_t(large.NumTris());}
    all&=report("original displacement powers 2/3/4 terrain",allPowers);
    std::vector<Triangle> rejectedTerrain;auto wrong=disp;wrong.power=31;
    bool rejectsDisp=!displacementTriangles(quad,0,wrong,dv,dt,rejectedTerrain);
    wrong=disp;wrong.m_iDispVertStart=std::numeric_limits<int>::max();rejectsDisp&=!displacementTriangles(quad,0,wrong,dv,dt,rejectedTerrain);
    wrong=disp;wrong.startPosition=Vector(0,0,0);rejectsDisp&=!displacementTriangles(quad,0,wrong,dv,dt,rejectedTerrain);
    auto invalidVerts=dv;invalidVerts[0].m_flDist=std::numeric_limits<float>::quiet_NaN();rejectsDisp&=!displacementTriangles(quad,0,disp,invalidVerts,dt,rejectedTerrain);
    all&=report("malformed displacement metadata rejected",rejectsDisp && rejectedTerrain.empty());
    Ray_t terrainRay;terrainRay.Init(Vector(0,0,80),Vector(0,0,-80));trace_t terrainTrace{};terrainTrace.fraction=1;
    const bool terrainHit=terrainTree && terrainTree->AABBTree_Ray(terrainRay,terrainRay.InvDelta(),&terrainTrace);
    Ray_t terrainHull;terrainHull.Init(Vector(0,0,80),Vector(0,0,-80),Vector(-1,-1,-1),Vector(1,1,1));trace_t hullTrace{};hullTrace.fraction=1;
    const bool hullHit=terrainTree && terrainTree->AABBTree_SweepAABB(terrainHull,terrainHull.InvDelta(),&hullTrace);
    all&=report("Source displacement ray/hull hit raised height",terrainHit && !terrainTrace.startsolid && std::abs(terrainTrace.fraction-.3f)<.01f
        && hullHit && !hullTrace.startsolid && std::abs(hullTrace.fraction-.29375f)<.003f);
    trace_t trace{};g_pPhysicsCollision->TraceBox(Vector(-190,-160,80),Vector(-190,-160,-80),Vector(0,0,0),Vector(0,0,0),impl_->fixtureCollision.get(),Vector(0,0,0),QAngle(0,0,0),&trace);
    all&=report("IVP polygon floor trace",trace.fraction>0 && trace.fraction<1 && !trace.startsolid);
    g_pPhysicsCollision->TraceBox(Vector(-190,-160,80),Vector(400,-160,80),Vector(-8,-8,-24),Vector(8,8,8),impl_->fixtureCollision.get(),Vector(0,0,0),QAngle(0,0,0),&trace);
    all&=report("IVP swept camera wall",trace.fraction>0 && trace.fraction<.8f && !trace.startsolid);
    all&=report("VTF preview texture decoded",impl_->texture.width==64 && impl_->texture.height==64 && impl_->texture.pixels.size()==64*64*4);
    all&=report("engine brush model loaded",impl_->world && modelloader->IsLoaded(impl_->world) && impl_->world->type==mod_brush && impl_->world->brush.pShared->numvertexes==56 && impl_->world->brush.pShared->numsurfaces==42);
    Ray_t ray;ray.Init(Vector(-190,-160,80),Vector(-190,-160,-80));CM_BoxTrace(ray,0,MASK_SOLID,true,trace);
    all&=report("engine CM floor trace",trace.fraction>.49f && trace.fraction<.51f);
    all&=report("engine CM point contents",CM_PointContents(Vector(-190,-160,80),0)==CONTENTS_EMPTY && (CM_PointContents(Vector(-190,-160,-8),0)&CONTENTS_SOLID));
    all&=report("engine worldspawn entities",CM_EntityString() && std::strstr(CM_EntityString(),"worldspawn"));
    auto testScene=physicsScene(impl_->fixtureCollision);
    if(!testScene)return report("live physics body pose advances",false);
    Vector oldPosition,newPosition;testScene->body->GetPosition(&oldPosition,nullptr);testScene->environment->Simulate(.02f);testScene->body->GetPosition(&newPosition,nullptr);
    all&=report("live physics body pose advances",newPosition.IsValid() && (newPosition-oldPosition).Length()>1e-5f);
    auto before=vertices(1);const auto saved=impl_->angles;look(10,0);auto after=vertices(1);impl_->angles=saved;
    all&=report("Source camera projection",before.size()==after.size()&&!before.empty()&&before[0].position[0]!=after[0].position[0]);return all;
}
}
