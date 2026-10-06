#include "SourceMap.hpp"
#include "BspLzmaFixture.hpp"
#include "SourceStudio.hpp"
#include "SourceEntities.hpp"
#include "SourceProps.hpp"
#include "SourcePhy.hpp"
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
#include "tier1/KeyValues.h"
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
bool readBounded(const char* path,const char* pathID,size_t limit,std::vector<std::uint8_t>& bytes){
    auto file=g_pFullFileSystem->Open(path,"rb",pathID);if(!file)return false;const unsigned size=g_pFullFileSystem->Size(file);bool ok=size>0&&size<=limit;if(ok){bytes.resize(size);ok=g_pFullFileSystem->Read(bytes.data(),size,file)==int(size);}g_pFullFileSystem->Close(file);if(!ok)bytes.clear();return ok;
}
bool materialPath(const std::string& path){return !path.empty()&&path.size()<240&&path.front()!='/'&&path.find("..") == std::string::npos&&path.find('\\')==std::string::npos&&path.find(':')==std::string::npos;}
bool normalizeMaterialPath(std::string& path){std::replace(path.begin(),path.end(),'\\','/');return materialPath(path);}
bool vtfResourceRangesValid(const std::vector<std::uint8_t>& bytes){
    // The legacy decoder allocates auxiliary chunks before checking their
    // payload range. Validate those ranges and the aggregate budget first.
    if(bytes.size()<sizeof(VTFFileBaseHeader_t))return false;
    VTFFileBaseHeader_t base{};std::memcpy(&base,bytes.data(),sizeof(base));
    if(std::memcmp(base.fileTypeString,"VTF\0",4)||base.version[0]!=7||base.version[1]<0||base.version[1]>VTF_MINOR_VERSION)return false;
    const size_t fixed=base.version[1]>=3?sizeof(VTFFileHeader_t):base.version[1]==2?sizeof(VTFFileHeaderV7_2_t):sizeof(VTFFileHeaderV7_1_t);
    if(bytes.size()<fixed||base.headerSize<int(fixed)||size_t(base.headerSize)>bytes.size())return false;
    VTFFileHeader_t header{};std::memcpy(&header,bytes.data(),fixed);
    if(!header.width||!header.height||header.width>2048||header.height>2048||header.numFrames!=1||(base.version[1]>=2&&header.depth!=1))return false;
    if(base.version[1]<3)return true;
    if(header.numResources>32)return false;
    const size_t end=fixed+size_t(header.numResources)*sizeof(ResourceEntryInfo);
    if(end>size_t(base.headerSize))return false;
    size_t budget=0;
    for(unsigned i=0;i<header.numResources;++i){ResourceEntryInfo entry{};std::memcpy(&entry,bytes.data()+fixed+i*sizeof(entry),sizeof(entry));
        if(entry.eType&RSRCF_HAS_NO_DATA_CHUNK)continue;
        const size_t offset=entry.resData;if(offset<size_t(base.headerSize)||offset>=bytes.size())return false;
        if(entry.eType==VTF_LEGACY_RSRC_IMAGE||entry.eType==VTF_LEGACY_RSRC_LOW_RES_IMAGE)continue;
        if(bytes.size()-offset<4)return false;int length=0;std::memcpy(&length,bytes.data()+offset,4);
        if(length<0||size_t(length)>bytes.size()-offset-4||size_t(length)>16*1024*1024-budget)return false;
        budget+=size_t(length);
    }return true;
}
bool vmtBaseTexture(std::string name,std::string& base){
    std::vector<std::string> visited;std::string insert,replace;bool hasInsert=false,hasReplace=false;
    for(unsigned chain=0;chain<10;++chain){
        if(!materialPath(name))return false;
        std::string key=name;for(char& c:key)if(c>='A'&&c<='Z')c+='a'-'A';
        if(std::find(visited.begin(),visited.end(),key)!=visited.end())return false;visited.push_back(key);
        std::vector<std::uint8_t> vmt;const std::string path="materials/"+name+".vmt";
        if(!readBounded(path.c_str(),"GAME",65536,vmt))return false;
        if(!vmt.empty()&&vmt.back()==0)vmt.pop_back();
        if(std::find(vmt.begin(),vmt.end(),0)!=vmt.end())return false;
        std::string directiveScan(vmt.begin(),vmt.end());
        for(char& c:directiveScan)if(c>='A'&&c<='Z')c+='a'-'A';
        if(directiveScan.find("#include")!=std::string::npos||directiveScan.find("#base")!=std::string::npos)return false;
        // No KeyValues #include/#base filesystem recursion: only the bounded
        // VMT Patch chain below can read another file.
        int depth=0;bool quoted=false,valid=true;
        for(size_t i=0;i<vmt.size();++i){const unsigned char c=vmt[i];
            if(c=='"'){quoted=!quoted;continue;}
            if(!quoted&&c=='/'&&i+1<vmt.size()&&vmt[i+1]=='/'){while(i<vmt.size()&&vmt[i]!='\n')++i;continue;}
            if(!quoted&&c=='#'){valid=false;break;}
            if(!quoted&&c=='{'&&++depth>16){valid=false;break;}
            if(!quoted&&c=='}'&&--depth<0){valid=false;break;}
        }
        if(!valid||quoted||depth)return false;
        auto* kv=new KeyValues("preview");kv->UsesEscapeSequences(false);const std::string text(vmt.begin(),vmt.end());
        const bool loaded=kv->LoadFromBuffer(path.c_str(),text.c_str());
        if(!loaded||kv->GetNextKey()){kv->deleteThis();return false;}
        const std::string shader=kv->GetName();
        if(!V_stricmp(shader.c_str(),"Patch")){
            // Match Source ApplyPatchKeyValues: insert overwrites/adds, replace
            // only changes an existing key. Inner patch keys win when gathered.
            if(auto* section=kv->FindKey("insert"))if(section->FindKey("$basetexture")){insert=section->GetString("$basetexture");hasInsert=true;}
            if(auto* section=kv->FindKey("replace"))if(section->FindKey("$basetexture")){replace=section->GetString("$basetexture");hasReplace=true;}
            std::string include=kv->GetString("include","");kv->deleteThis();
            std::replace(include.begin(),include.end(),'\\','/');
            if(include.size()>10&&!V_strnicmp(include.c_str(),"materials/",10))include.erase(0,10);
            if(include.size()<5||V_stricmp(include.substr(include.size()-4).c_str(),".vmt"))return false;
            include.resize(include.size()-4);name=std::move(include);continue;
        }
        bool exists=kv->FindKey("$basetexture")!=nullptr;base=kv->GetString("$basetexture","");kv->deleteThis();
        if(V_stricmp(shader.c_str(),"VertexLitGeneric")&&V_stricmp(shader.c_str(),"UnlitGeneric")&&V_stricmp(shader.c_str(),"LightmappedGeneric"))return false;
        if(hasInsert){base=insert;exists=true;}if(hasReplace&&exists)base=replace;
        return normalizeMaterialPath(base);
    }return false;
}
bool decodeMaterial(const std::vector<std::string>& candidates,source1ios::SourceTexture& texture,const char* kind){
    for(const auto& name:candidates){std::string base;if(!vmtBaseTexture(name,base))continue;const std::string path="materials/"+name+".vmt";
        std::vector<std::uint8_t> vtf;const std::string texturePath="materials/"+base+".vtf";if(!readBounded(texturePath.c_str(),"GAME",16*1024*1024,vtf))continue;
        if(!vtfResourceRangesValid(vtf))continue;
        auto* image=CreateVTFTexture();if(!image)continue;CUtlBuffer buffer(vtf.data(),vtf.size(),CUtlBuffer::READ_ONLY);
        bool ok=image->Unserialize(buffer,true)&&image->Width()>0&&image->Height()>0&&image->Width()<=2048&&image->Height()<=2048&&image->Depth()==1&&image->FrameCount()==1&&image->FaceCount()==1;
        if(ok){buffer.SeekGet(CUtlBuffer::SEEK_HEAD,0);ok=image->Unserialize(buffer);}
        if(ok){image->ConvertImageFormat(IMAGE_FORMAT_RGBA8888,false);texture.width=image->Width();texture.height=image->Height();const auto* data=image->ImageData(0,0,0);ok=data!=nullptr;if(ok)texture.pixels.assign(data,data+size_t(texture.width)*texture.height*4);}
        DestroyVTFTexture(image);if(ok){Msg("Source %s VMT/VTF base texture decoded: %s (%ux%u)\n",kind,path.c_str(),texture.width,texture.height);return true;}
    }return false;
}
bool report(const char* name, bool ok) {
    Msg("Source BSP self-test %s: %s\n",name,ok?"PASS":"FAIL");return ok;
}
struct MeshPoint { Vector position; float color[3]; float uv[2]; unsigned material=0; float lightmap[3]{}; };
unsigned appendAtlasTile(source1ios::SourceTexture& atlas,unsigned& count,const source1ios::SourceTexture& texture){
    const unsigned slot=count++,oldColumns=std::min(16u,slot),columns=std::min(16u,count),rows=(count+columns-1)/columns;
    source1ios::SourceTexture next{columns*64,rows*64,std::vector<std::uint8_t>(size_t(columns)*rows*64*64*4)};
    for(unsigned i=0;i<slot;++i)for(unsigned y=0;y<64;++y)
        std::memcpy(next.pixels.data()+(size_t(i/columns*64+y)*next.width+i%columns*64)*4,
            atlas.pixels.data()+(size_t(i/oldColumns*64+y)*atlas.width+i%oldColumns*64)*4,64*4);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
        std::memcpy(next.pixels.data()+(size_t(slot/columns*64+y)*next.width+slot%columns*64+x)*4,
            texture.pixels.data()+(size_t(y*texture.height/64)*texture.width+x*texture.width/64)*4,4);
    atlas=std::move(next);return slot;
}
source1ios::SourceTexture studioAtlas(const source1ios::StudioMesh& model,const source1ios::SourceTexture& fallback,const source1ios::SourceTexture& checker,unsigned& count){
    if(model.materials.size()<=1){source1ios::SourceTexture texture;count=1;
        return !model.materials.empty()&&decodeMaterial(model.materials.front(),texture,"studio")?texture:fallback;}
    source1ios::SourceTexture atlas;count=0;
    for(unsigned slot=0;slot<std::max(size_t(1),model.materials.size());++slot){source1ios::SourceTexture texture;
        if(slot>=model.materials.size()||!decodeMaterial(model.materials[slot],texture,"studio"))texture=checker;
        appendAtlasTile(atlas,count,texture);}
    return atlas;
}
struct CollisionDelete {
    void operator()(CPhysCollide* p) const { if(p && g_pPhysicsCollision) g_pPhysicsCollision->DestroyCollide(p); }
};
using Collision = std::shared_ptr<CPhysCollide>;
struct PropCollision {Collision shape,cameraShape;Vector origin;QAngle angles,cameraAngles{0,0,0};bool phy=false;};
Collision ownedPhy(const source1ios::PhyGeometry& geometry,float scale){
    std::vector<CPhysConvex*> convexes;auto fail=[&](){for(auto* c:convexes)g_pPhysicsCollision->ConvexFree(c);return Collision{};};
    for(const auto& cloud:geometry.convexes){std::vector<Vector> points;for(const auto& p:cloud){Vector v(p[0]*scale,p[1]*scale,p[2]*scale);if(!v.IsValid()||std::abs(v.x)>65536||std::abs(v.y)>65536||std::abs(v.z)>65536)return fail();points.push_back(v);}
        if(points.size()<4)return fail();const auto origin=points.front();auto far=std::max_element(points.begin(),points.end(),[&](const Vector& a,const Vector& b){return (a-origin).LengthSqr()<(b-origin).LengthSqr();});Vector axis=*far-origin,normal;float area=0;
        for(const auto& p:points){Vector cross;CrossProduct(axis,p-origin,cross);if(cross.LengthSqr()>area){area=cross.LengthSqr();normal=cross;}}
        if(area<1e-6f)return fail();VectorNormalize(normal);float height=0;for(const auto& p:points)height=std::max(height,std::abs(DotProduct(normal,p-origin)));if(height<.01f)return fail();
        std::vector<Vector*> refs;for(auto& p:points)refs.push_back(&p);auto* convex=g_pPhysicsCollision->ConvexFromVerts(refs.data(),refs.size());if(!convex)return fail();convexes.push_back(convex);}
    if(convexes.empty())return {};
    return Collision(g_pPhysicsCollision->ConvertConvexToCollide(convexes.data(),convexes.size()),CollisionDelete{});
}
std::vector<std::uint8_t> serializePhy(Collision shape,int checksum){
    if(!shape)return {};const int n=g_pPhysicsCollision->CollideSize(shape.get());if(n<48||n>4*1024*1024)return {};
    std::vector<std::uint8_t> bytes(20+size_t(n)+1);const int header[4]={16,0,1,checksum};std::memcpy(bytes.data(),header,16);std::memcpy(bytes.data()+16,&n,4);
    if(g_pPhysicsCollision->CollideWrite(reinterpret_cast<char*>(bytes.data()+20),shape.get())!=n)return {};return bytes;
}
Collision ownedBox(const Vector& lo,const Vector& hi){
    for(int axis=0;axis<3;++axis)if(!std::isfinite(lo[axis])||!std::isfinite(hi[axis])||hi[axis]-lo[axis]<.01f||std::abs(lo[axis])>65536||std::abs(hi[axis])>65536)return {};
    auto* convex=g_pPhysicsCollision->BBoxToConvex(lo,hi);if(!convex)return {};
    return Collision(g_pPhysicsCollision->ConvertConvexToCollide(&convex,1),CollisionDelete{});
}
bool propBox(const source1ios::StudioMesh& model,const source1ios::PreviewProp& prop,PropCollision& out){
    PropCollision staged;staged.origin=Vector(prop.origin[0],prop.origin[1],prop.origin[2]);staged.angles=QAngle(prop.angles[0],prop.angles[1],prop.angles[2]);
    staged.shape=ownedBox(model.hullMins*prop.scale,model.hullMaxs*prop.scale);if(!staged.shape)return false;
    matrix3x4_t rotation;AngleMatrix(staged.angles,rotation);Vector lo,hi;TransformAABB(rotation,model.renderMins*prop.scale,model.renderMaxs*prop.scale,lo,hi);
    staged.cameraShape=ownedBox(lo,hi);if(!staged.cameraShape)return false;out=std::move(staged);return true;
}
void tracePropBoxes(const std::vector<PropCollision>& props,const Vector& start,const Vector& end,const Vector& mins,const Vector& maxs,trace_t& result){
    for(const auto& prop:props){trace_t hit{};g_pPhysicsCollision->TraceBox(start,end,mins,maxs,prop.cameraShape.get(),prop.origin,prop.cameraAngles,&hit);
        if(hit.startsolid||hit.allsolid||hit.fraction<result.fraction)result=hit;}
}
struct PhysicsScene {
    IPhysics* physics=nullptr;IPhysicsEnvironment* environment=nullptr;
    IPhysicsObject* level=nullptr;IPhysicsObject* anchor=nullptr;IPhysicsObject* body=nullptr;
    IPhysicsConstraint* joint=nullptr;Collision levelShape;
    std::vector<IPhysicsObject*> props;std::vector<PropCollision> propShapes;
    ~PhysicsScene(){
        if(environment){if(joint)environment->DestroyConstraint(joint);if(body)environment->DestroyObject(body);
            if(anchor)environment->DestroyObject(anchor);for(auto* prop:props)environment->DestroyObject(prop);if(level)environment->DestroyObject(level);physics->DestroyEnvironment(environment);}
    }
};
std::unique_ptr<PhysicsScene> physicsScene(Collision shape,const std::vector<PropCollision>& props={}){
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
    scene->propShapes=props;for(const auto& prop:scene->propShapes){auto* object=scene->environment->CreatePolyObjectStatic(prop.shape.get(),material,prop.origin,prop.angles,&params);if(!object)return nullptr;scene->props.push_back(object);}
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
constexpr int geometryLumps[]={LUMP_VERTEXES,LUMP_EDGES,LUMP_SURFEDGES,LUMP_FACES,LUMP_DISPINFO,LUMP_DISP_VERTS,LUMP_DISP_TRIS,LUMP_FACES_HDR};
constexpr size_t geometryStrides[]={sizeof(dvertex_t),sizeof(dedge_t),sizeof(int),sizeof(dface_t),sizeof(ddispinfo_t),sizeof(CDispVert),sizeof(CDispTri),sizeof(dface_t)};
constexpr size_t geometryLumpCount=sizeof(geometryLumps)/sizeof(*geometryLumps);
constexpr int materialLumps[]={LUMP_TEXINFO,LUMP_TEXDATA,LUMP_TEXDATA_STRING_TABLE,LUMP_TEXDATA_STRING_DATA,LUMP_LIGHTING,LUMP_LIGHTING_HDR};
constexpr size_t materialStrides[]={sizeof(texinfo_t),sizeof(dtexdata_t),sizeof(int),1,sizeof(ColorRGBExp32),sizeof(ColorRGBExp32)};
constexpr size_t materialLumpCount=sizeof(materialLumps)/sizeof(*materialLumps);
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
        const bool supportedVersion=l.version==0 || ((geometryLumps[i]==LUMP_FACES || geometryLumps[i]==LUMP_FACES_HDR) && l.version==LUMP_FACES_VERSION);
        if(!supportedVersion)return fail("unsupported geometry lump version",geometryLumps[i]);
    }
    for(size_t i=0;i<materialLumpCount;++i){const auto& l=h.lumps[materialLumps[i]];const size_t decoded=l.uncompressedSize?l.uncompressedSize:l.filelen;
        if(decoded>maximumLump||size_t(l.filelen)>maximumLump)return fail("material lump exceeds 16 MiB limit",materialLumps[i]);
        if(decoded%materialStrides[i])return fail("material record size mismatch",materialLumps[i]);
        if(l.version!=0 && !((materialLumps[i]==LUMP_LIGHTING || materialLumps[i]==LUMP_LIGHTING_HDR) && l.version==1))return fail("unsupported material lump version",materialLumps[i]);}
    return true;
}
struct PreviewLighting { int faces=LUMP_FACES, samples=LUMP_LIGHTING;bool hdr=false; };
PreviewLighting previewLighting(const dheader_t& h){
    // Prefer LDR when present. Like the original loader, HDR may share LDR
    // face records if the HDR face lump is absent; never mix HDR faces with LDR samples.
    PreviewLighting result;
    if(!h.lumps[LUMP_LIGHTING].filelen && h.lumps[LUMP_LIGHTING_HDR].filelen){
        result.hdr=true;result.samples=LUMP_LIGHTING_HDR;
        if(h.lumps[LUMP_FACES_HDR].filelen)result.faces=LUMP_FACES_HDR;
    }
    return result;
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
bool readProps(FileHandle_t file,const dheader_t& header,std::vector<source1ios::PreviewProp>& props){
    const auto& parent=header.lumps[LUMP_GAME_LUMP];if(!parent.filelen)return true;
    if(parent.version||parent.uncompressedSize||parent.filelen<4||parent.filelen>16*1024*1024)return false;
    g_pFullFileSystem->Seek(file,parent.fileofs,FILESYSTEM_SEEK_HEAD);int count=0;
    if(g_pFullFileSystem->Read(&count,4,file)!=4||count<0||count>64||4+size_t(count)*sizeof(dgamelump_t)>size_t(parent.filelen))return false;
    std::vector<dgamelump_t> entries(count);
    if(count&&g_pFullFileSystem->Read(entries.data(),entries.size()*sizeof(dgamelump_t),file)!=int(entries.size()*sizeof(dgamelump_t)))return false;
    bool found=false;
    for(const auto& entry:entries){if(entry.id!=0x73707270)continue;
        if(found||entry.flags||entry.filelen<0||entry.filelen>4*1024*1024||entry.fileofs<parent.fileofs+4+count*int(sizeof(dgamelump_t))
            ||size_t(entry.fileofs-parent.fileofs)>size_t(parent.filelen)||size_t(entry.filelen)>size_t(parent.filelen)-size_t(entry.fileofs-parent.fileofs))return false;
        found=true;std::vector<std::uint8_t> bytes(entry.filelen);g_pFullFileSystem->Seek(file,entry.fileofs,FILESYSTEM_SEEK_HEAD);
        if(g_pFullFileSystem->Read(bytes.data(),bytes.size(),file)!=int(bytes.size())||!source1ios::parsePreviewProps(bytes,entry.version,header.version,props))return false;
    }return true;
}
struct BspMaterials {std::vector<texinfo_t> infos;std::vector<dtexdata_t> data;std::vector<int> table;std::vector<char> strings;std::vector<std::string> names;std::vector<unsigned> slots;unsigned slotCount=0;};
bool bspMaterials(BspMaterials& out,const std::array<std::vector<unsigned char>,HEADER_LUMPS>& decoded){
    out.infos=lump<texinfo_t>(LUMP_TEXINFO,decoded[LUMP_TEXINFO]);out.data=lump<dtexdata_t>(LUMP_TEXDATA,decoded[LUMP_TEXDATA]);out.table=lump<int>(LUMP_TEXDATA_STRING_TABLE,decoded[LUMP_TEXDATA_STRING_TABLE]);out.strings=lump<char>(LUMP_TEXDATA_STRING_DATA,decoded[LUMP_TEXDATA_STRING_DATA]);
    if(out.infos.empty()||out.infos.size()>65536||out.data.empty()||out.data.size()>MAX_MAP_TEXDATA||out.table.empty()||out.table.size()>MAX_MAP_TEXDATA_STRING_TABLE||out.strings.empty())return false;
    for(const auto& info:out.infos){if(info.texdata<0||size_t(info.texdata)>=out.data.size())return false;
        for(int axis=0;axis<2;++axis)for(int component=0;component<4;++component)
            if(!std::isfinite(info.textureVecsTexelsPerWorldUnits[axis][component])||!std::isfinite(info.lightmapVecsLuxelsPerWorldUnits[axis][component]))return false;}
    for(const auto& data:out.data){if(data.nameStringTableID<0||size_t(data.nameStringTableID)>=out.table.size()||data.width<=0||data.height<=0||data.width>2048||data.height>2048)return false;
        const int offset=out.table[data.nameStringTableID];if(offset<0||size_t(offset)>=out.strings.size())return false;std::string name;
        for(size_t i=offset;i<out.strings.size()&&name.size()<240&&out.strings[i];++i)name.push_back(out.strings[i]);
        if(name.empty()||name.size()>=240||size_t(offset)+name.size()>=out.strings.size()||out.strings[size_t(offset)+name.size()]||!materialPath(name))return false;out.names.push_back(name);
    }
    out.slots.resize(out.names.size());std::vector<std::string> unique;
    for(size_t i=0;i<out.names.size();++i){auto found=std::find(unique.begin(),unique.end(),out.names[i]);if(found!=unique.end())out.slots[i]=found-unique.begin();
        else if(unique.size()<512){out.slots[i]=unique.size();unique.push_back(out.names[i]);}else {Warning("Source BSP: more than 512 distinct material names\n");return false;}}
    out.slotCount=std::max(1u,unsigned(unique.size()));return true;
}
source1ios::SourceTexture bspAtlas(const BspMaterials& materials,const source1ios::SourceTexture& fallback){
    constexpr unsigned tile=64;const unsigned columns=std::min(16u,materials.slotCount),rows=(materials.slotCount+columns-1)/columns;
    source1ios::SourceTexture atlas;atlas.width=tile*columns;atlas.height=tile*rows;atlas.pixels.resize(size_t(atlas.width)*atlas.height*4);
    std::vector<bool> written(materials.slotCount,false);
    for(size_t i=0;i<materials.names.size();++i){const unsigned slot=materials.slots[i];if(written[slot])continue;written[slot]=true;source1ios::SourceTexture decoded;
        if(!decodeMaterial({materials.names[i]},decoded,"BSP"))decoded=fallback;
        for(unsigned y=0;y<tile;++y)for(unsigned x=0;x<tile;++x){const unsigned sx=std::min(decoded.width-1,x*decoded.width/tile),sy=std::min(decoded.height-1,y*decoded.height/tile);
            std::memcpy(atlas.pixels.data()+(size_t((slot/columns)*tile+y)*atlas.width+(slot%columns)*tile+x)*4,decoded.pixels.data()+(size_t(sy)*decoded.width+sx)*4,4);}
    }return atlas;
}
bool lightmapRange(const dface_t& face,const texinfo_t& info,size_t bytes,unsigned& width,unsigned& height){
    if(face.lightofs==-1){width=height=0;return true;}
    if(face.lightofs<0 || face.lightofs%sizeof(ColorRGBExp32))return false;
    for(int i=0;i<2;++i)if(face.m_LightmapTextureSizeInLuxels[i]<0 || face.m_LightmapTextureSizeInLuxels[i]>255)return false;
    width=face.m_LightmapTextureSizeInLuxels[0]+1;height=face.m_LightmapTextureSizeInLuxels[1]+1;
    unsigned styles=0;while(styles<MAXLIGHTMAPS&&face.styles[styles]!=255)++styles;
    if(!styles)return false;
    const size_t required=size_t(width)*height*styles*((info.flags&SURF_BUMPLIGHT)?4:1)*sizeof(ColorRGBExp32);
    return size_t(face.lightofs)<=bytes && required<=bytes-size_t(face.lightofs);
}
// First static lightstyle. HDR fallback uses fixed exposure / Reinhard mapping
// into this 8-bit preview atlas, not Source eye adaptation or an HDR framebuffer.
unsigned char previewLightChannel(unsigned channel,int exponent,bool hdr){
    double linear=std::ldexp(double(channel)/255.0,exponent);
    if(hdr)linear=linear/(1.0+linear);
    return std::lround(std::pow(std::clamp(linear,0.0,1.0),1.0/2.2)*255);
}
struct LightmapAtlas {
    source1ios::SourceTexture texture{1024,1024,std::vector<std::uint8_t>(1024*1024*4,255)};
    unsigned x=0,y=0,row=0,faces=0;bool hdr=false;
    bool add(const dface_t& face,const texinfo_t& info,const std::vector<ColorRGBExp32>& samples,unsigned& ox,unsigned& oy,unsigned& w,unsigned& h){
        if(!lightmapRange(face,info,samples.size()*sizeof(ColorRGBExp32),w,h))return false;
        if(!w)return true;
        if(x+w+2>texture.width){x=0;y+=row;row=0;}
        if(y+h+2>texture.height)return false;
        ox=x+1;oy=y+1;const auto* source=samples.data()+face.lightofs/sizeof(ColorRGBExp32);
        for(unsigned sy=0;sy<h+2;++sy)for(unsigned sx=0;sx<w+2;++sx){const auto& c=source[size_t(std::min(h-1,sy?sy-1:0))*w+std::min(w-1,sx?sx-1:0)];
            auto* pixel=texture.pixels.data()+(size_t(y+sy)*texture.width+x+sx)*4;
            const unsigned channels[]={c.r,c.g,c.b};for(int k=0;k<3;++k)pixel[k]=previewLightChannel(channels[k],int(c.exponent),hdr);}
        x+=w+2;row=std::max(row,h+2);++faces;return true;
    }
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
std::vector<unsigned char> fixture(bool displaced=false,bool materialGrid=false) {
    const unsigned materialCount=materialGrid?17:2;
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
        for(unsigned f=0;f<6;++f){faces[box*6+f].planenum=box*6+facePlanes[f];faces[box*6+f].texinfo=(f/2)*materialCount+(materialGrid?(box*6+f)%materialCount:(box+f)%2);}
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
    // Each face uses axes in its own plane. XY on a vertical wall would
    // collapse one UV coordinate, turning brick/grid textures into stripes.
    std::vector<texinfo_t> infos(materialCount*3);for(unsigned i=0;i<infos.size();++i){const int s=i/materialCount==2?1:0,t=i/materialCount==0?1:2;
        infos[i].textureVecsTexelsPerWorldUnits[0][s]=infos[i].textureVecsTexelsPerWorldUnits[1][t]=1;
        infos[i].lightmapVecsLuxelsPerWorldUnits[0][s]=infos[i].lightmapVecsLuxelsPerWorldUnits[1][t]=1.f/16;infos[i].texdata=i%materialCount;}
    std::vector<dtexdata_t> tex(materialCount);for(unsigned i=0;i<materialCount;++i){tex[i].width=tex[i].height=tex[i].view_width=tex[i].view_height=64;tex[i].reflectivity=Vector(1,1,1);tex[i].nameStringTableID=i;}
    // Synthetic baked gradient stored in the real BSP lighting format. This
    // fixture exercises loading/sampling, not a radiosity/light compiler.
    std::vector<ColorRGBExp32> lighting;
    for(auto& face:faces){const auto& info=infos[face.texinfo];std::fill(std::begin(face.styles),std::end(face.styles),255);face.styles[0]=0;face.lightofs=lighting.size()*sizeof(ColorRGBExp32);
        for(int axis=0;axis<2;++axis){float minimum=1e9f,maximum=-1e9f;const auto& v=info.lightmapVecsLuxelsPerWorldUnits[axis];
            for(int edge=0;edge<face.numedges;++edge){const int se=surfedges[face.firstedge+edge];const auto& p=vertices[edges[std::abs(se)].v[se<0?1:0]].point;
                const float luxel=p.x*v[0]+p.y*v[1]+p.z*v[2]+v[3];minimum=std::min(minimum,luxel);maximum=std::max(maximum,luxel);}
            face.m_LightmapTextureMinsInLuxels[axis]=int(std::floor(minimum));face.m_LightmapTextureSizeInLuxels[axis]=int(std::ceil(maximum))-face.m_LightmapTextureMinsInLuxels[axis];}
        const unsigned w=face.m_LightmapTextureSizeInLuxels[0]+1,hgt=face.m_LightmapTextureSizeInLuxels[1]+1;
        for(unsigned y=0;y<hgt;++y)for(unsigned x=0;x<w;++x){const unsigned value=70+185*x/std::max(1u,w-1);lighting.push_back({static_cast<unsigned char>(value),static_cast<unsigned char>(value),static_cast<unsigned char>(value),0});}
    }
    append(LUMP_LIGHTING,lighting.data(),lighting.size()*sizeof(ColorRGBExp32));h.lumps[LUMP_LIGHTING].version=1;
    std::memcpy(bytes.data()+h.lumps[LUMP_FACES].fileofs,faces.data(),faces.size()*sizeof(dface_t));
    std::vector<unsigned short> leafFaces;for(unsigned i=0;i<faces.size();++i)leafFaces.push_back(i);leaves[1].numleaffaces=faces.size();
    append(LUMP_LEAFFACES,leafFaces.data(),leafFaces.size()*sizeof(unsigned short));
    darea_t areas[2]{};unsigned short leafbrush[]={0,1,2,3,4,5,6};std::vector<char> names;std::vector<int> nameIndex;
    for(unsigned i=0;i<materialCount;++i){const std::string name=i==0?"debug/debugempty":i==1?"debug/debugblue":"debug/source1ios_slot"+std::to_string(i);nameIndex.push_back(names.size());names.insert(names.end(),name.begin(),name.end());names.push_back(0);}names.push_back(0);
    const char entities[]="{ \"classname\" \"worldspawn\" }\n{ \"classname\" \"info_player_start\" \"origin\" \"-190 -160 16\" \"angles\" \"8 45 0\" }\n";
    append(LUMP_PLANES,planes.data(),planes.size()*sizeof(dplane_t));append(LUMP_BRUSHSIDES,sides.data(),sides.size()*sizeof(dbrushside_t));append(LUMP_BRUSHES,brushes.data(),brushes.size()*sizeof(dbrush_t));
    append(LUMP_LEAFS,leaves,sizeof(leaves));h.lumps[LUMP_LEAFS].version=1;append(LUMP_LEAFBRUSHES,leafbrush,sizeof(leafbrush));
    append(LUMP_NODES,&node,sizeof(node));append(LUMP_MODELS,&model,sizeof(model));append(LUMP_TEXINFO,infos.data(),infos.size()*sizeof(texinfo_t));
    append(LUMP_TEXDATA,tex.data(),tex.size()*sizeof(dtexdata_t));append(LUMP_TEXDATA_STRING_TABLE,nameIndex.data(),nameIndex.size()*sizeof(int));append(LUMP_TEXDATA_STRING_DATA,names.data(),names.size());
    append(LUMP_AREAS,areas,sizeof(areas));append(LUMP_ENTITIES,entities,sizeof(entities));
    std::memcpy(bytes.data(),&h,sizeof(h));return bytes;
}
std::vector<std::uint8_t> propFixture(unsigned solid=2){
    auto bytes=fixture();dheader_t header;std::memcpy(&header,bytes.data(),sizeof(header));
    std::vector<std::uint8_t> payload;auto append=[&](const void* p,size_t n){const auto* b=static_cast<const std::uint8_t*>(p);payload.insert(payload.end(),b,b+n);};
    int count=1;append(&count,4);char name[128]="models/__source1ios_static_probe.mdl";append(name,128);
    count=0;append(&count,4);count=2;append(&count,4);
    for(unsigned i=0;i<2;++i){std::array<std::uint8_t,56> record{};const float origin[3]={i?-140.f:-100.f,i?-90.f:-100.f,0};const float angles[3]={0,i?90.f:30.f,0};
        std::memcpy(record.data(),origin,12);std::memcpy(record.data()+12,angles,12);record[30]=solid;append(record.data(),record.size());}
    auto& lump=header.lumps[LUMP_GAME_LUMP];lump.fileofs=bytes.size();lump.filelen=4+sizeof(dgamelump_t)+payload.size();
    dgamelump_t entry{};entry.id=0x73707270;entry.version=4;entry.fileofs=lump.fileofs+4+sizeof(entry);entry.filelen=payload.size();count=1;
    const auto* p=reinterpret_cast<const std::uint8_t*>(&count);bytes.insert(bytes.end(),p,p+4);p=reinterpret_cast<const std::uint8_t*>(&entry);bytes.insert(bytes.end(),p,p+sizeof(entry));bytes.insert(bytes.end(),payload.begin(),payload.end());
    std::memcpy(bytes.data(),&header,sizeof(header));return bytes;
}
std::vector<std::uint8_t> hdrFixture(){
    auto bytes=propFixture(6);dheader_t header;std::memcpy(&header,bytes.data(),sizeof(header));
    header.lumps[LUMP_FACES_HDR]=header.lumps[LUMP_FACES];header.lumps[LUMP_FACES]={};
    header.lumps[LUMP_LIGHTING_HDR]=header.lumps[LUMP_LIGHTING];header.lumps[LUMP_LIGHTING]={};
    const auto& lighting=header.lumps[LUMP_LIGHTING_HDR];
    for(int i=0;i<lighting.filelen/int(sizeof(ColorRGBExp32));++i){ColorRGBExp32 sample;
        std::memcpy(&sample,bytes.data()+lighting.fileofs+i*sizeof(sample),sizeof(sample));sample.exponent=2;
        std::memcpy(bytes.data()+lighting.fileofs+i*sizeof(sample),&sample,sizeof(sample));}
    std::memcpy(bytes.data(),&header,sizeof(header));return bytes;
}
std::vector<std::uint8_t> skinPropsFixture(){
    auto bytes=hdrFixture();dheader_t header;std::memcpy(&header,bytes.data(),sizeof(header));
    dgamelump_t entry;std::memcpy(&entry,bytes.data()+header.lumps[LUMP_GAME_LUMP].fileofs+4,sizeof(entry));
    std::memset(bytes.data()+entry.fileofs+4,0,128);const char name[]="models/__source1ios_multimat_probe.mdl";
    std::memcpy(bytes.data()+entry.fileofs+4,name,sizeof(name));const int skin=1;
    std::memcpy(bytes.data()+entry.fileofs+140+56+32,&skin,4);return bytes;
}
std::vector<std::uint8_t> entityPropsFixture(){
    auto bytes=fixture();dheader_t header;std::memcpy(&header,bytes.data(),sizeof(header));
    const std::string text="{ classname worldspawn } { classname info_player_start origin \"-190 -160 16\" angles \"8 45 0\" } "
        "{ classname prop_dynamic model models/__source1ios_multimat_probe.mdl origin \"-100 -100 0\" angles \"0 30 0\" skin 0 } "
        "{ classname prop_dynamic_override model models/__source1ios_multimat_probe.mdl origin \"-140 -90 0\" angle 90 skin 1 modelscale 0.5 }";
    header.lumps[LUMP_ENTITIES].fileofs=bytes.size();header.lumps[LUMP_ENTITIES].filelen=text.size()+1;
    bytes.insert(bytes.end(),text.begin(),text.end());bytes.push_back(0);std::memcpy(bytes.data(),&header,sizeof(header));return bytes;
}
}
namespace source1ios {
struct SourceMap::Impl {
    StudioMesh studio;double poseTime=0;unsigned animation=0;bool animationPlaying=false;
    std::vector<MeshPoint> mesh,modelMesh;Collision collision, fixtureCollision;std::unique_ptr<PhysicsScene> scene;
    std::vector<MeshPoint> propsMesh;unsigned propInstances=0,entityModelInstances=0,entityModelCandidates=0;
    std::vector<PropCollision> propCollisions;
    std::vector<std::unique_ptr<PortCDispCollTree>> displacementCollision;
    std::vector<PreviewSpawn> spawns;Vector spawnCamera{-190,-160,80};QAngle spawnAngles{8,45,0};
    std::string builtin,builtinModel;Vector camera;QAngle angles;SourceTexture checkerTexture,texture,modelTexture,lightmapTexture;unsigned mapMaterialCount=1,modelMaterialCount=1,lightmappedFaces=0;bool hdrLighting=false;std::uint64_t textureRevision=0,modelTextureRevision=0;model_t* world=nullptr;bool builtinActive=false,builtinModelActive=false;
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
    decoded->ConvertImageFormat(IMAGE_FORMAT_RGBA8888,false);impl_->checkerTexture.width=decoded->Width();impl_->checkerTexture.height=decoded->Height();
    impl_->checkerTexture.pixels.assign(decoded->ImageData(0,0,0),decoded->ImageData(0,0,0)+64*64*4);impl_->texture=impl_->checkerTexture;++impl_->textureRevision;
    Msg("Source BSP startup: generating BSP\n");
    const auto bytes=fixture();auto file=g_pFullFileSystem->Open(impl_->builtin.c_str(),"wb","PORT_BSP_PREVIEW");
    bool ok=file && g_pFullFileSystem->Write(bytes.data(),bytes.size(),file)==int(bytes.size());if(file)g_pFullFileSystem->Close(file);
    const auto terrain=fixture(true);auto terrainFile=g_pFullFileSystem->Open("__source1ios_displacement.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok && terrainFile && g_pFullFileSystem->Write(terrain.data(),terrain.size(),terrainFile)==int(terrain.size());if(terrainFile)g_pFullFileSystem->Close(terrainFile);
    const auto materialGrid=fixture(false,true);auto gridFile=g_pFullFileSystem->Open("__source1ios_material_grid.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok&&gridFile&&g_pFullFileSystem->Write(materialGrid.data(),materialGrid.size(),gridFile)==int(materialGrid.size());if(gridFile)g_pFullFileSystem->Close(gridFile);
    const auto propBytes=propFixture();auto propFile=g_pFullFileSystem->Open("__source1ios_props.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok&&propFile&&g_pFullFileSystem->Write(propBytes.data(),propBytes.size(),propFile)==int(propBytes.size());if(propFile)g_pFullFileSystem->Close(propFile);
    const auto phyMap=propFixture(6);auto phyMapFile=g_pFullFileSystem->Open("__source1ios_phy.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok&&phyMapFile&&g_pFullFileSystem->Write(phyMap.data(),phyMap.size(),phyMapFile)==int(phyMap.size());if(phyMapFile)g_pFullFileSystem->Close(phyMapFile);
    const auto hdrMap=hdrFixture();auto hdrFile=g_pFullFileSystem->Open("__source1ios_hdr.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok&&hdrFile&&g_pFullFileSystem->Write(hdrMap.data(),hdrMap.size(),hdrFile)==int(hdrMap.size());if(hdrFile)g_pFullFileSystem->Close(hdrFile);
    const auto skinMap=skinPropsFixture();auto skinFile=g_pFullFileSystem->Open("__source1ios_skins.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok&&skinFile&&g_pFullFileSystem->Write(skinMap.data(),skinMap.size(),skinFile)==int(skinMap.size());if(skinFile)g_pFullFileSystem->Close(skinFile);
    const auto entityMap=entityPropsFixture();auto entityFile=g_pFullFileSystem->Open("__source1ios_entities.bsp","wb","PORT_BSP_PREVIEW");
    ok=ok&&entityFile&&g_pFullFileSystem->Write(entityMap.data(),entityMap.size(),entityFile)==int(entityMap.size());if(entityFile)g_pFullFileSystem->Close(entityFile);
    std::filesystem::create_directories(root/"game/models");const auto studio=makeStudioFixture();
    std::filesystem::create_directories(root/"game/materials/models/source1ios");
    auto writeModel=[&](const char* path,const std::vector<std::uint8_t>& data){auto out=g_pFullFileSystem->Open(path,"wb","DEFAULT_WRITE_PATH");const bool written=out&&g_pFullFileSystem->Write(data.data(),data.size(),out)==int(data.size());if(out)g_pFullFileSystem->Close(out);return written;};
    auto multiple=makeStudioFixture(false,true);const int multiVersion=48;std::memcpy(multiple.mdl.data()+4,&multiVersion,4);
    ok=ok&&writeModel("models/__source1ios_multimat_probe.mdl",multiple.mdl)&&writeModel("models/__source1ios_multimat_probe.vvd",multiple.vvd)&&writeModel("models/__source1ios_multimat_probe.dx90.vtx",multiple.vtx);
    ok=ok&&writeModel(impl_->builtinModel.c_str(),studio.mdl)&&writeModel("models/__source1ios_static_probe.vvd",studio.vvd)&&writeModel("models/__source1ios_static_probe.dx90.vtx",studio.vtx);
    StudioMesh phyModel;std::string phyError;PhyGeometry fixtureGeometry;
    if(parseStudioModel(studio.mdl,studio.vvd,studio.vtx,phyModel,phyError)){std::vector<std::array<float,3>> cloud;for(const auto& v:phyModel.triangles)cloud.push_back({v.position.x,v.position.y,v.position.z});fixtureGeometry.convexes.push_back(std::move(cloud));}
    const auto phyBytes=serializePhy(ownedPhy(fixtureGeometry,1),phyModel.checksum);
    ok=ok&&!phyBytes.empty()&&writeModel("models/__source1ios_static_probe.phy",phyBytes);
    ok=ok&&writeModel("models/__source1ios_multimat_probe.phy",phyBytes);
    const auto externalStudio=makeStudioFixture(true);
    ok=ok&&writeModel("models/__source1ios_external_probe.mdl",externalStudio.mdl)&&writeModel("models/__source1ios_external_probe.vvd",externalStudio.vvd)
        &&writeModel("models/__source1ios_external_probe.dx90.vtx",externalStudio.vtx)&&writeModel("models/__source1ios_external_probe.ani",externalStudio.ani);
    auto legacyStudio=studio,legacyExternal=externalStudio;const int legacyVersion=48;std::memcpy(legacyStudio.mdl.data()+4,&legacyVersion,4);std::memcpy(legacyExternal.mdl.data()+4,&legacyVersion,4);
    ok=ok&&writeModel("models/__source1ios_legacy_probe.mdl",legacyStudio.mdl)&&writeModel("models/__source1ios_legacy_probe.vvd",legacyStudio.vvd)&&writeModel("models/__source1ios_legacy_probe.dx90.vtx",legacyStudio.vtx)
        &&writeModel("models/__source1ios_external48_probe.mdl",legacyExternal.mdl)&&writeModel("models/__source1ios_external48_probe.vvd",legacyExternal.vvd)&&writeModel("models/__source1ios_external48_probe.dx90.vtx",legacyExternal.vtx);
    for(int mip=0;mip<source->MipCount();++mip){const int size=std::max(1,64>>mip);auto* pixels=source->ImageData(0,0,mip);for(int y=0;y<size;++y)for(int x=0;x<size;++x){const bool stripe=((y*64/size)/8)%2;auto* p=pixels+(y*size+x)*4;p[0]=stripe?240:20;p[1]=stripe?100:200;p[2]=stripe?30:240;p[3]=255;}}
    CUtlBuffer modelVtf;ok=ok&&source->Serialize(modelVtf);std::vector<std::uint8_t> modelVtfBytes(static_cast<std::uint8_t*>(modelVtf.Base()),static_cast<std::uint8_t*>(modelVtf.Base())+modelVtf.TellPut());
    const std::string modelVmt="VertexLitGeneric { \"$basetexture\" \"models/source1ios/__source1ios_model\" }";
    ok=ok&&writeModel("materials/models/source1ios/__source1ios_model.vtf",modelVtfBytes)&&writeModel("materials/models/source1ios/__source1ios_model.vmt",std::vector<std::uint8_t>(modelVmt.begin(),modelVmt.end()));
    for(int mip=0;mip<source->MipCount();++mip){const int size=std::max(1,64>>mip);auto* pixels=source->ImageData(0,0,mip);for(int y=0;y<size;++y)for(int x=0;x<size;++x){const int sx=x*64/size,sy=y*64/size;const bool mortar=(sx%16<2)||(sy%8<2);auto* p=pixels+(y*size+x)*4;p[0]=mortar?190:145;p[1]=mortar?180:55;p[2]=mortar?160:32;p[3]=255;}}
    CUtlBuffer bspVtf;ok=ok&&source->Serialize(bspVtf);std::vector<std::uint8_t> bspVtfBytes(static_cast<std::uint8_t*>(bspVtf.Base()),static_cast<std::uint8_t*>(bspVtf.Base())+bspVtf.TellPut());
    const std::string bspVmt="LightmappedGeneric { \"$basetexture\" \"debug/debugempty\" }";
    std::filesystem::create_directories(root/"game/materials/debug");ok=ok&&writeModel("materials/debug/debugempty.vtf",bspVtfBytes)&&writeModel("materials/debug/debugempty.vmt",std::vector<std::uint8_t>(bspVmt.begin(),bspVmt.end()));
    for(int mip=0;mip<source->MipCount();++mip){const int size=std::max(1,64>>mip);auto* pixels=source->ImageData(0,0,mip);for(int y=0;y<size;++y)for(int x=0;x<size;++x){const bool grid=((x*64/size)%16<2)||((y*64/size)%16<2);auto* p=pixels+(y*size+x)*4;p[0]=grid?90:30;p[1]=grid?150:75;p[2]=grid?210:155;p[3]=255;}}
    CUtlBuffer blueVtf;ok=ok&&source->Serialize(blueVtf);std::vector<std::uint8_t> blueVtfBytes(static_cast<std::uint8_t*>(blueVtf.Base()),static_cast<std::uint8_t*>(blueVtf.Base())+blueVtf.TellPut());
    const std::string blueVmt="Patch { include \"materials/debug/debugempty.vmt\" replace { \"$basetexture\" \"debug/debugblue\" } }";
    ok=ok&&writeModel("materials/debug/debugblue.vtf",blueVtfBytes)&&writeModel("materials/debug/debugblue.vmt",std::vector<std::uint8_t>(blueVmt.begin(),blueVmt.end()));
    const std::string altVmt="VertexLitGeneric { \"$basetexture\" \"debug/debugblue\" }";
    ok=ok&&writeModel("materials/models/source1ios/__source1ios_model_alt.vmt",std::vector<std::uint8_t>(altVmt.begin(),altVmt.end()));
    for(unsigned i=2;i<17;++i){const std::string name="materials/debug/source1ios_slot"+std::to_string(i)+".vmt";
        const std::string vmt=i%2?bspVmt:blueVmt;ok=ok&&writeModel(name.c_str(),std::vector<std::uint8_t>(vmt.begin(),vmt.end()));}
    const std::pair<const char*,const char*> patchFixtures[]={
        {"__source1ios_patch_empty","LightmappedGeneric { }"},
        {"__source1ios_patch_insert","Patch { include \"materials/debug/__source1ios_patch_empty.vmt\" insert { \"$basetexture\" \"debug/debugblue\" } replace { \"$basetexture\" \"debug/debugempty\" } }"},
        {"__source1ios_patch_noinsert","Patch { include \"materials/debug/__source1ios_patch_empty.vmt\" replace { \"$basetexture\" \"debug/debugblue\" } }"},
        {"__source1ios_patch_outer","Patch { include \"materials/debug/debugblue.vmt\" replace { \"$basetexture\" \"debug/debugempty\" } }"},
        {"__source1ios_patch_cycle","Patch { include \"materials/debug/__source1ios_patch_cycle.vmt\" }"},
        {"__source1ios_patch_unsafe","Patch { include \"materials/../../outside.vmt\" }"},
        {"__source1ios_patch_macro","#include \"outside.vmt\"\nLightmappedGeneric { }"}};
    for(const auto& entry:patchFixtures){const std::string path=std::string("materials/debug/")+entry.first+".vmt",text=entry.second;ok=ok&&writeModel(path.c_str(),std::vector<std::uint8_t>(text.begin(),text.end()));}
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
bool SourceMap::demoMaterials(){return impl_ && load("__source1ios_material_grid.bsp",nullptr);}
bool SourceMap::demoProps(){return impl_ && load("__source1ios_props.bsp",nullptr);}
bool SourceMap::demoPhy(){return impl_ && load("__source1ios_phy.bsp",nullptr);}
bool SourceMap::demoHdr(){return impl_ && load("__source1ios_hdr.bsp",nullptr);}
bool SourceMap::demoSkins(){return impl_ && load("__source1ios_skins.bsp",nullptr);}
bool SourceMap::demoEntities(){return impl_ && load("__source1ios_entities.bsp",nullptr);}
bool SourceMap::entitiesSelfTest(){
    if(!impl_)return false;
    bool all=report("live BSP entity models staged",impl_->entityModelCandidates==2&&impl_->entityModelInstances==2&&impl_->propsMesh.size()==72&&impl_->mapMaterialCount==4);
    bool slots=impl_->propsMesh.size()==72;if(slots)for(size_t i=0;i<72;++i)slots&=impl_->propsMesh[i].material==(i<36?(i<18?2u:3u):(i<54?3u:2u));
    all&=report("live BSP entity skins and visual-only physics",slots&&impl_->propCollisions.empty()&&impl_->scene&&impl_->scene->props.empty());return all;
}
bool SourceMap::setSkin(unsigned family){
    if(!impl_||impl_->modelMesh.size()!=impl_->studio.triangles.size()||!selectStudioSkin(impl_->studio,family))return false;
    for(size_t i=0;i<impl_->modelMesh.size();++i)impl_->modelMesh[i].material=impl_->studio.triangles[i].material;
    if(family!=0)impl_->builtinModelActive=false;
    Msg("Source studio skin selected: %u; geometry animation and texture atlas retained\n",family);return true;
}
bool SourceMap::skinSelfTest(){
    if(!impl_)return false;
    bool all=report("live studio skin family selected",impl_->studio.activeSkin==1&&impl_->studio.skinFamilies.size()==2);
    bool remapped=impl_->modelMesh.size()==36&&impl_->modelMaterialCount==2;
    if(remapped)for(size_t i=0;i<36;++i)remapped&=impl_->modelMesh[i].material==(i<18?0u:1u);
    all&=report("live studio skin changes per-mesh texture assignment",remapped);
    StudioPose pose;std::vector<StudioVertex> output;bool retained=sampleStudioAnimation(impl_->studio,0,.375,pose)&&skinStudioModel(impl_->studio,pose.rotations,output,pose.positions)&&output.size()==impl_->modelMesh.size();
    if(retained)for(size_t i=0;i<output.size();++i)retained&=output[i].material==impl_->modelMesh[i].material;
    all&=report("live studio skin variant preserves weighted animation",retained);return all;
}
bool SourceMap::propsSkinSelfTest(){
    if(!impl_)return false;
    bool all=report("live static props different skins same model",impl_->propInstances==2&&impl_->propsMesh.size()==72);
    bool remapped=impl_->propsMesh.size()==72&&impl_->mapMaterialCount==4;
    if(remapped)for(size_t i=0;i<72;++i)remapped&=impl_->propsMesh[i].material==(i<36?(i<18?3u:2u):(i<54?2u:3u));
    all&=report("live static prop skin families use distinct texture assignments",remapped);
    all&=report("live static prop skin variants retain PHY objects",impl_->propCollisions.size()==2&&impl_->scene&&impl_->scene->props.size()==2&&impl_->propCollisions[0].phy&&impl_->propCollisions[1].phy);return all;
}
bool SourceMap::hdrSelfTest(){
    const bool pass=impl_&&impl_->hdrLighting&&impl_->lightmappedFaces==42&&impl_->propCollisions.size()==2
        &&impl_->mesh.size()==252&&impl_->mesh[0].lightmap[2]==1;
    Msg("Source BSP self-test live HDR-only faces lighting and PHY scene: %s\n",pass?"PASS":"FAIL");return pass;
}
bool SourceMap::materialsSelfTest(){
    if(!impl_)return false;
    bool slots[2]{};for(const auto& v:impl_->modelMesh)if(v.material<2)slots[v.material]=true;
    const auto& texture=impl_->modelTexture;
    bool all=report("live studio per-mesh material slots",impl_->modelMaterialCount==2&&slots[0]&&slots[1]&&impl_->studio.meshes==2);
    all&=report("live studio two VMT VTF atlas tiles",texture.width==128&&texture.height==64&&texture.pixels.size()==128*64*4&&std::memcmp(texture.pixels.data(),texture.pixels.data()+64*4,64*4)!=0);
    StudioPose pose;std::vector<StudioVertex> skinned;bool preserved=sampleStudioAnimation(impl_->studio,0,.375,pose)&&skinStudioModel(impl_->studio,pose.rotations,skinned,pose.positions)&&skinned.size()==impl_->modelMesh.size();
    if(preserved)for(size_t i=0;i<skinned.size();++i)preserved&=skinned[i].material==impl_->modelMesh[i].material;
    all&=report("live studio skinning preserves material slots",preserved);return all;
}
bool SourceMap::propsSelfTest(){
    if(!impl_)return false;bool all=report("live static prop vphysics objects",impl_->propCollisions.size()==2&&impl_->scene&&impl_->scene->props.size()==2);
    if(impl_->propCollisions.empty())return false;const auto& prop=impl_->propCollisions[0];trace_t ray{},hull{};ray.fraction=hull.fraction=1;
    const auto start=prop.origin+Vector(80,0,32),end=prop.origin+Vector(-80,0,32);
    tracePropBoxes({prop},start,end,Vector(0,0,0),Vector(0,0,0),ray);tracePropBoxes({prop},start,end,Vector(-8,-8,-24),Vector(8,8,8),hull);
    all&=report("live static prop ray and swept hull",!ray.startsolid&&ray.fraction>0&&ray.fraction<.5f&&!hull.startsolid&&hull.fraction<ray.fraction);
    const auto savedCamera=impl_->camera;const auto savedAngles=impl_->angles;impl_->camera=prop.origin+Vector(-80,-15,80);impl_->angles=QAngle(0,0,0);
    for(unsigned i=0;i<20;++i)move(1,0,.1f);const float distance=prop.origin.x-impl_->camera.x;const auto stopped=impl_->camera;for(unsigned i=0;i<20;++i)move(1,0,.1f);
    const float drift=(impl_->camera-stopped).Length();const bool blocked=distance>16&&distance<40&&drift<.05f;
    if(!blocked)Warning("Source static prop camera check: distance %.3f, drift %.3f\n",distance,drift);impl_->camera=savedCamera;impl_->angles=savedAngles;
    all&=report("live static prop blocks camera without tunneling",blocked);return all;
}
bool SourceMap::phySelfTest(){
    if(!impl_)return false;bool all=report("live PHY static vphysics objects",impl_->propCollisions.size()==2&&impl_->scene&&impl_->scene->props.size()==2&&impl_->propCollisions.front().phy);
    if(impl_->propCollisions.empty())return false;const auto& prop=impl_->propCollisions.front();matrix3x4_t transform;AngleMatrix(prop.angles,prop.origin,transform);
    auto ray=[&](float x,trace_t& hit){Vector start,end;VectorTransform(Vector(x,-32,60),transform,start);VectorTransform(Vector(x,32,60),transform,end);hit.fraction=1;tracePropBoxes({prop},start,end,Vector(0,0,0),Vector(0,0,0),hit);};
    trace_t center{},outside{};ray(0,center);ray(10,outside);all&=report("live PHY tapered shape differs from bounding box",center.fraction>0&&center.fraction<.5f&&outside.fraction==1&&!outside.startsolid);
    const auto savedCamera=impl_->camera;const auto savedAngles=impl_->angles;impl_->camera=prop.origin+Vector(-80,-8,80);impl_->angles=QAngle(0,0,0);
    for(unsigned i=0;i<20;++i)move(1,0,.1f);const float distance=prop.origin.x-impl_->camera.x;const auto stopped=impl_->camera;for(unsigned i=0;i<20;++i)move(1,0,.1f);
    const bool blocked=distance>8&&distance<30&&(impl_->camera-stopped).Length()<.05f;if(!blocked)Warning("Source PHY camera check distance %.3f drift %.3f\n",distance,(impl_->camera-stopped).Length());impl_->camera=savedCamera;impl_->angles=savedAngles;
    all&=report("live PHY swept camera blocks without tunneling",blocked);return all;
}
bool SourceMap::resetModel(){return impl_&&loadModel(impl_->builtinModel.c_str());}
bool SourceMap::loadModel(const char* filename,const char* pathID){
    if(!impl_||!filename||std::strlen(filename)>=MAX_PATH)return false;std::string mdlPath=filename;
    if(mdlPath.size()<5||mdlPath.substr(mdlPath.size()-4)!=".mdl"){Warning("Source studio: expected .mdl path: %s\n",filename);return false;}
    const auto base=mdlPath.substr(0,mdlPath.size()-4);auto read=[&](const std::string& path,std::vector<std::uint8_t>& bytes){return readBounded(path.c_str(),pathID,32*1024*1024,bytes);};
    std::vector<std::uint8_t> mdl,vvd,vtx;if(!read(mdlPath,mdl)||!read(base+".vvd",vvd)||!read(base+".dx90.vtx",vtx)){Warning("Source studio: missing MDL/VVD/DX90.VTX companion for %s\n",filename);return false;}
    StudioMesh parsed;std::string error;if(!parseStudioModel(mdl,vvd,vtx,parsed,error)){Warning("Source studio rejected %s: %s\n",filename,error.c_str());return false;}
    if(!parsed.animationPath.empty()){
        const auto& name=parsed.animationPath;
        if(!materialPath(name)||name.size()<5||name.substr(name.size()-4)!=".ani"){Warning("Source studio rejected %s: unsafe ANI path\n",filename);return false;}
        const size_t slash=mdlPath.find_last_of('/');const std::string aniPath=name.find('/')==std::string::npos?(slash==std::string::npos?std::string():mdlPath.substr(0,slash+1))+name:name;
        std::vector<std::uint8_t> ani;
        if(g_pFullFileSystem->FileExists(aniPath.c_str(),pathID)){
            if(!read(aniPath,ani)||ani.empty()||!parseStudioModel(mdl,vvd,vtx,parsed,error,ani)){Warning("Source studio rejected %s: invalid ANI companion (%s)\n",filename,error.c_str());return false;}
            Msg("Source studio external ANI loaded: %s; %zu bytes\n",aniPath.c_str(),ani.size());
        }else Msg("Source studio external ANI missing: %s; available embedded clips/bind pose retained\n",aniPath.c_str());
    }
    unsigned modelMaterials=0;SourceTexture modelTexture=studioAtlas(parsed,impl_->texture,impl_->checkerTexture,modelMaterials);
    std::vector<MeshPoint> staged;staged.reserve(parsed.triangles.size());const Vector origin(0,64,0);
    for(const auto& v:parsed.triangles){const float light=.35f+.65f*std::abs(v.normal.z*.8f+v.normal.x*.3f+v.normal.y*.2f);staged.push_back({v.position+origin,{light,light,light},{v.uv.x,v.uv.y},v.material});}
    impl_->modelTexture=std::move(modelTexture);impl_->modelMaterialCount=modelMaterials;++impl_->modelTextureRevision;
    Msg("Source studio material atlas ready: %u slots, %ux%u RGBA\n",modelMaterials,impl_->modelTexture.width,impl_->modelTexture.height);
    impl_->modelMesh=std::move(staged);impl_->studio=std::move(parsed);impl_->poseTime=0;impl_->animation=0;impl_->animationPlaying=!impl_->studio.animations.empty();
    impl_->builtinModelActive=mdlPath==impl_->builtinModel;
    Msg("Source studio model loaded: %u source vertices, %zu triangles, %u meshes from %s\n",impl_->studio.sourceVertices,impl_->studio.triangles.size()/3,impl_->studio.meshes,filename);
    int loadedVersion=0;std::memcpy(&loadedVersion,mdl.data()+4,4);Msg("Source studio MDL version: %d\n",loadedVersion);
    Msg("Source studio skeleton: %zu bones; weighted CPU skinning; %zu animation clips\n",impl_->studio.bones.size(),impl_->studio.animations.size());
    for(size_t i=0;i<impl_->studio.animations.size();++i){const auto& clip=impl_->studio.animations[i];Msg("Source studio clip %zu: %s; %zu frames at %.2f fps\n",i,clip.name.c_str(),clip.frames.size(),clip.fps);}return true;
}
bool SourceMap::playAnimation(unsigned index){if(!impl_||index>=impl_->studio.animations.size())return false;impl_->animation=index;impl_->poseTime=0;impl_->animationPlaying=true;Msg("Source studio animation selected: %u\n",index);return true;}
bool SourceMap::setAnimationPlaying(bool playing){if(!impl_||impl_->studio.animations.empty())return false;impl_->animationPlaying=playing;Msg("Source studio animation %s\n",playing?"resumed":"paused");return true;}
bool SourceMap::load(const char* filename,const char* pathID) {
    if(!impl_ || !filename || std::strlen(filename)>=MAX_PATH || CMapLoadHelper::GetRefCount()!=0)return false;
    auto file=g_pFullFileSystem->Open(filename,"rb",pathID);if(!file){Warning("Source BSP: file not found: %s\n",filename);return false;}
    const auto size=g_pFullFileSystem->Size(file);dheader_t h{};
    const bool read=g_pFullFileSystem->Read(&h,sizeof(h),file)==sizeof(h);
    if(!read || !headerValid(h,size,filename)){if(!read)Warning("Source BSP rejected %s: truncated BSP header (%u bytes)\n",filename,size);g_pFullFileSystem->Close(file);return false;}
    std::array<std::vector<unsigned char>,HEADER_LUMPS> decoded;
    size_t decodedTotal=0;
    // Material lumps must use the same bounded decoder as geometry. Passing
    // compressed materials to CMapLoadHelper invokes legacy Uncompress().
    for(size_t i=0;i<geometryLumpCount+materialLumpCount;++i){const int id=i<geometryLumpCount?geometryLumps[i]:materialLumps[i-geometryLumpCount];const auto& l=h.lumps[id];
        const size_t decodedSize=l.uncompressedSize?l.uncompressedSize:l.filelen;
        if(decodedSize>64u*1024*1024-decodedTotal){Warning("Source BSP rejected %s: decoded lump budget exceeds 64 MiB\n",filename);g_pFullFileSystem->Close(file);return false;}decodedTotal+=decodedSize;
        if(!l.uncompressedSize)continue;
        std::vector<unsigned char> raw(l.filelen);g_pFullFileSystem->Seek(file,l.fileofs,FILESYSTEM_SEEK_HEAD);
        const char* reason="short compressed lump read";
        if(g_pFullFileSystem->Read(raw.data(),raw.size(),file)!=int(raw.size()) || !decodeLump(raw,l.uncompressedSize,decoded[id],reason)){
            Warning("Source BSP rejected %s: %s; lump=%d\n",filename,reason,id);g_pFullFileSystem->Close(file);return false;
        }
        Msg("Source BSP LZMA lump %d decoded: %d -> %zu bytes\n",id,l.filelen,decoded[id].size());
    }
    const auto& entityLump=h.lumps[LUMP_ENTITIES];
    const size_t entitySize=entityLump.uncompressedSize?entityLump.uncompressedSize:entityLump.filelen;
    if(entityLump.version!=0||entitySize>1024*1024||entityLump.filelen>1024*1024){g_pFullFileSystem->Close(file);Warning("Source BSP: entity lump exceeds supported bounds\n");return false;}
    std::vector<unsigned char> entityBytes(entityLump.filelen);g_pFullFileSystem->Seek(file,entityLump.fileofs,FILESYSTEM_SEEK_HEAD);
    if(!entityBytes.empty()&&g_pFullFileSystem->Read(entityBytes.data(),entityBytes.size(),file)!=int(entityBytes.size())){g_pFullFileSystem->Close(file);return false;}
    if(entityLump.uncompressedSize){std::vector<unsigned char> expanded;const char* reason="";
        if(!decodeLump(entityBytes,entitySize,expanded,reason)){g_pFullFileSystem->Close(file);return false;}entityBytes=std::move(expanded);}
    std::vector<PreviewSpawn> spawns;std::vector<PreviewProp> entityModels;
    if(!parsePreviewSpawns(std::string(entityBytes.begin(),entityBytes.end()),spawns,&entityModels)){g_pFullFileSystem->Close(file);Warning("Source BSP: malformed entity text, player spawn or model\n");return false;}
    std::vector<PreviewProp> props;if(!readProps(file,h,props)){g_pFullFileSystem->Close(file);Warning("Source BSP: invalid or unsupported static prop lump\n");return false;}
    const size_t staticPropCount=props.size();props.insert(props.end(),entityModels.begin(),entityModels.end());
    g_pFullFileSystem->Close(file);
    // External .lmp overlays bypass the validated on-disk header. Reject them.
    char overlay[MAX_PATH];V_StripExtension(filename,overlay,sizeof(overlay));V_strncat(overlay,"_l_0.lmp",sizeof(overlay));
    if(g_pFullFileSystem->FileExists(overlay,pathID)){Warning("Source BSP: external lump overlays unsupported\n");return false;}
    const auto selectedLighting=previewLighting(h);
    std::vector<MeshPoint> mesh;SourceTexture stagedMapTexture=impl_->checkerTexture;unsigned stagedMaterialCount=1;LightmapAtlas lightmaps;lightmaps.hdr=selectedLighting.hdr;
    std::vector<std::unique_ptr<PortCDispCollTree>> displacementCollision;
    {
        LoaderScope scope(filename);
        auto points=lump<dvertex_t>(LUMP_VERTEXES,decoded[LUMP_VERTEXES]);auto edges=lump<dedge_t>(LUMP_EDGES,decoded[LUMP_EDGES]);
        auto surfedges=lump<int>(LUMP_SURFEDGES,decoded[LUMP_SURFEDGES]);auto faces=lump<dface_t>(selectedLighting.faces,decoded[selectedLighting.faces]);
        auto disps=lump<ddispinfo_t>(LUMP_DISPINFO,decoded[LUMP_DISPINFO]);auto dispVerts=lump<CDispVert>(LUMP_DISP_VERTS,decoded[LUMP_DISP_VERTS]);auto dispTris=lump<CDispTri>(LUMP_DISP_TRIS,decoded[LUMP_DISP_TRIS]);
        BspMaterials materials;if(!bspMaterials(materials,decoded))return false;
        const auto lighting=lump<ColorRGBExp32>(selectedLighting.samples,decoded[selectedLighting.samples]);
        stagedMapTexture=bspAtlas(materials,impl_->checkerTexture);stagedMaterialCount=materials.slotCount;
        Msg("Source BSP material atlas ready: %u slots, %ux%u RGBA\n",materials.slotCount,stagedMapTexture.width,stagedMapTexture.height);
        for(const auto& p:points)if(!p.point.IsValid() || std::abs(p.point.x)>32768 || std::abs(p.point.y)>32768 || std::abs(p.point.z)>32768)return false;
        for(size_t face=0;face<faces.size();++face){const auto& f=faces[face];
            if(f.numedges<3 || f.numedges>256 || f.firstedge<0 || size_t(f.firstedge)>surfedges.size() || size_t(f.numedges)>surfedges.size()-size_t(f.firstedge))return false;
            if(f.texinfo<0||size_t(f.texinfo)>=materials.infos.size())return false;const auto& texinfo=materials.infos[f.texinfo];const auto& texdata=materials.data[texinfo.texdata];
            unsigned lx=0,ly=0,lw=0,lh=0;if(!lightmaps.add(f,texinfo,lighting,lx,ly,lw,lh)){Warning("Source BSP rejected %s: invalid lightmap range or atlas capacity; face=%zu\n",filename,face);return false;}
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
                    const auto& s=texinfo.textureVecsTexelsPerWorldUnits[0];const auto& t=texinfo.textureVecsTexelsPerWorldUnits[1];
                    const float tx=(p.x*s[0]+p.y*s[1]+p.z*s[2]+s[3])/texdata.width;
                    const float ty=(p.x*t[0]+p.y*t[1]+p.z*t[2]+t[3])/texdata.height;
                    if(!std::isfinite(tx)||!std::isfinite(ty)||std::abs(tx)>65536||std::abs(ty)>65536)return false;
                    MeshPoint vertex{p,{palette[face%6][0]*shade,palette[face%6][1]*shade,palette[face%6][2]*shade},{tx,ty},materials.slots[texinfo.texdata]};
                    if(lw){for(int axis=0;axis<2;++axis){const auto& v=texinfo.lightmapVecsLuxelsPerWorldUnits[axis];const float local=p.x*v[0]+p.y*v[1]+p.z*v[2]+v[3]-f.m_LightmapTextureMinsInLuxels[axis];
                            if(!std::isfinite(local))return false;const unsigned extent=axis?lh:lw,origin=axis?ly:lx;vertex.lightmap[axis]=(origin+.5f+std::clamp(local,0.f,float(extent-1)))/1024.f;}
                        vertex.lightmap[2]=1;for(float& color:vertex.color)color=1;}
                    mesh.push_back(vertex);
                }
            }
        }
    }
    if(mesh.empty())return false;
    std::vector<MeshPoint> propsMesh;unsigned propInstances=0,skipped=0,collisionSkipped=0;std::vector<PropCollision> propCollisions;
    struct CachedProp {std::string name;StudioMesh mesh;PhyGeometry phy;std::vector<std::pair<float,Collision>> shapes;std::vector<unsigned> slots;bool valid=false,phyRead=false;};std::vector<CachedProp> cached;
    size_t cachedVertices=0,modelBytes=0,phyPoints=0,propIndex=0;unsigned phyInstances=0,entityInstances=0;
    for(const auto& prop:props){
        const bool entityModel=propIndex++>=staticPropCount;
        auto found=std::find_if(cached.begin(),cached.end(),[&](const auto& c){return c.name==prop.model;});
        if(found==cached.end()){
            if(cached.size()>=128){Warning("Source BSP: static prop model budget exceeded\n");return false;}
            CachedProp candidate;candidate.name=prop.model;const std::string base=prop.model.substr(0,prop.model.size()-4);
            std::vector<std::uint8_t> mdl,vvd,vtx;std::string reason;
            auto readModel=[&](const std::string& name,std::vector<std::uint8_t>& data){if(!readBounded(name.c_str(),"GAME",32*1024*1024,data)||data.size()>64*1024*1024-modelBytes)return false;modelBytes+=data.size();return true;};
            if(readModel(prop.model,mdl)&&readModel(base+".vvd",vvd)&&readModel(base+".dx90.vtx",vtx)&&parseStudioModel(mdl,vvd,vtx,candidate.mesh,reason)){
                candidate.mesh.animations.clear();candidate.mesh.bones.clear();
                if(candidate.mesh.triangles.size()>maximumVertices-cachedVertices||stagedMaterialCount>=512)return false;
                cachedVertices+=candidate.mesh.triangles.size();
                const size_t slots=std::max(size_t(1),candidate.mesh.materials.size());if(slots>512-stagedMaterialCount)return false;
                for(size_t slot=0;slot<slots;++slot){SourceTexture texture;if(slot>=candidate.mesh.materials.size()||!decodeMaterial(candidate.mesh.materials[slot],texture,"static prop"))texture=impl_->checkerTexture;
                    candidate.slots.push_back(appendAtlasTile(stagedMapTexture,stagedMaterialCount,texture));}candidate.valid=true;
            }else Warning("Source BSP static prop model unavailable: %s (%s)\n",prop.model.c_str(),reason.c_str());
            cached.push_back(std::move(candidate));found=cached.end()-1;
        }
        if(!found->valid){++skipped;continue;}
        if(!selectStudioSkin(found->mesh,unsigned(prop.skin))){Warning("Source BSP static prop skin rejected: %s; skin=%d\n",prop.model.c_str(),prop.skin);return false;}
        if(found->mesh.triangles.size()>maximumVertices-mesh.size()-propsMesh.size())return false;
        matrix3x4_t transform;AngleMatrix(QAngle(prop.angles[0],prop.angles[1],prop.angles[2]),Vector(prop.origin[0],prop.origin[1],prop.origin[2]),transform);
        for(const auto& v:found->mesh.triangles){Vector position,normal;VectorTransform(v.position*prop.scale,transform,position);VectorRotate(v.normal,transform,normal);
            if(!position.IsValid()||std::abs(position.x)>65536||std::abs(position.y)>65536||std::abs(position.z)>65536)return false;
            if(v.material>=found->slots.size())return false;
            const float light=.35f+.65f*std::abs(normal.z*.8f+normal.x*.3f+normal.y*.2f);propsMesh.push_back({position,{light,light,light},{v.uv.x,v.uv.y},found->slots[v.material]});}
        ++propInstances;
        if(entityModel)++entityInstances;
        if(prop.solid==2){if(propCollisions.size()>=512){Warning("Source BSP: static prop collision budget exceeded\n");return false;}PropCollision collider;
            if(propBox(found->mesh,prop,collider))propCollisions.push_back(std::move(collider));else{++collisionSkipped;Warning("Source BSP static prop collision skipped: invalid or degenerate bounds (%s)\n",prop.model.c_str());}}
        else if(prop.solid==6){
            if(!found->phyRead){found->phyRead=true;const std::string path=prop.model.substr(0,prop.model.size()-4)+".phy";
                if(g_pFullFileSystem->FileExists(path.c_str(),"GAME")){std::vector<std::uint8_t> data;std::string reason;
                    if(!readBounded(path.c_str(),"GAME",16*1024*1024,data)||data.size()>64*1024*1024-modelBytes||!parsePhy(data,found->mesh.checksum,found->phy,reason)){Warning("Source BSP PHY rejected %s: %s\n",path.c_str(),reason.c_str());return false;}modelBytes+=data.size();
                    Msg("Source BSP PHY decoded: %s; %zu convex pieces\n",path.c_str(),found->phy.convexes.size());}}
            if(found->phy.convexes.empty()){++collisionSkipped;Warning("Source BSP static prop PHY missing: %s\n",prop.model.c_str());continue;}
            if(propCollisions.size()>=512)return false;auto cachedShape=std::find_if(found->shapes.begin(),found->shapes.end(),[&](const auto& s){return s.first==prop.scale;});
            if(cachedShape==found->shapes.end()){for(const auto& cloud:found->phy.convexes){if(cloud.size()>262144-phyPoints)return false;phyPoints+=cloud.size();}auto shape=ownedPhy(found->phy,prop.scale);if(!shape)return false;found->shapes.push_back({prop.scale,shape});cachedShape=found->shapes.end()-1;}
            PropCollision collider;collider.shape=collider.cameraShape=cachedShape->second;collider.origin=Vector(prop.origin[0],prop.origin[1],prop.origin[2]);collider.angles=collider.cameraAngles=QAngle(prop.angles[0],prop.angles[1],prop.angles[2]);collider.phy=true;propCollisions.push_back(std::move(collider));++phyInstances;}
    }
    auto* soup=g_pPhysicsCollision->PolysoupCreate();if(!soup)return false;
    for(size_t i=0;i<mesh.size();i+=3)g_pPhysicsCollision->PolysoupAddTriangle(soup,mesh[i].position,mesh[i+1].position,mesh[i+2].position,0);
    Collision collision(g_pPhysicsCollision->ConvertPolysoupToCollide(soup,false),CollisionDelete{});g_pPhysicsCollision->PolysoupDestroy(soup);if(!collision)return false;
    auto live=physicsScene(collision,propCollisions);if(!live)return false;
    impl_->spawns=std::move(spawns);impl_->spawnCamera=Vector(-190,-160,80);impl_->spawnAngles=QAngle(8,45,0);
    if(!impl_->spawns.empty()){
        auto rank=[](const PreviewSpawn& spawn){return spawn.classname=="info_player_start"?0:spawn.classname=="info_player_counterterrorist"?1:spawn.classname=="info_player_terrorist"?2:3;};
        const auto& spawn=*std::min_element(impl_->spawns.begin(),impl_->spawns.end(),[&](const auto& a,const auto& b){return rank(a)<rank(b);});
        impl_->spawnCamera=Vector(spawn.origin[0],spawn.origin[1],spawn.origin[2]+64);
        impl_->spawnAngles=QAngle(std::clamp(std::remainder(spawn.angles[0],360.f),-85.f,85.f),std::remainder(spawn.angles[1],360.f),0);
        Msg("Source BSP camera spawn: %s; eye %.2f %.2f %.2f; angles %.2f %.2f; %zu candidates\n",spawn.classname.c_str(),impl_->spawnCamera.x,impl_->spawnCamera.y,impl_->spawnCamera.z,impl_->spawnAngles.x,impl_->spawnAngles.y,impl_->spawns.size());
    }else Msg("Source BSP camera spawn: no player start; preview fallback\n");
    impl_->scene=std::move(live);
    impl_->propsMesh=std::move(propsMesh);impl_->propInstances=propInstances;
    impl_->entityModelCandidates=entityModels.size();impl_->entityModelInstances=entityInstances;
    Msg("Source BSP entity models staged: %u instances from %zu candidates; bind pose, visual only\n",entityInstances,entityModels.size());
    impl_->propCollisions=std::move(propCollisions);Msg("Source BSP static prop collision ready: %zu SOLID_BBOX objects, %u unsupported/degenerate\n",impl_->propCollisions.size()-phyInstances,collisionSkipped);
    if(phyInstances)Msg("Source BSP exact PHY collision ready: %u SOLID_VPHYSICS objects\n",phyInstances);
    Msg("Source BSP static props staged: %u instances, %zu triangles, %zu model types, %u skipped\n",propInstances,impl_->propsMesh.size()/3,cached.size(),skipped);
    impl_->displacementCollision=std::move(displacementCollision);
    impl_->lightmapTexture=std::move(lightmaps.texture);impl_->lightmappedFaces=lightmaps.faces;impl_->hdrLighting=selectedLighting.hdr;
    Msg("Source BSP %s lightmap atlas ready: %u faces, 1024x1024 RGBA\n",impl_->hdrLighting?"HDR preview":"LDR",impl_->lightmappedFaces);
    impl_->mesh=std::move(mesh);impl_->collision=std::move(collision);impl_->texture=std::move(stagedMapTexture);impl_->mapMaterialCount=stagedMaterialCount;++impl_->textureRevision;impl_->builtinActive=impl_->builtin==filename;resetCamera();
    Msg("Source BSP polygons loaded: %zu triangles from %s\n",impl_->mesh.size()/3,filename);return true;
}
void SourceMap::resetCamera(){if(impl_){impl_->camera=impl_->spawnCamera;impl_->angles=impl_->spawnAngles;}}
bool SourceMap::resetPhysics(){
    if(!impl_)return false;auto scene=physicsScene(impl_->collision,impl_->propCollisions);if(!scene)return false;
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
    tracePropBoxes(impl_->propCollisions,impl_->camera,impl_->camera+delta,Vector(-8,-8,-24),Vector(8,8,8),trace);
    if(!trace.startsolid)impl_->camera+=delta*std::max(0.f,trace.fraction-.001f);
}
void SourceMap::frame(float seconds){if(!impl_||seconds<=0||!std::isfinite(seconds))return;seconds=std::min(seconds,.05f);
    if(impl_->scene)impl_->scene->environment->Simulate(seconds);
    if(impl_->animationPlaying){impl_->poseTime+=seconds;StudioPose pose;std::vector<StudioVertex> posed;
        if(sampleStudioAnimation(impl_->studio,impl_->animation,impl_->poseTime,pose)&&skinStudioModel(impl_->studio,pose.rotations,posed,pose.positions)){for(size_t i=0;i<posed.size();++i){impl_->modelMesh[i].position=posed[i].position+Vector(0,64,0);const auto& n=posed[i].normal;const float light=.35f+.65f*std::abs(n.z*.8f+n.x*.3f+n.y*.2f);for(float& channel:impl_->modelMesh[i].color)channel=light;}}}
}
std::vector<SourceVertex> SourceMap::vertices(float aspect) const {
    std::vector<SourceVertex> out;if(!impl_)return out;out.reserve(impl_->mesh.size()+impl_->modelMesh.size());Vector f,r,u;AngleVectors(impl_->angles,&f,&r,&u);
    const float a=std::max(aspect,.01f),scale=1.3f,near=1,far=8192;
    auto append=[&](const MeshPoint& v,bool model=false){Vector relative=v.position-impl_->camera;float depth=DotProduct(relative,f);
        out.push_back({{DotProduct(relative,r)*scale/a,DotProduct(relative,u)*scale,depth*far/(far-near)-near*far/(far-near),depth},{v.color[0],v.color[1],v.color[2],1.f},{v.uv[0],v.uv[1]},{model?-float(v.material+1):float(v.material),float(model?impl_->modelMaterialCount:impl_->mapMaterialCount)},{v.lightmap[0],v.lightmap[1],v.lightmap[2],0}});};
    for(const auto& v:impl_->mesh)append(v);
    for(const auto& v:impl_->propsMesh)append(v);
    for(const auto& v:impl_->modelMesh)append(v,true);
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
std::uint64_t SourceMap::textureRevision() const { return impl_?impl_->textureRevision:0; }
const SourceTexture& SourceMap::lightmapTexture() const { static const SourceTexture empty;return impl_?impl_->lightmapTexture:empty; }
const SourceTexture& SourceMap::modelTexture() const { static const SourceTexture empty;return impl_?impl_->modelTexture:empty; }
std::uint64_t SourceMap::modelTextureRevision() const { return impl_?impl_->modelTextureRevision:0; }
bool SourceMap::selfTest(){
    if(!impl_)return false;
    if(!impl_->builtinActive || !impl_->builtinModelActive){
        // Fixture contracts belong to an isolated preview, not the imported
        // scene. Keep its physics, camera, animation, textures and revisions intact.
        Msg("Source BSP self-test: using isolated fixture; active scene preserved\n");
        SourceMap probe;probe.impl_=std::make_unique<Impl>();probe.impl_->builtin=impl_->builtin;probe.impl_->builtinModel=impl_->builtinModel;
        probe.impl_->checkerTexture=impl_->checkerTexture;probe.impl_->fixtureCollision=impl_->fixtureCollision;probe.impl_->world=impl_->world;
        return probe.load(probe.impl_->builtin.c_str(),nullptr)&&probe.loadModel(probe.impl_->builtinModel.c_str())&&probe.selfTest();
    }
    bool all=report("original lump geometry",!impl_->mesh.empty()&&CMapLoadHelper::GetRefCount()==0);
    bool materialSlots=impl_->mapMaterialCount==2&&impl_->texture.width==128&&impl_->texture.height==64&&impl_->texture.pixels.size()==128*64*4;
    all&=report("BSP texinfo multi-material VMT VTF atlas",materialSlots&&impl_->texture.pixels!=impl_->checkerTexture.pixels);
    all&=report("BSP material atlas slots remain distinct",materialSlots&&std::memcmp(impl_->texture.pixels.data(),impl_->texture.pixels.data()+64*4,64*4));
    const auto propBytes=propFixture();dheader_t propHeader;std::memcpy(&propHeader,propBytes.data(),sizeof(propHeader));dgamelump_t propEntry;std::memcpy(&propEntry,propBytes.data()+propHeader.lumps[LUMP_GAME_LUMP].fileofs+4,sizeof(propEntry));
    std::vector<std::uint8_t> propPayload(propBytes.begin()+propEntry.fileofs,propBytes.begin()+propEntry.fileofs+propEntry.filelen);std::vector<PreviewProp> parsedProps;
    all&=report("BSP static prop dictionary and instances",parsePreviewProps(propPayload,4,propHeader.version,parsedProps)&&parsedProps.size()==2&&parsedProps[0].origin[0]==-100&&parsedProps[1].angles[1]==90);
    bool propVersions=true;for(unsigned version=4;version<=11;++version){const unsigned stride=version==4?56:version==5?60:version==6?64:version<=8?68:version<=10?72:80;std::vector<std::uint8_t> upgraded(propPayload.begin(),propPayload.begin()+140);upgraded.resize(140+2*stride);for(unsigned i=0;i<2;++i){std::memcpy(upgraded.data()+140+i*stride,propPayload.data()+140+i*56,56);if(version==11){float scale=1.5f;std::memcpy(upgraded.data()+140+i*stride+76,&scale,4);}}propVersions&=parsePreviewProps(upgraded,version,20,parsedProps)&&parsedProps.size()==2;}
    all&=report("BSP static prop versions 4 through 11",propVersions);
    auto badProps=propPayload;std::uint16_t invalidPropModel=1;std::memcpy(badProps.data()+140+24,&invalidPropModel,2);bool propRejects=!parsePreviewProps(badProps,4,20,parsedProps)&&parsedProps.size()==2;badProps=propPayload;badProps.pop_back();propRejects&=!parsePreviewProps(badProps,4,20,parsedProps);badProps=propPayload;float badPropOrigin=std::numeric_limits<float>::quiet_NaN();std::memcpy(badProps.data()+140,&badPropOrigin,4);propRejects&=!parsePreviewProps(badProps,4,20,parsedProps);
    all&=report("BSP malformed static prop ranges rejected",propRejects);
    auto propAtlas=impl_->texture;unsigned propSlots=2;const auto propSlot=appendAtlasTile(propAtlas,propSlots,impl_->modelTexture);
    all&=report("BSP static prop atlas preserves world materials",propSlot==2&&propSlots==3&&propAtlas.width==192&&std::memcmp(propAtlas.pixels.data(),impl_->texture.pixels.data(),128*4)==0&&std::memcmp(propAtlas.pixels.data()+128*4,impl_->modelTexture.pixels.data(),64*4)==0);
    BspMaterials gridMaterials;gridMaterials.slotCount=17;for(unsigned slot=0;slot<17;++slot){gridMaterials.names.push_back(slot==16?"debug/debugblue":"source1ios/missing_atlas_probe"+std::to_string(slot));gridMaterials.slots.push_back(slot);}
    const auto gridAtlas=bspAtlas(gridMaterials,impl_->checkerTexture);
    all&=report("BSP material atlas seventeenth slot uses second row",gridAtlas.width==1024&&gridAtlas.height==128&&gridAtlas.pixels.size()==1024*128*4
        &&std::memcmp(gridAtlas.pixels.data()+64*1024*4,impl_->texture.pixels.data()+64*4,64*4)==0);
    bool planarUV=true;for(size_t i=0;i<impl_->mesh.size();i+=3){const auto& a=impl_->mesh[i];const auto& b=impl_->mesh[i+1];const auto& c=impl_->mesh[i+2];
        const float area=(b.uv[0]-a.uv[0])*(c.uv[1]-a.uv[1])-(c.uv[0]-a.uv[0])*(b.uv[1]-a.uv[1]);planarUV&=std::isfinite(area)&&std::abs(area)>1e-6f;}
    all&=report("BSP fixture planar UVs on floors and walls",planarUV);
    all&=report("BSP LDR lightmap atlas and surface coordinates",impl_->lightmappedFaces==42&&impl_->lightmapTexture.pixels.size()==1024*1024*4&&impl_->mesh[0].lightmap[2]==1);
    bool gradients=false;for(size_t i=0;i<impl_->lightmapTexture.pixels.size();i+=4)gradients|=impl_->lightmapTexture.pixels[i]>0&&impl_->lightmapTexture.pixels[i]<255;
    all&=report("BSP RGBExp32 lightmap gradient decoded",gradients);
    const auto hdrBytes=hdrFixture();dheader_t hdrHeader;std::memcpy(&hdrHeader,hdrBytes.data(),sizeof(hdrHeader));
    auto selection=previewLighting(hdrHeader);bool selections=selection.hdr&&selection.faces==LUMP_FACES_HDR&&selection.samples==LUMP_LIGHTING_HDR;
    auto sharedHeader=hdrHeader;sharedHeader.lumps[LUMP_FACES]=sharedHeader.lumps[LUMP_FACES_HDR];sharedHeader.lumps[LUMP_FACES_HDR]={};
    selection=previewLighting(sharedHeader);selections&=selection.hdr&&selection.faces==LUMP_FACES;
    sharedHeader.lumps[LUMP_LIGHTING]=sharedHeader.lumps[LUMP_LIGHTING_HDR];selection=previewLighting(sharedHeader);
    selections&=!selection.hdr&&selection.faces==LUMP_FACES&&selection.samples==LUMP_LIGHTING;
    all&=report("BSP HDR-only selection shared faces and LDR preference",selections);
    LightmapAtlas hdrAtlas;hdrAtlas.hdr=true;dface_t hdrFace{};hdrFace.lightofs=0;std::fill(std::begin(hdrFace.styles),std::end(hdrFace.styles),255);hdrFace.styles[0]=0;
    texinfo_t hdrInfo{};unsigned hx=0,hy=0,hw=0,hh=0;std::vector<ColorRGBExp32> hdrSamples{{255,128,0,2}};
    const unsigned bright=previewLightChannel(255,2,true), dim=previewLightChannel(255,0,true);
    bool hdrMapped=hdrAtlas.add(hdrFace,hdrInfo,hdrSamples,hx,hy,hw,hh)&&bright>dim&&bright<255
        &&previewLightChannel(0,127,true)==0&&previewLightChannel(255,-128,true)==0&&previewLightChannel(255,127,true)==255
        &&hdrAtlas.texture.pixels[0]==bright&&hdrAtlas.texture.pixels[(1024+1)*4]==bright
        &&hdrAtlas.texture.pixels[2]==0&&hdrAtlas.texture.pixels[3]==255;
    all&=report("BSP HDR RGBExp32 fixed exposure mapping and tile borders",hdrMapped);
    bool hdrBounds=headerValid(hdrHeader,hdrBytes.size());auto badHdr=hdrHeader;badHdr.lumps[LUMP_FACES_HDR].filelen-=1;hdrBounds&=!headerValid(badHdr,hdrBytes.size());
    badHdr=hdrHeader;badHdr.lumps[LUMP_LIGHTING_HDR].uncompressedSize=maximumLump+4;hdrBounds&=!headerValid(badHdr,hdrBytes.size());
    badHdr=hdrHeader;badHdr.lumps[LUMP_LIGHTING_HDR].version=2;hdrBounds&=!headerValid(badHdr,hdrBytes.size());
    hdrFace.lightofs=4;hdrBounds&=!hdrAtlas.add(hdrFace,hdrInfo,hdrSamples,hx,hy,hw,hh);
    all&=report("BSP HDR malformed face sample metadata and light offsets rejected",hdrBounds);
    dface_t badLight{};texinfo_t lightInfo{};badLight.lightofs=0;std::fill(std::begin(badLight.styles),std::end(badLight.styles),255);badLight.styles[0]=0;unsigned lightW=0,lightH=0;
    bool lightRejects=lightmapRange(badLight,lightInfo,4,lightW,lightH);badLight.lightofs=4;lightRejects&=!lightmapRange(badLight,lightInfo,4,lightW,lightH);badLight.lightofs=0;badLight.m_LightmapTextureSizeInLuxels[0]=256;lightRejects&=!lightmapRange(badLight,lightInfo,4,lightW,lightH);
    badLight.m_LightmapTextureSizeInLuxels[0]=0;lightInfo.flags=SURF_BUMPLIGHT;lightRejects&=!lightmapRange(badLight,lightInfo,4,lightW,lightH);
    all&=report("BSP malformed lightmap offsets sizes and bump ranges rejected",lightRejects);
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
    const auto multi=makeStudioFixture(false,true);StudioMesh multiModel;
    bool multiParsed=parseStudioModel(multi.mdl,multi.vvd,multi.vtx,multiModel,modelError);
    bool multiSlots=multiParsed&&multiModel.materials.size()==2&&multiModel.meshes==2&&multiModel.triangles.size()==36;
    if(multiSlots)for(size_t i=0;i<36;++i)multiSlots&=multiModel.triangles[i].material==(i<18?1u:0u);
    all&=report("studio per-mesh textures and default skin remap",multiSlots);
    studiohdr_t multiHeader;std::memcpy(&multiHeader,multi.mdl.data(),sizeof(multiHeader));
    auto badMulti=multi.mdl;short badSkin=-1;std::memcpy(badMulti.data()+multiHeader.skinindex,&badSkin,2);
    bool materialRejects=!parseStudioModel(badMulti,multi.vvd,multi.vtx,multiModel,modelError)&&multiModel.triangles.size()==36;
    badMulti=multi.mdl;int badSkinOffset=std::numeric_limits<int>::max();std::memcpy(badMulti.data()+offsetof(studiohdr_t,skinindex),&badSkinOffset,4);
    materialRejects&=!parseStudioModel(badMulti,multi.vvd,multi.vtx,multiModel,modelError);
    mstudiobodyparts_t multiBody;std::memcpy(&multiBody,multi.mdl.data()+multiHeader.bodypartindex,sizeof(multiBody));mstudiomodel_t multiDiskModel;
    const size_t multiModelOffset=multiHeader.bodypartindex+multiBody.modelindex;std::memcpy(&multiDiskModel,multi.mdl.data()+multiModelOffset,sizeof(multiDiskModel));
    badMulti=multi.mdl;const int badMeshMaterial=2;std::memcpy(badMulti.data()+multiModelOffset+multiDiskModel.meshindex+offsetof(mstudiomesh_t,material),&badMeshMaterial,4);
    materialRejects&=!parseStudioModel(badMulti,multi.vvd,multi.vtx,multiModel,modelError);
    all&=report("studio invalid skin ranges and mesh material references rejected",materialRejects);
    unsigned multiCount=0;const auto multiAtlas=studioAtlas(multiModel,impl_->checkerTexture,impl_->checkerTexture,multiCount);
    all&=report("studio multiple VMT VTF atlas tiles remain distinct",multiCount==2&&multiAtlas.width==128&&multiAtlas.height==64&&std::memcmp(multiAtlas.pixels.data(),multiAtlas.pixels.data()+64*4,64*4)!=0);
    StudioPose multiPose;std::vector<StudioVertex> multiSkin;
    bool multiSkinOK=sampleStudioAnimation(multiModel,0,.375,multiPose)&&skinStudioModel(multiModel,multiPose.rotations,multiSkin,multiPose.positions)&&multiSkin.size()==multiModel.triangles.size();
    if(multiSkinOK)for(size_t i=0;i<multiSkin.size();++i)multiSkinOK&=multiSkin[i].material==multiModel.triangles[i].material;
    all&=report("studio weighted skinning retains per-mesh materials",multiSkinOK);
    const auto skinGeometry=multiModel.triangles;bool skinSwap=selectStudioSkin(multiModel,1);
    if(skinSwap)for(size_t i=0;i<multiModel.triangles.size();++i)skinSwap&=multiModel.triangles[i].material==(i<18?0u:1u)&&multiModel.triangles[i].position==skinGeometry[i].position&&multiModel.triangles[i].uv==skinGeometry[i].uv;
    skinSwap&=selectStudioSkin(multiModel,0)&&multiModel.triangles[0].material==1;
    all&=report("studio skin families remap and restore without geometry changes",skinSwap);
    const auto beforeInvalidSkin=multiModel.triangles;bool invalidSkins=!selectStudioSkin(multiModel,2)&&!selectStudioSkin(multiModel,256)&&multiModel.activeSkin==0;
    for(size_t i=0;i<multiModel.triangles.size();++i)invalidSkins&=multiModel.triangles[i].material==beforeInvalidSkin[i].material;
    auto badSkinModel=multiModel;badSkinModel.triangles[0].materialReference=2;
    invalidSkins&=!selectStudioSkin(badSkinModel,1)&&badSkinModel.activeSkin==0&&badSkinModel.triangles[0].material==1;
    badMulti=multi.mdl;const short invalidUnusedSkin=2;std::memcpy(badMulti.data()+multiHeader.skinindex+4,&invalidUnusedSkin,2);
    invalidSkins&=!parseStudioModel(badMulti,multi.vvd,multi.vtx,multiModel,modelError)&&multiModel.activeSkin==0;
    all&=report("studio invalid skin family and unused family indices rejected atomically",invalidSkins);
    std::vector<StudioVertex> skinVariantPose;bool skinAnimation=selectStudioSkin(multiModel,1)&&skinStudioModel(multiModel,multiPose.rotations,skinVariantPose,multiPose.positions)&&skinVariantPose.size()==multiSkin.size();
    if(skinAnimation)for(size_t i=0;i<skinVariantPose.size();++i)skinAnimation&=skinVariantPose[i].position==multiSkin[i].position&&skinVariantPose[i].material==(i<18?0u:1u);
    all&=report("studio skin family keeps sampled bone pose",skinAnimation);
    auto badBounds=studio.mdl;float invalidBound=std::numeric_limits<float>::quiet_NaN();std::memcpy(badBounds.data()+offsetof(studiohdr_t,hull_min),&invalidBound,4);StudioMesh rejectedBounds;
    all&=report("studio collision bounds decoded and invalid bounds rejected",model.hullMins==Vector(-12,-8,0)&&model.hullMaxs==Vector(12,8,64)&&!parseStudioModel(badBounds,studio.vvd,studio.vtx,rejectedBounds,modelError));
    auto solidPayload=propPayload;bool solidFlags=true;for(unsigned solid:{0,2,6}){solidPayload[170]=solid;solidFlags&=parsePreviewProps(solidPayload,4,20,parsedProps)&&parsedProps[0].solid==solid;}solidPayload[170]=1;solidFlags&=!parsePreviewProps(solidPayload,4,20,parsedProps);
    all&=report("BSP static prop SOLID_NONE BBOX VPHYSICS flags",solidFlags);
    PreviewProp boxProp;boxProp.scale=1.5f;boxProp.angles[1]=30;PropCollision box;bool boxReady=propBox(model,boxProp,box);trace_t boxRay{},boxHull{};boxRay.fraction=boxHull.fraction=1;
    if(boxReady){tracePropBoxes({box},Vector(-100,0,32),Vector(100,0,32),Vector(0,0,0),Vector(0,0,0),boxRay);tracePropBoxes({box},Vector(-100,0,32),Vector(100,0,32),Vector(-8,-8,-24),Vector(8,8,8),boxHull);}
    all&=report("static prop scaled rotated world AABB ray and hull",boxReady&&!boxRay.startsolid&&std::abs(boxRay.fraction-.39206f)<.005f&&!boxHull.startsolid&&std::abs(boxHull.fraction-.35206f)<.005f);
    StudioMesh platform;platform.hullMins=platform.renderMins=Vector(-32,-32,0);platform.hullMaxs=platform.renderMaxs=Vector(32,32,64);PreviewProp platformProp;platformProp.origin={-190,-160,0};PropCollision platformBox;
    auto boxScene=propBox(platform,platformProp,platformBox)?physicsScene(impl_->fixtureCollision,{platformBox}):nullptr;bool resting=false;
    auto fallingShape=ownedBox(Vector(-8,-8,-8),Vector(8,8,8));
    if(boxScene&&fallingShape){objectparams_t params{nullptr,5,1,0,0,.05f,"Source prop drop test",nullptr,0,1,true};auto* falling=boxScene->environment->CreatePolyObject(fallingShape.get(),0,Vector(-190,-160,96),QAngle(0,0,0),&params);
        if(falling){falling->Wake();for(unsigned i=0;i<240;++i)boxScene->environment->Simulate(1.f/120);Vector position;QAngle angles;falling->GetPosition(&position,&angles);resting=boxScene->props.size()==1&&position.IsValid()&&position.z>70&&position.z<75;boxScene->environment->DestroyObject(falling);}}
    auto sphereScene=propBox(platform,platformProp,platformBox)?physicsScene(impl_->fixtureCollision,{platformBox}):nullptr;
    if(sphereScene){objectparams_t params{nullptr,5,1,0,0,.05f,"Source prop sphere drop test",nullptr,0,1,true};auto* sphere=sphereScene->environment->CreateSphereObject(8,0,Vector(-190,-160,96),QAngle(0,0,0),&params,false);
        if(sphere){sphere->Wake();for(unsigned i=0;i<240;++i)sphereScene->environment->Simulate(1.f/120);Vector position;sphere->GetPosition(&position,nullptr);const bool sphereRest=position.IsValid()&&position.z>70&&position.z<75;if(!sphereRest)Warning("Source prop sphere drop: position %.3f %.3f %.3f\n",position.x,position.y,position.z);resting&=sphereRest;sphereScene->environment->DestroyObject(sphere);}else resting=false;}else resting=false;
    all&=report("static prop original vphysics cube and sphere rest on box",resting);
    auto mdl48=studio.mdl;const int version48=48;std::memcpy(mdl48.data()+4,&version48,4);StudioMesh legacy;StudioPose legacyPose;
    std::vector<std::uint8_t> phyBytes;PhyGeometry phyGeometry;std::string phyError;
    const bool phyDecoded=readBounded("models/__source1ios_static_probe.phy","GAME",16*1024*1024,phyBytes)&&parsePhy(phyBytes,model.checksum,phyGeometry,phyError);
    all&=report("PHY bounded VPHY convex point extraction",phyDecoded&&phyGeometry.convexes.size()==1&&phyGeometry.convexes[0].size()==8);
    auto exact=phyDecoded?ownedPhy(phyGeometry,1.5f):Collision{};trace_t exactCenter{},exactOutside{};
    if(exact){g_pPhysicsCollision->TraceBox(Vector(0,-48,90),Vector(0,48,90),Vector(0,0,0),Vector(0,0,0),exact.get(),Vector(0,0,0),QAngle(0,0,0),&exactCenter);g_pPhysicsCollision->TraceBox(Vector(15,-48,90),Vector(15,48,90),Vector(0,0,0),Vector(0,0,0),exact.get(),Vector(0,0,0),QAngle(0,0,0),&exactOutside);}
    all&=report("PHY original convex rebuild exact ray and scaling",exact&&exactCenter.fraction>0&&exactCenter.fraction<.5f&&exactOutside.fraction==1);
    bool phyRejects=phyDecoded;auto rejectPhy=[&](std::vector<std::uint8_t> data){PhyGeometry saved;saved.convexes={{{1,2,3}}};std::string why;return !parsePhy(data,model.checksum,saved,why)&&saved.convexes.size()==1&&saved.convexes[0][0][0]==1;};
    if(phyDecoded){for(size_t offset:{size_t(0),size_t(12),size_t(16),size_t(24),size_t(76),size_t(80),size_t(92)}){auto data=phyBytes;const int invalid=-1;std::memcpy(data.data()+offset,&invalid,4);phyRejects&=rejectPhy(std::move(data));}
        const size_t surface=48,node=surface+phy_detail::get<int>(phyBytes,surface+32),ledge=node+phy_detail::get<int>(phyBytes,node+4),point=ledge+phy_detail::get<int>(phyBytes,ledge);
        auto data=phyBytes;const float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(data.data()+point,&nan,4);phyRejects&=rejectPhy(data);data=phyBytes;int badOffset=-1;std::memcpy(data.data()+ledge,&badOffset,4);phyRejects&=rejectPhy(data);
        data=phyBytes;unsigned invalidEdge=0xffff;std::memcpy(data.data()+ledge+20,&invalidEdge,4);phyRejects&=rejectPhy(data);data=phyBytes;data.resize(data.size()-8);phyRejects&=rejectPhy(data);}
    all&=report("PHY malformed headers offsets edges points rejected",phyRejects);
    CPhysConvex* halves[2]={g_pPhysicsCollision->BBoxToConvex(Vector(-32,-8,0),Vector(-16,8,64)),g_pPhysicsCollision->BBoxToConvex(Vector(16,-8,0),Vector(32,8,64))};
    Collision split(g_pPhysicsCollision->ConvertConvexToCollide(halves,2),CollisionDelete{});PhyGeometry splitGeometry;const auto splitBytes=serializePhy(split,model.checksum);const bool splitParsed=parsePhy(splitBytes,model.checksum,splitGeometry,phyError);auto rebuiltSplit=splitParsed?ownedPhy(splitGeometry,1):Collision{};trace_t gap{},side{};
    if(rebuiltSplit){g_pPhysicsCollision->TraceBox(Vector(0,-32,32),Vector(0,32,32),Vector(0,0,0),Vector(0,0,0),rebuiltSplit.get(),Vector(0,0,0),QAngle(0,0,0),&gap);g_pPhysicsCollision->TraceBox(Vector(24,-32,32),Vector(24,32,32),Vector(0,0,0),Vector(0,0,0),rebuiltSplit.get(),Vector(0,0,0),QAngle(0,0,0),&side);}
    all&=report("PHY separate convex pieces preserve empty gap",rebuiltSplit&&splitGeometry.convexes.size()==2&&gap.fraction==1&&side.fraction>0&&side.fraction<1);
    all&=report("studio MDL48 geometry and embedded clip",parseStudioModel(mdl48,studio.vvd,studio.vtx,legacy,modelError)&&legacy.triangles.size()==36&&legacy.bones.size()==2&&sampleStudioAnimation(legacy,0,.375,legacyPose));
    auto mdl44=studio.mdl;const int version44=44;std::memcpy(mdl44.data()+4,&version44,4);StudioMesh cssLegacy;all&=report("studio MDL44 geometry and embedded clip",parseStudioModel(mdl44,studio.vvd,studio.vtx,cssLegacy,modelError)&&cssLegacy.triangles.size()==36&&cssLegacy.bones.size()==2&&sampleStudioAnimation(cssLegacy,0,.375,legacyPose));
    bool unsupportedVersions=true;for(int version:{47,50}){auto unsupported=mdl48;std::memcpy(unsupported.data()+4,&version,4);unsupportedVersions&=!parseStudioModel(unsupported,studio.vvd,studio.vtx,legacy,modelError);}
    all&=report("studio unsupported MDL versions rejected",unsupportedVersions);
    std::vector<StudioVertex> bindPose;bool bindOK=skinStudioModel(model,{},bindPose)&&bindPose.size()==model.triangles.size();
    for(size_t i=0;bindOK&&i<bindPose.size();++i)bindOK&=(bindPose[i].position-model.triangles[i].position).LengthSqr()<1e-6f;
    all&=report("studio inverse bind pose identity",bindOK&&model.bones.size()==2);
    std::vector<Quaternion> bent={Quaternion(0,0,0,1),Quaternion(0,0,0,1)};AngleQuaternion(QAngle(0,0,90),bent[1]);std::vector<StudioVertex> bentPose;
    bool bentOK=skinStudioModel(model,bent,bentPose);bool rootStill=false,childMoves=false;
    for(size_t i=0;bentOK&&i<bentPose.size();++i){const float delta=(bentPose[i].position-model.triangles[i].position).Length();if(model.triangles[i].bones[0]==0)rootStill|=delta<.001f;else childMoves|=delta>20;}
    all&=report("studio parent hierarchy weighted skinning",bentOK&&rootStill&&childMoves);
    auto blend=model;for(auto& vertex:blend.triangles){vertex.influences=2;vertex.bones={0,1,0};vertex.weights={.5f,.5f,0};}std::vector<StudioVertex> blendPose;
    bool blendOK=skinStudioModel(blend,bent,blendPose);for(size_t i=0;blendOK&&i<blendPose.size();++i){Vector moved;matrix3x4_t rotation;QuaternionMatrix(bent[1],rotation);VectorRotate(model.triangles[i].position-Vector(0,0,32),rotation,moved);const auto expected=(model.triangles[i].position+moved+Vector(0,0,32))*.5f;blendOK&=(blendPose[i].position-expected).LengthSqr()<1e-5f;}
    all&=report("studio two bone weight blend",blendOK);
    StudioPose startPose,halfPose,endPose;bool animationOK=model.animations.size()==1&&sampleStudioAnimation(model,0,0,startPose)&&sampleStudioAnimation(model,0,.375,halfPose)&&sampleStudioAnimation(model,0,2,endPose);
    Quaternion expected;AngleQuaternion(RadianEuler(.4f,0,0),expected);bool halfway=animationOK&&std::abs(QuaternionDotProduct(halfPose.rotations[1],expected))>.9999f;
    all&=report("studio embedded RLE clip interpolation/loop",halfway&&std::abs(QuaternionDotProduct(startPose.rotations[1],endPose.rotations[1]))>.99999f);
    auto emptyAnimation=studio.mdl;auto* emptyHeader=reinterpret_cast<studiohdr_t*>(emptyAnimation.data());auto* emptyDesc=emptyHeader->pLocalAnimdesc(0);auto* emptyTrack=reinterpret_cast<mstudioanim_t*>(reinterpret_cast<unsigned char*>(emptyDesc)+emptyDesc->animindex);emptyTrack->bone=255;emptyTrack->flags=0;emptyTrack->nextoffset=0;
    StudioMesh emptyModel;StudioPose emptyPose;all&=report("studio bone 255 empty animation uses bind pose",parseStudioModel(emptyAnimation,studio.vvd,studio.vtx,emptyModel,modelError)&&sampleStudioAnimation(emptyModel,0,.25,emptyPose)&&emptyPose.rotations.size()==emptyModel.bones.size()&&emptyPose.positions.size()==emptyModel.bones.size());
    auto windowsMaterials=studio.mdl;auto* windowsHeader=reinterpret_cast<studiohdr_t*>(windowsMaterials.data());char* windowsDirectory=const_cast<char*>(windowsHeader->pCdtexture(0));for(char* c=windowsDirectory;*c;++c)if(*c=='/')*c='\\';StudioMesh normalizedMaterials;
    all&=report("studio Windows material separators normalize safely",parseStudioModel(windowsMaterials,studio.vvd,studio.vtx,normalizedMaterials,modelError)&&!normalizedMaterials.materials.empty()&&!normalizedMaterials.materials[0].empty()&&normalizedMaterials.materials[0][0].find('\\')==std::string::npos);
    const char windowsVmt[]="VertexLitGeneric { \"$basetexture\" \"debug\\debugblue\" }";auto windowsFile=g_pFullFileSystem->Open("materials/debug/__source1ios_windows.vmt","wb","DEFAULT_WRITE_PATH");bool windowsBase=windowsFile&&g_pFullFileSystem->Write(windowsVmt,sizeof(windowsVmt)-1,windowsFile)==int(sizeof(windowsVmt)-1);if(windowsFile)g_pFullFileSystem->Close(windowsFile);SourceTexture windowsTexture;
    windowsBase&=decodeMaterial({"debug/__source1ios_windows"},windowsTexture,"studio")&&windowsTexture.width==64&&windowsTexture.height==64;
    all&=report("studio VMT Windows basetexture path resolves safely",windowsBase);
    const auto externalStudio=makeStudioFixture(true);StudioMesh externalModel;StudioPose externalPose;
    auto external48=externalStudio.mdl;std::memcpy(external48.data()+4,&version48,4);StudioMesh externalLegacy;
    all&=report("studio MDL48 external ANI clip",parseStudioModel(external48,externalStudio.vvd,externalStudio.vtx,externalLegacy,modelError,externalStudio.ani)&&sampleStudioAnimation(externalLegacy,0,.375,legacyPose));
    all&=report("studio external ANI block RLE pose",parseStudioModel(externalStudio.mdl,externalStudio.vvd,externalStudio.vtx,externalModel,modelError,externalStudio.ani)
        &&externalModel.animations.size()==1&&sampleStudioAnimation(externalModel,0,.375,externalPose)&&std::abs(QuaternionDotProduct(externalPose.rotations[1],expected))>.9999f);
    StudioMesh missingExternal;all&=report("studio missing ANI retains bind geometry",parseStudioModel(externalStudio.mdl,externalStudio.vvd,externalStudio.vtx,missingExternal,modelError)&&missingExternal.animations.empty()&&missingExternal.triangles.size()==36);
    auto shortAni=externalStudio.ani;shortAni.pop_back();auto crossedAni=externalStudio.ani;crossedAni[16+sizeof(mstudioanim_t)+sizeof(mstudioanim_valueptr_t)]=0;
    StudioMesh invalidExternal;all&=report("studio ANI truncated block and malformed RLE rejected",!parseStudioModel(externalStudio.mdl,externalStudio.vvd,externalStudio.vtx,invalidExternal,modelError,shortAni)
        &&!parseStudioModel(externalStudio.mdl,externalStudio.vvd,externalStudio.vtx,invalidExternal,modelError,crossedAni));
    auto badAnimation=studio.mdl;auto* animHeader=reinterpret_cast<studiohdr_t*>(badAnimation.data());auto* anim=animHeader->pLocalAnimdesc(0);auto* track=reinterpret_cast<mstudioanim_t*>(reinterpret_cast<unsigned char*>(anim)+anim->animindex);auto* values=track->pRotV()->pAnimvalue(0);values->num.total=0;
    StudioMesh rejectedAnimation;std::string animationError;all&=report("studio malformed RLE track rejected",!parseStudioModel(badAnimation,studio.vvd,studio.vtx,rejectedAnimation,animationError));
    auto rawAnimation=studio.mdl;auto* rawHeader=reinterpret_cast<studiohdr_t*>(rawAnimation.data());auto* rawDesc=rawHeader->pLocalAnimdesc(0);auto* rawTrack=reinterpret_cast<mstudioanim_t*>(reinterpret_cast<unsigned char*>(rawDesc)+rawDesc->animindex);rawTrack->flags=STUDIO_ANIM_RAWROT2|STUDIO_ANIM_RAWPOS;
    Quaternion rawExpected;AngleQuaternion(RadianEuler(.25f,0,0),rawExpected);Quaternion64 rawQuaternion;rawQuaternion=rawExpected;Vector48 rawPosition;rawPosition=Vector(0,0,40);
    std::memcpy(rawTrack->pData(),&rawQuaternion,sizeof(rawQuaternion));std::memcpy(rawTrack->pData()+sizeof(rawQuaternion),&rawPosition,sizeof(rawPosition));
    StudioMesh rawModel;StudioPose rawPose;all&=report("studio raw Quaternion64 Vector48 clip",parseStudioModel(rawAnimation,studio.vvd,studio.vtx,rawModel,animationError)&&sampleStudioAnimation(rawModel,0,.5,rawPose)&&std::abs(QuaternionDotProduct(rawPose.rotations[1],rawExpected))>.9999f&&(rawPose.positions[1]-Vector(0,0,40)).LengthSqr()<.001f);
    auto fixedVvd=studio.vvd;const auto originalVvd=*reinterpret_cast<const vertexFileHeader_t*>(studio.vvd.data());
    fixedVvd.resize(sizeof(vertexFileHeader_t)+sizeof(vertexFileFixup_t)+8*sizeof(mstudiovertex_t));
    auto* fixedHeader=reinterpret_cast<vertexFileHeader_t*>(fixedVvd.data());*fixedHeader=originalVvd;fixedHeader->numFixups=1;fixedHeader->fixupTableStart=sizeof(vertexFileHeader_t);fixedHeader->vertexDataStart=sizeof(vertexFileHeader_t)+sizeof(vertexFileFixup_t);
    auto* fixup=reinterpret_cast<vertexFileFixup_t*>(fixedVvd.data()+fixedHeader->fixupTableStart);fixup->lod=0;fixup->sourceVertexID=0;fixup->numVertexes=8;
    std::memcpy(fixedVvd.data()+fixedHeader->vertexDataStart,studio.vvd.data()+originalVvd.vertexDataStart,8*sizeof(mstudiovertex_t));
    auto stripVtx=studio.vtx;auto* stripHeader=reinterpret_cast<OptimizedModel::FileHeader_t*>(stripVtx.data());auto* strip=stripHeader->pBodyPart(0)->pModel(0)->pLOD(0)->pMesh(0)->pStripGroup(0)->pStrip(0);strip->flags=OptimizedModel::STRIP_IS_TRISTRIP;strip->numIndices=4;
    all&=report("VVD fixups and VTX triangle strip",parseStudioModel(studio.mdl,fixedVvd,stripVtx,model,modelError)&&model.sourceVertices==8&&model.triangles.size()==6);
    auto wrongVvd=studio.vvd;reinterpret_cast<vertexFileHeader_t*>(wrongVvd.data())->checksum^=1;
    bool modelRejects=!parseStudioModel(studio.mdl,wrongVvd,studio.vtx,model,modelError);
    auto badBones=studio.mdl;auto* badBoneHeader=reinterpret_cast<studiohdr_t*>(badBones.data());badBoneHeader->pBone(0)->parent=1;modelRejects&=!parseStudioModel(badBones,studio.vvd,studio.vtx,model,modelError);
    auto badWeights=studio.vvd;auto* badWeightHeader=reinterpret_cast<vertexFileHeader_t*>(badWeights.data());auto* badVertex=reinterpret_cast<mstudiovertex_t*>(badWeights.data()+badWeightHeader->vertexDataStart);badVertex[0].m_BoneWeights.weight[0]=std::numeric_limits<float>::quiet_NaN();modelRejects&=!parseStudioModel(studio.mdl,badWeights,studio.vtx,model,modelError);
    auto wrongVtx=studio.vtx;auto* header=reinterpret_cast<OptimizedModel::FileHeader_t*>(wrongVtx.data());header->bodyPartOffset=std::numeric_limits<int>::max();modelRejects&=!parseStudioModel(studio.mdl,studio.vvd,wrongVtx,model,modelError);
    auto wrongMdl=studio.mdl;reinterpret_cast<studiohdr_t*>(wrongMdl.data())->length=std::numeric_limits<int>::max();modelRejects&=!parseStudioModel(wrongMdl,studio.vvd,studio.vtx,model,modelError);
    all&=report("MDL companion mismatch/ranges rejected",modelRejects);
    const MDLHandle_t handle=g_pMDLCache->FindMDL(impl_->builtinModel.c_str());const auto* cached=handle==MDLHANDLE_INVALID?nullptr:g_pMDLCache->GetStudioHdr(handle);
    all&=report("original MDLCache studio header",cached&&cached->id==idStudioHeader&&cached->version==STUDIO_VERSION&&cached->checksum==0x510510);
    if(handle!=MDLHANDLE_INVALID)g_pMDLCache->Release(handle);
    all&=report("Metal static model geometry staged",impl_->modelMesh.size()==36);
    std::string patchBase;
    all&=report("VMT Patch include chain base texture",vmtBaseTexture("debug/debugblue",patchBase)&&patchBase=="debug/debugblue"&&vmtBaseTexture("debug/__source1ios_patch_outer",patchBase)&&patchBase=="debug/debugblue");
    all&=report("VMT Patch insert replace existing-key semantics",vmtBaseTexture("debug/__source1ios_patch_insert",patchBase)&&patchBase=="debug/debugempty"&&!vmtBaseTexture("debug/__source1ios_patch_noinsert",patchBase));
    all&=report("VMT Patch cycle unsafe include and KV macros rejected",!vmtBaseTexture("debug/__source1ios_patch_cycle",patchBase)&&!vmtBaseTexture("debug/__source1ios_patch_unsafe",patchBase)&&!vmtBaseTexture("debug/__source1ios_patch_macro",patchBase));
    all&=report("studio MDL VMT VTF base texture",impl_->modelTexture.width==64&&impl_->modelTexture.height==64&&impl_->modelTexture.pixels.size()==64*64*4&&impl_->modelTexture.pixels!=impl_->texture.pixels);
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
    all&=report("VTF checker fallback decoded",impl_->checkerTexture.width==64 && impl_->checkerTexture.height==64 && impl_->checkerTexture.pixels.size()==64*64*4);
    all&=report("engine brush model loaded",impl_->world && modelloader->IsLoaded(impl_->world) && impl_->world->type==mod_brush && impl_->world->brush.pShared->numvertexes==56 && impl_->world->brush.pShared->numsurfaces==42);
    Ray_t ray;ray.Init(Vector(-190,-160,80),Vector(-190,-160,-80));CM_BoxTrace(ray,0,MASK_SOLID,true,trace);
    all&=report("engine CM floor trace",trace.fraction>.49f && trace.fraction<.51f);
    all&=report("engine CM point contents",CM_PointContents(Vector(-190,-160,80),0)==CONTENTS_EMPTY && (CM_PointContents(Vector(-190,-160,-8),0)&CONTENTS_SOLID));
    std::vector<PreviewSpawn> parsedSpawns;
    all&=report("BSP player spawn origin and eye camera",impl_->spawns.size()==1&&impl_->spawnCamera==Vector(-190,-160,80)&&impl_->spawnAngles==QAngle(8,45,0));
    const bool csSpawn=parsePreviewSpawns("// map starts\n{ classname worldspawn } { classname info_player_counterterrorist origin \"100 200 16\" angle 90 } { classname info_player_terrorist origin \"-100 0 16\" angles \"0 180 0\" }",parsedSpawns);
    all&=report("BSP CS team spawn entity data",csSpawn&&parsedSpawns.size()==2&&parsedSpawns[0].origin[1]==200&&parsedSpawns[0].angles[1]==90&&parsedSpawns[1].angles[1]==180);
    const auto savedSpawns=parsedSpawns;
    all&=report("BSP malformed spawn and entity bounds rejected",!parsePreviewSpawns("{ classname info_player_start origin \"nan 0 0\" }",parsedSpawns)&&!parsePreviewSpawns("{ classname worldspawn",parsedSpawns)&&!parsePreviewSpawns(std::string(1024*1024+1,' '),parsedSpawns)&&parsedSpawns.size()==savedSpawns.size());
    std::vector<PreviewProp> entityModels;const bool entityOK=parsePreviewSpawns("{ classname prop_dynamic model models/test.mdl origin \"1 2 3\" angle 90 skin 1 modelscale 2 } { classname prop_dynamic_override model models/test.mdl body 1 }",parsedSpawns,&entityModels);
    all&=report("BSP entity model transform skin scale and bodygroup bounds",entityOK&&entityModels.size()==1&&entityModels[0].origin[2]==3&&entityModels[0].angles[1]==90&&entityModels[0].skin==1&&entityModels[0].scale==2&&entityModels[0].solid==0);
    const auto savedModels=entityModels;
    all&=report("BSP malformed entity model preserves staged output",!parsePreviewSpawns("{ classname prop_dynamic model models/../bad.mdl }",parsedSpawns,&entityModels)&&!parsePreviewSpawns("{ classname prop_dynamic model models/test.mdl skin 1.5 }",parsedSpawns,&entityModels)&&!parsePreviewSpawns("{ classname prop_dynamic model models/test.mdl modelscale nan }",parsedSpawns,&entityModels)&&entityModels.size()==savedModels.size()&&entityModels[0].model==savedModels[0].model);
    all&=report("engine worldspawn entities",CM_EntityString() && std::strstr(CM_EntityString(),"worldspawn"));
    auto testScene=physicsScene(impl_->fixtureCollision);
    if(!testScene)return report("live physics body pose advances",false);
    Vector oldPosition,newPosition;testScene->body->GetPosition(&oldPosition,nullptr);testScene->environment->Simulate(.02f);testScene->body->GetPosition(&newPosition,nullptr);
    all&=report("live physics body pose advances",newPosition.IsValid() && (newPosition-oldPosition).Length()>1e-5f);
    auto before=vertices(1);const auto saved=impl_->angles;look(10,0);auto after=vertices(1);impl_->angles=saved;
    all&=report("Source camera projection",before.size()==after.size()&&!before.empty()&&before[0].position[0]!=after[0].position[0]);return all;
}
}
