#include "SourceStudio.hpp"
#include "studio.h"
#include "optimize.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {
constexpr int idStudioHeader=(('T'<<24)+('S'<<16)+('D'<<8)+'I');
constexpr size_t maximumModelFile=32*1024*1024;
constexpr size_t maximumModelVertices=65536;
constexpr size_t maximumModelTriangles=65536;
// Serialized layouts used by the preview are shared by SDK 2013 MDL 48 and
// this upstream's MDL 49. Never read native 64-bit pointer layouts from disk.
static_assert(sizeof(studiohdr_t)==408 && sizeof(mstudiobone_t)==216,"Studio disk header/bone layout changed");
static_assert(sizeof(mstudiomodel_t)==148 && sizeof(mstudiomesh_t)==116,"Studio disk model/mesh layout changed");
static_assert(sizeof(mstudioanimdesc_t)==100 && sizeof(mstudiovertex_t)==48,"Studio disk animation/vertex layout changed");
template<class T> T* at(std::vector<std::uint8_t>& bytes,size_t offset){
    return offset<=bytes.size() && sizeof(T)<=bytes.size()-offset?reinterpret_cast<T*>(bytes.data()+offset):nullptr;
}
template<class T> const T* at(const std::vector<std::uint8_t>& bytes,size_t offset,size_t count=1){
    if(count>std::numeric_limits<size_t>::max()/sizeof(T) || offset>bytes.size() || count*sizeof(T)>bytes.size()-offset)return nullptr;
    return reinterpret_cast<const T*>(bytes.data()+offset);
}
bool addRelative(size_t base,int relative,size_t& result){
    if(relative<0 || size_t(relative)>std::numeric_limits<size_t>::max()-base)return false;
    result=base+size_t(relative);return true;
}
template<class T> size_t append(std::vector<std::uint8_t>& bytes,const T& value){
    const size_t offset=bytes.size();bytes.resize(offset+sizeof(T));std::memcpy(bytes.data()+offset,&value,sizeof(T));return offset;
}
template<class T> size_t appendMany(std::vector<std::uint8_t>& bytes,const T* values,size_t count){
    const size_t offset=bytes.size();bytes.resize(offset+sizeof(T)*count);if(count)std::memcpy(bytes.data()+offset,values,sizeof(T)*count);return offset;
}
bool finiteVertex(const mstudiovertex_t& v){
    return v.m_vecPosition.IsValid() && v.m_vecNormal.IsValid()
        && std::isfinite(v.m_vecTexCoord.x) && std::isfinite(v.m_vecTexCoord.y)
        && v.m_vecPosition.LengthSqr()<32768.f*32768.f && v.m_vecNormal.LengthSqr()>.01f && v.m_vecNormal.LengthSqr()<4;
}
bool animationValues(const std::vector<std::uint8_t>& bytes,size_t base,short relative,int frames,float scale,std::vector<float>& out){
    out.assign(frames,0);if(!relative)return true;if(relative<0||!std::isfinite(scale))return false;size_t cursor=base+size_t(relative);int frame=0;
    while(frame<frames){const auto* run=at<mstudioanimvalue_t>(bytes,cursor);if(!run||!run->num.total||!run->num.valid||run->num.valid>run->num.total)return false;
        const unsigned valid=run->num.valid,total=run->num.total;const auto* values=at<mstudioanimvalue_t>(bytes,cursor,valid+1);if(!values)return false;
        for(unsigned j=0;j<total&&frame<frames;++j,++frame){const float value=values[1+std::min(j,valid-1)].value*scale;if(!std::isfinite(value))return false;out[frame]=value;}
        cursor+=(valid+1)*sizeof(*values);
    }return true;
}
bool readAnimations(const std::vector<std::uint8_t>& bytes,const studiohdr_t& header,const mstudiobone_t* bones,source1ios::StudioMesh& result,std::string& error,const std::vector<std::uint8_t>& ani){
    auto fail=[&](const char* why){error=why;return false;};if(header.numlocalanim<0||header.numlocalanim>128)return fail("animation count exceeds limit");
    const auto* descriptions=at<mstudioanimdesc_t>(bytes,header.localanimindex,header.numlocalanim);if(!descriptions)return fail("animation descriptions outside MDL");size_t budget=0;
    for(int a=0;a<header.numlocalanim;++a){const auto& desc=descriptions[a];const size_t base=size_t(header.localanimindex)+size_t(a)*sizeof(*descriptions);
        // Sections, delta/IK/local hierarchy and newer frame animation
        // need separate paths; retain static geometry without pretending to play them.
        if(desc.sectionframes||desc.numlocalhierarchy||desc.numikrules||(desc.flags&~STUDIO_LOOPING)||desc.animblock==-1)continue;
        if(desc.animblock<0||desc.animblock>=std::max(1,header.numanimblocks))return fail("invalid external animation block index");
        if(desc.animblock && ani.empty())continue;
        std::vector<std::uint8_t> external;
        if(desc.animblock){const auto* block=at<mstudioanimblock_t>(bytes,header.animblockindex,header.numanimblocks);if(!block)return fail("animation block table outside MDL");const auto& range=block[desc.animblock];
            if(range.datastart<0||range.dataend<=range.datastart||size_t(range.dataend)>ani.size())return fail("animation block range outside ANI");
            external.assign(ani.begin()+range.datastart,ani.begin()+range.dataend);}
        const auto& tracks=desc.animblock?external:bytes;
        if(desc.numframes<1||desc.numframes>2048||!std::isfinite(desc.fps)||desc.fps<=0||desc.fps>240)return fail("invalid embedded animation timing");
        const size_t cost=size_t(desc.numframes)*result.bones.size();if(cost>262144-budget)return fail("animation pose budget exceeded");budget+=cost;
        source1ios::StudioAnimation clip;clip.fps=desc.fps;clip.looping=(desc.flags&STUDIO_LOOPING)!=0;size_t name=0;
        if(!addRelative(base,desc.sznameindex,name)||name>=bytes.size())return fail("animation name outside MDL");
        for(size_t i=name;i<bytes.size()&&i<name+128&&bytes[i];++i)clip.name.push_back(char(bytes[i]));if(name+clip.name.size()>=bytes.size()||bytes[name+clip.name.size()])return fail("unterminated animation name");
        clip.frames.resize(desc.numframes);for(auto& frame:clip.frames)for(const auto& bone:result.bones){frame.rotations.push_back(bone.rotation);frame.positions.push_back(bone.position);}
        size_t cursor=0;if(desc.animindex&&!addRelative(desc.animblock?0:base,desc.animindex,cursor))return fail("animation track offset overflow");std::vector<bool> seen(result.bones.size(),false);
        const bool hasTracks=desc.animblock||desc.animindex;
        for(size_t n=0;hasTracks&&n<=result.bones.size();++n){const auto* track=at<mstudioanim_t>(tracks,cursor);if(!track||track->bone>=result.bones.size()||seen[track->bone])return fail("invalid or repeated animation bone");seen[track->bone]=true;
            const auto& bone=bones[track->bone];const unsigned flags=track->flags;if(flags&~(STUDIO_ANIM_RAWPOS|STUDIO_ANIM_RAWROT|STUDIO_ANIM_RAWROT2|STUDIO_ANIM_ANIMPOS|STUDIO_ANIM_ANIMROT))return fail("unsupported embedded track flags");
            const bool raw=flags&(STUDIO_ANIM_RAWPOS|STUDIO_ANIM_RAWROT|STUDIO_ANIM_RAWROT2);if(raw&&(flags&(STUDIO_ANIM_ANIMPOS|STUDIO_ANIM_ANIMROT)))return fail("mixed raw and RLE track");
            size_t data=cursor+sizeof(*track);Quaternion rawRotation=bone.quat;Vector rawPosition=bone.pos;
            if(flags&STUDIO_ANIM_RAWROT){if(flags&STUDIO_ANIM_RAWROT2)return fail("two raw rotation formats");const auto* q=at<Quaternion48>(tracks,data);if(!q)return fail("truncated Quaternion48");Quaternion48 copy;std::memcpy(&copy,q,sizeof(copy));rawRotation=copy;data+=sizeof(*q);}
            if(flags&STUDIO_ANIM_RAWROT2){const auto* q=at<Quaternion64>(tracks,data);if(!q)return fail("truncated Quaternion64");Quaternion64 copy;std::memcpy(&copy,q,sizeof(copy));rawRotation=copy;data+=sizeof(*q);}
            if(flags&STUDIO_ANIM_RAWPOS){const auto* p=at<Vector48>(tracks,data);if(!p)return fail("truncated Vector48");Vector48 copy;std::memcpy(&copy,p,sizeof(copy));rawPosition=copy;}
            std::vector<float> rotation[3],position[3];
            if(flags&STUDIO_ANIM_ANIMROT){const auto* ptr=at<mstudioanim_valueptr_t>(tracks,data);if(!ptr)return fail("truncated rotation pointers");for(int j=0;j<3;++j)if(!animationValues(tracks,data,ptr->offset[j],desc.numframes,bone.rotscale[j],rotation[j]))return fail("invalid rotation RLE stream");data+=sizeof(*ptr);}
            if(flags&STUDIO_ANIM_ANIMPOS){const auto* ptr=at<mstudioanim_valueptr_t>(tracks,data);if(!ptr)return fail("truncated position pointers");for(int j=0;j<3;++j)if(!animationValues(tracks,data,ptr->offset[j],desc.numframes,bone.posscale[j],position[j]))return fail("invalid position RLE stream");}
            for(int f=0;f<desc.numframes;++f){auto q=rawRotation;auto p=rawPosition;if(flags&STUDIO_ANIM_ANIMROT){RadianEuler angles(bone.rot.x+rotation[0][f],bone.rot.y+rotation[1][f],bone.rot.z+rotation[2][f]);if(!angles.IsValid())return fail("invalid animated angles");AngleQuaternion(angles,q);}
                if(flags&STUDIO_ANIM_ANIMPOS)for(int j=0;j<3;++j)p[j]+=position[j][f];float norm=0;for(int j=0;j<4;++j){if(!std::isfinite(q[j]))return fail("nonfinite animated quaternion");norm+=q[j]*q[j];}if(std::abs(norm-1)>.01f||!p.IsValid()||p.LengthSqr()>32768.f*32768.f)return fail("invalid animated pose");clip.frames[f].rotations[track->bone]=q;clip.frames[f].positions[track->bone]=p;}
            if(!track->nextoffset)break;if(track->nextoffset<int(sizeof(*track))||!addRelative(cursor,track->nextoffset,cursor))return fail("invalid animation track link");
        }result.animations.push_back(std::move(clip));
    }return true;
}
}

namespace source1ios {
StudioFixture makeStudioFixture(bool external,bool multipleMaterials){
    StudioFixture f;constexpr int checksum=0x510510;
    studiohdr_t mdl{};mdl.id=idStudioHeader;mdl.version=STUDIO_VERSION;mdl.checksum=checksum;
    mdl.hull_min=mdl.view_bbmin=Vector(-12,-8,0);mdl.hull_max=mdl.view_bbmax=Vector(12,8,64);
    std::strncpy(mdl.name,"source1ios_static_probe.mdl",sizeof(mdl.name)-1);mdl.numbodyparts=1;
    const size_t mdlHeader=append(f.mdl,mdl),boneOffset=f.mdl.size();
    mstudiobone_t bones[2]{};for(auto& bone:bones){bone.quat=Quaternion(0,0,0,1);SetIdentityMatrix(bone.poseToBone);for(auto& controller:bone.bonecontroller)controller=-1;}
    bones[0].parent=-1;bones[1].parent=0;bones[1].pos=Vector(0,0,32);bones[1].poseToBone[2][3]=-32;bones[1].rotscale=Vector(.0001f,.0001f,.0001f);appendMany(f.mdl,bones,2);
    const size_t bodyOffset=f.mdl.size();mstudiobodyparts_t body{};body.nummodels=1;append(f.mdl,body);
    const size_t modelOffset=f.mdl.size();mstudiomodel_t model{};std::strncpy(model.name,"static_probe",sizeof(model.name)-1);model.nummeshes=1;model.numvertices=8;model.vertexindex=0;append(f.mdl,model);
    const size_t meshOffset=f.mdl.size();mstudiomesh_t mesh{};mesh.numvertices=8;mesh.vertexoffset=0;mesh.modelindex=int(modelOffset-meshOffset);append(f.mdl,mesh);
    const size_t animationOffset=f.mdl.size();mstudioanimdesc_t animation{};animation.fps=4;animation.flags=STUDIO_LOOPING;animation.numframes=9;append(f.mdl,animation);
    const char clipName[]="source1ios_bend";const size_t nameOffset=appendMany(f.mdl,clipName,sizeof(clipName));
    const size_t trackOffset=f.mdl.size();mstudioanim_t track{};track.bone=1;track.flags=STUDIO_ANIM_ANIMROT;append(f.mdl,track);
    mstudioanim_valueptr_t values{};values.offset[0]=sizeof(values);append(f.mdl,values);
    mstudioanimvalue_t run{};run.num.valid=run.num.total=9;append(f.mdl,run);
    const short angles[]={0,3000,5000,3000,0,-3000,-5000,-3000,0};for(short angle:angles){mstudioanimvalue_t value{};value.value=angle;append(f.mdl,value);}
    auto* desc=at<mstudioanimdesc_t>(f.mdl,animationOffset);desc->baseptr=-int(animationOffset);desc->sznameindex=nameOffset-animationOffset;desc->animindex=trackOffset-animationOffset;
    const size_t textureOffset=f.mdl.size();mstudiotexture_t texture{};append(f.mdl,texture);const char textureName[]="__source1ios_model";const size_t textureNameOffset=appendMany(f.mdl,textureName,sizeof(textureName));at<mstudiotexture_t>(f.mdl,textureOffset)->sznameindex=textureNameOffset-textureOffset;
    const size_t directoriesOffset=f.mdl.size();int directory=0;append(f.mdl,directory);const char directoryName[]="models/source1ios/";directory=f.mdl.size();appendMany(f.mdl,directoryName,sizeof(directoryName));std::memcpy(f.mdl.data()+directoriesOffset,&directory,sizeof(directory));
    auto* mh=at<studiohdr_t>(f.mdl,mdlHeader);auto* bp=at<mstudiobodyparts_t>(f.mdl,bodyOffset);auto* mo=at<mstudiomodel_t>(f.mdl,modelOffset);
    mh->numbones=2;mh->boneindex=boneOffset;mh->bodypartindex=bodyOffset;mh->length=f.mdl.size();bp->modelindex=modelOffset-bodyOffset;mo->meshindex=meshOffset-modelOffset;
    mh->numlocalanim=1;mh->localanimindex=animationOffset;
    mh->numtextures=1;mh->textureindex=textureOffset;mh->numcdtextures=1;mh->cdtextureindex=directoriesOffset;
    if(external){f.ani.resize(16,0);f.ani.insert(f.ani.end(),f.mdl.begin()+trackOffset,f.mdl.begin()+textureOffset);
        const mstudioanimblock_t blocks[]={{0,0},{16,int(f.ani.size())}};const size_t blockOffset=appendMany(f.mdl,blocks,2);
        const char aniName[]="__source1ios_external_probe.ani";const size_t aniNameOffset=appendMany(f.mdl,aniName,sizeof(aniName));
        mh=at<studiohdr_t>(f.mdl,mdlHeader);mh->numanimblocks=2;mh->animblockindex=blockOffset;mh->szanimblocknameindex=aniNameOffset;mh->length=f.mdl.size();
        auto* desc=at<mstudioanimdesc_t>(f.mdl,animationOffset);desc->animblock=1;desc->animindex=0;}

    vertexFileHeader_t vh{};vh.id=MODEL_VERTEX_FILE_ID;vh.version=MODEL_VERTEX_FILE_VERSION;vh.checksum=checksum;vh.numLODs=1;vh.numLODVertexes[0]=8;
    const size_t vvdHeader=append(f.vvd,vh);const Vector positions[]={
        {-12,-8,0},{12,-8,0},{12,8,0},{-12,8,0},{-8,-5,64},{8,-5,64},{8,5,64},{-8,5,64}};
    mstudiovertex_t vertices[8]{};for(int i=0;i<8;++i){vertices[i].m_vecPosition=positions[i];vertices[i].m_vecNormal=Vector(positions[i].x,positions[i].y,i<4?-8:8);VectorNormalize(vertices[i].m_vecNormal);vertices[i].m_vecTexCoord=Vector2D((i&1)?1:0,(i&2)?1:0);vertices[i].m_BoneWeights.numbones=1;vertices[i].m_BoneWeights.weight[0]=1;}
    for(int i=4;i<8;++i)vertices[i].m_BoneWeights.bone[0]=1;
    const size_t vertexOffset=appendMany(f.vvd,vertices,8);auto* vhp=at<vertexFileHeader_t>(f.vvd,vvdHeader);vhp->vertexDataStart=vertexOffset;vhp->tangentDataStart=0;

    using namespace OptimizedModel;FileHeader_t fh{};fh.version=OPTIMIZED_MODEL_FILE_VERSION;fh.checkSum=checksum;fh.numLODs=1;fh.numBodyParts=1;
    const size_t fhOffset=append(f.vtx,fh);MaterialReplacementListHeader_t replacement{};const size_t replacementOffset=append(f.vtx,replacement);
    BodyPartHeader_t vbp{};vbp.numModels=1;const size_t vbpOffset=append(f.vtx,vbp);ModelHeader_t vm{};vm.numLODs=1;const size_t vmOffset=append(f.vtx,vm);
    ModelLODHeader_t lod{};lod.numMeshes=1;const size_t lodOffset=append(f.vtx,lod);MeshHeader_t vmesh{};vmesh.numStripGroups=1;const size_t vmeshOffset=append(f.vtx,vmesh);
    StripGroupHeader_t group{};group.numVerts=8;group.numIndices=36;group.numStrips=1;const size_t groupOffset=append(f.vtx,group);
    StripHeader_t strip{};strip.numIndices=36;strip.numVerts=8;strip.flags=STRIP_IS_TRILIST;const size_t stripOffset=append(f.vtx,strip);
    Vertex_t map[8]{};for(unsigned i=0;i<8;++i){map[i].origMeshVertID=i;map[i].numBones=1;map[i].boneID[0]=0;}
    const size_t mapOffset=appendMany(f.vtx,map,8);const unsigned short indices[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    const size_t indexOffset=appendMany(f.vtx,indices,36);
    auto* fhh=at<FileHeader_t>(f.vtx,fhOffset);auto* bph=at<BodyPartHeader_t>(f.vtx,vbpOffset);auto* vmh=at<ModelHeader_t>(f.vtx,vmOffset);auto* l=at<ModelLODHeader_t>(f.vtx,lodOffset);auto* me=at<MeshHeader_t>(f.vtx,vmeshOffset);auto* sg=at<StripGroupHeader_t>(f.vtx,groupOffset);auto* st=at<StripHeader_t>(f.vtx,stripOffset);
    fhh->materialReplacementListOffset=replacementOffset;fhh->bodyPartOffset=vbpOffset;bph->modelOffset=vmOffset-vbpOffset;vmh->lodOffset=lodOffset-vmOffset;l->meshOffset=vmeshOffset-lodOffset;me->stripGroupHeaderOffset=groupOffset-vmeshOffset;sg->stripOffset=stripOffset-groupOffset;sg->vertOffset=mapOffset-groupOffset;sg->indexOffset=indexOffset-groupOffset;st->indexOffset=0;st->vertOffset=0;
    if(multipleMaterials){
        f.mdl.resize((f.mdl.size()+3)&~size_t(3));
        const size_t texturesOffset=f.mdl.size();mstudiotexture_t textures[2]{};appendMany(f.mdl,textures,2);
        const char* names[]={"__source1ios_model","__source1ios_model_alt"};
        for(unsigned i=0;i<2;++i){const size_t name=appendMany(f.mdl,names[i],std::strlen(names[i])+1);at<mstudiotexture_t>(f.mdl,texturesOffset+i*sizeof(mstudiotexture_t))->sznameindex=name-(texturesOffset+i*sizeof(mstudiotexture_t));}
        f.mdl.resize((f.mdl.size()+3)&~size_t(3));const short skinRefs[]={1,0};const size_t skinsOffset=appendMany(f.mdl,skinRefs,2);
        const size_t meshesOffset=f.mdl.size();mstudiomesh_t meshes[2]{};
        for(unsigned i=0;i<2;++i){meshes[i]=mesh;meshes[i].material=i;meshes[i].modelindex=int(modelOffset-(meshesOffset+i*sizeof(mesh)));}appendMany(f.mdl,meshes,2);
        auto* header=at<studiohdr_t>(f.mdl,0);header->numtextures=2;header->textureindex=texturesOffset;header->numskinref=2;header->numskinfamilies=1;header->skinindex=skinsOffset;header->length=f.mdl.size();
        auto* model=at<mstudiomodel_t>(f.mdl,modelOffset);model->nummeshes=2;model->meshindex=meshesOffset-modelOffset;
        const size_t meshesVtx=f.vtx.size();MeshHeader_t vmeshes[2]{};appendMany(f.vtx,vmeshes,2);
        for(unsigned i=0;i<2;++i){const size_t gb=f.vtx.size();StripGroupHeader_t g=group;g.numIndices=18;append(f.vtx,g);
            const size_t sb=f.vtx.size();StripHeader_t s=strip;s.numIndices=18;s.indexOffset=0;append(f.vtx,s);
            const size_t mb=appendMany(f.vtx,map,8),ib=appendMany(f.vtx,indices+i*18,18);
            auto* group=at<StripGroupHeader_t>(f.vtx,gb);group->vertOffset=mb-gb;group->indexOffset=ib-gb;group->stripOffset=sb-gb;
            auto* vm=at<MeshHeader_t>(f.vtx,meshesVtx+i*sizeof(MeshHeader_t));vm->numStripGroups=1;vm->stripGroupHeaderOffset=gb-(meshesVtx+i*sizeof(MeshHeader_t));}
        auto* lod=at<ModelLODHeader_t>(f.vtx,lodOffset);lod->numMeshes=2;lod->meshOffset=meshesVtx-lodOffset;
    }
    return f;
}

bool parseStudioModel(const std::vector<std::uint8_t>& mdl,const std::vector<std::uint8_t>& vvd,
    const std::vector<std::uint8_t>& vtx,StudioMesh& output,std::string& error,const std::vector<std::uint8_t>& ani){
    StudioMesh result;auto fail=[&](const char* why){error=why;return false;};
    if(mdl.size()<sizeof(studiohdr_t)||vvd.size()<sizeof(vertexFileHeader_t)||vtx.size()<sizeof(OptimizedModel::FileHeader_t))return fail("truncated MDL/VVD/VTX header");
    if(mdl.size()>maximumModelFile||vvd.size()>maximumModelFile||vtx.size()>maximumModelFile||ani.size()>maximumModelFile)return fail("model companion exceeds 32 MiB limit");
    const auto* mh=at<studiohdr_t>(mdl,0);const auto* vh=at<vertexFileHeader_t>(vvd,0);const auto* fh=at<OptimizedModel::FileHeader_t>(vtx,0);
    if(mh->id!=idStudioHeader||(mh->version!=48&&mh->version!=49)||mh->length<int(sizeof(studiohdr_t))||size_t(mh->length)>mdl.size())return fail("unsupported MDL signature/version/length");
    auto boundsValid=[](const Vector& lo,const Vector& hi){for(int axis=0;axis<3;++axis)if(!std::isfinite(lo[axis])||!std::isfinite(hi[axis])||lo[axis]>hi[axis]||std::abs(lo[axis])>32768||std::abs(hi[axis])>32768)return false;return true;};
    if(!boundsValid(mh->hull_min,mh->hull_max)||!boundsValid(mh->view_bbmin,mh->view_bbmax))return fail("invalid studio collision/render bounds");
    result.hullMins=mh->hull_min;result.hullMaxs=mh->hull_max;result.renderMins=mh->view_bbmin;result.renderMaxs=mh->view_bbmax;
    if(vh->id!=MODEL_VERTEX_FILE_ID||vh->version!=MODEL_VERTEX_FILE_VERSION||vh->numLODs<1||vh->numLODs>MAX_NUM_LODS
        ||vh->numFixups<0||vh->numFixups>int(maximumModelVertices))return fail("unsupported VVD header or fixups");
    if(fh->version!=OPTIMIZED_MODEL_FILE_VERSION||fh->numLODs<1||fh->numLODs>MAX_NUM_LODS)return fail("unsupported VTX header");
    if(mh->checksum!=vh->checksum||mh->checksum!=fh->checkSum)return fail("MDL/VVD/VTX checksum mismatch");
    result.checksum=mh->checksum;
    auto stringAt=[&](size_t offset,std::string& out){if(offset>=size_t(mh->length))return false;out.clear();for(size_t i=offset;i<size_t(mh->length)&&out.size()<240;++i){if(!mdl[i])return true;out.push_back(char(mdl[i]));}return false;};
    if(mh->numanimblocks<0||mh->numanimblocks>1024)return fail("animation block count exceeds limit");
    if(mh->numanimblocks>1){if(mh->szanimblocknameindex<=0||!stringAt(mh->szanimblocknameindex,result.animationPath)||result.animationPath.empty()
            ||!at<mstudioanimblock_t>(mdl,mh->animblockindex,mh->numanimblocks))return fail("invalid external animation filename or block table");}
    if(mh->numtextures<0||mh->numtextures>256||mh->numcdtextures<0||mh->numcdtextures>32)return fail("invalid studio material counts");
    const auto* textures=at<mstudiotexture_t>(mdl,mh->textureindex,mh->numtextures);const auto* directories=at<int>(mdl,mh->cdtextureindex,mh->numcdtextures);if(!textures||!directories)return fail("studio materials outside MDL");
    for(int slot=0;slot<mh->numtextures;++slot){std::string name;size_t offset=0;
        if(!addRelative(size_t(mh->textureindex)+size_t(slot)*sizeof(mstudiotexture_t),textures[slot].sznameindex,offset)||!stringAt(offset,name)||name.empty())return fail("invalid studio material name");
        std::vector<std::string> paths;if(!mh->numcdtextures)paths.push_back(name);
        for(int d=0;d<mh->numcdtextures;++d){std::string directory;if(directories[d]<0||!stringAt(size_t(directories[d]),directory))return fail("invalid studio material directory");paths.push_back(directory+name);}
        result.materials.push_back(std::move(paths));}
    if(!result.materials.empty())result.materialPaths=result.materials.front();
    if(mh->numskinref<0||mh->numskinref>256||mh->numskinfamilies<0||mh->numskinfamilies>256||(mh->numskinref==0)!=(mh->numskinfamilies==0))return fail("invalid skin table counts");
    const short* skinRefs=nullptr;
    if(mh->numskinref){const size_t count=size_t(mh->numskinref)*mh->numskinfamilies;skinRefs=at<short>(mdl,mh->skinindex,count);if(!skinRefs)return fail("skin table outside MDL");
        for(size_t i=0;i<count;++i)if(skinRefs[i]<0||skinRefs[i]>=mh->numtextures)return fail("skin texture index outside material table");}
    if(mh->numbones<0||mh->numbones>256)return fail("invalid studio bone count");
    const auto* bones=at<mstudiobone_t>(mdl,mh->boneindex,mh->numbones);if(!bones)return fail("studio bones outside file");
    for(int i=0;i<mh->numbones;++i){const auto& bone=bones[i];
        if(bone.parent < -1||bone.parent>=i||!bone.pos.IsValid()||bone.pos.LengthSqr()>32768.f*32768.f)return fail("invalid bone hierarchy or position");
        float norm=0;for(int q=0;q<4;++q){if(!std::isfinite(bone.quat[q]))return fail("nonfinite bone quaternion");norm+=bone.quat[q]*bone.quat[q];}if(std::abs(norm-1)>.01f)return fail("nonunit bone quaternion");
        for(int row=0;row<3;++row)for(int col=0;col<4;++col)if(!std::isfinite(bone.poseToBone[row][col])||std::abs(bone.poseToBone[row][col])>32768)return fail("invalid inverse bind matrix");
        result.bones.push_back({bone.parent,bone.pos,bone.quat,bone.poseToBone});
    }
    if(!readAnimations(mdl,*mh,bones,result,error,ani))return false;
    const int totalVertices=vh->numLODVertexes[0];if(totalVertices<=0||totalVertices>int(maximumModelVertices))return fail("invalid VVD vertex count");
    const auto* vertices=at<mstudiovertex_t>(vvd,vh->vertexDataStart,totalVertices);if(!vertices)return fail("VVD vertices outside file");
    std::vector<const mstudiovertex_t*> orderedVertices;orderedVertices.reserve(totalVertices);
    if(vh->numFixups){
        const auto* fixups=at<vertexFileFixup_t>(vvd,vh->fixupTableStart,vh->numFixups);if(!fixups)return fail("VVD fixup table outside file");
        for(int i=0;i<vh->numFixups;++i){const auto& fixup=fixups[i];
            if(fixup.lod<0||fixup.lod>=vh->numLODs||fixup.sourceVertexID<0||fixup.numVertexes<0
                ||size_t(fixup.sourceVertexID)>size_t(totalVertices)||size_t(fixup.numVertexes)>size_t(totalVertices)-size_t(fixup.sourceVertexID))return fail("invalid VVD fixup range");
            // Root LOD 0 keeps every range and reconstructs the mesh-ordered pool
            // exactly like Studio_LoadVertexes in the original engine.
            if(size_t(fixup.numVertexes)>size_t(totalVertices)-orderedVertices.size())return fail("VVD fixup output exceeds root LOD");
            for(int j=0;j<fixup.numVertexes;++j)orderedVertices.push_back(vertices+fixup.sourceVertexID+j);
        }
        if(orderedVertices.size()!=size_t(totalVertices))return fail("VVD fixups do not rebuild root LOD");
    }else for(int i=0;i<totalVertices;++i)orderedVertices.push_back(vertices+i);
    if(mh->numbodyparts<=0||mh->numbodyparts>64||fh->numBodyParts!=mh->numbodyparts)return fail("bodypart hierarchy mismatch");
    const auto* bodies=at<mstudiobodyparts_t>(mdl,mh->bodypartindex,mh->numbodyparts);const auto* vBodies=at<OptimizedModel::BodyPartHeader_t>(vtx,fh->bodyPartOffset,fh->numBodyParts);if(!bodies||!vBodies)return fail("bodyparts outside file");
    for(int bi=0;bi<mh->numbodyparts;++bi){size_t modelBase=0,vModelBase=0;if(bodies[bi].nummodels<=0||bodies[bi].nummodels>64||vBodies[bi].numModels!=bodies[bi].nummodels||!addRelative(mh->bodypartindex+bi*sizeof(*bodies),bodies[bi].modelindex,modelBase)||!addRelative(fh->bodyPartOffset+bi*sizeof(*vBodies),vBodies[bi].modelOffset,vModelBase))return fail("model hierarchy mismatch");
        const auto* models=at<mstudiomodel_t>(mdl,modelBase,bodies[bi].nummodels);const auto* vModels=at<OptimizedModel::ModelHeader_t>(vtx,vModelBase,vBodies[bi].numModels);if(!models||!vModels)return fail("models outside file");
        for(int mi=0;mi<bodies[bi].nummodels;++mi){const auto& model=models[mi];const auto& vModel=vModels[mi];if(model.nummeshes<=0||model.nummeshes>256||model.numvertices<0||model.vertexindex<0
                ||model.vertexindex%int(sizeof(mstudiovertex_t))||vModel.numLODs<1)return fail("invalid model counts");
            size_t meshBase=0,lodBase=0;if(!addRelative(modelBase+mi*sizeof(*models),model.meshindex,meshBase)||!addRelative(vModelBase+mi*sizeof(*vModels),vModel.lodOffset,lodBase))return fail("mesh/LOD offset overflow");
            const auto* meshes=at<mstudiomesh_t>(mdl,meshBase,model.nummeshes);const auto* lod=at<OptimizedModel::ModelLODHeader_t>(vtx,lodBase);if(!meshes||!lod||lod->numMeshes!=model.nummeshes)return fail("mesh hierarchy mismatch");
            size_t vMeshBase=0;if(!addRelative(lodBase,lod->meshOffset,vMeshBase))return fail("VTX mesh offset overflow");const auto* vMeshes=at<OptimizedModel::MeshHeader_t>(vtx,vMeshBase,lod->numMeshes);if(!vMeshes)return fail("VTX meshes outside file");
            for(int xi=0;xi<model.nummeshes;++xi){const auto& mesh=meshes[xi];const auto& vm=vMeshes[xi];if(mesh.numvertices<0||mesh.vertexoffset<0||size_t(mesh.vertexoffset)+mesh.numvertices>size_t(model.numvertices)||vm.numStripGroups<=0||vm.numStripGroups>64)return fail("invalid mesh vertex range");
                if(mesh.material<0 || (skinRefs?mesh.material>=mh->numskinref:mh->numtextures?mesh.material>=mh->numtextures:mesh.material!=0))return fail("mesh material outside skin or texture table");
                const unsigned material=skinRefs?unsigned(skinRefs[mesh.material]):unsigned(mesh.material);
                size_t groupBase=0;if(!addRelative(vMeshBase+xi*sizeof(*vMeshes),vm.stripGroupHeaderOffset,groupBase))return fail("strip group offset overflow");
                const bool mdl49Groups=(vm.flags&OptimizedModel::MESH_IS_MDL49)!=0;const size_t groupStride=mdl49Groups?sizeof(OptimizedModel::StripGroupHeader_v49_t):sizeof(OptimizedModel::StripGroupHeader_t);
                if(!at<std::uint8_t>(vtx,groupBase,size_t(vm.numStripGroups)*groupStride))return fail("strip groups outside file");
                for(int gi=0;gi<vm.numStripGroups;++gi){const size_t gb=groupBase+size_t(gi)*groupStride;const auto* group=at<OptimizedModel::StripGroupHeader_t>(vtx,gb);if(!group)return fail("strip group outside file");
                    if(group->numVerts<=0||group->numVerts>int(maximumModelVertices)||group->numIndices<=0||group->numIndices>int(maximumModelTriangles*3)||group->numStrips<=0||group->numStrips>4096)return fail("invalid strip group counts");
                    size_t mapBase=0,indexBase=0,stripBase=0;if(!addRelative(gb,group->vertOffset,mapBase)||!addRelative(gb,group->indexOffset,indexBase)||!addRelative(gb,group->stripOffset,stripBase))return fail("strip group offset overflow");
                    const auto* maps=at<OptimizedModel::Vertex_t>(vtx,mapBase,group->numVerts);const auto* indices=at<unsigned short>(vtx,indexBase,group->numIndices);if(!maps||!indices)return fail("strip data outside file");
                    const bool mdl49Strips=mdl49Groups||(group->flags&OptimizedModel::STRIPGROUP_IS_MDL49);const size_t stripStride=mdl49Strips?sizeof(OptimizedModel::StripHeader_v49_t):sizeof(OptimizedModel::StripHeader_t);
                    if(!at<std::uint8_t>(vtx,stripBase,size_t(group->numStrips)*stripStride))return fail("strips outside file");
                    auto emit=[&](unsigned mapIndex){if(mapIndex>=unsigned(group->numVerts)){error="VTX index outside vertex map";return false;}const unsigned local=maps[mapIndex].origMeshVertID;if(local>=unsigned(mesh.numvertices)){error="VTX remap outside MDL mesh";return false;}
                        const size_t first=size_t(model.vertexindex)/sizeof(mstudiovertex_t);if(first>size_t(totalVertices)||size_t(mesh.vertexoffset)>size_t(totalVertices)-first||size_t(local)>size_t(totalVertices)-first-size_t(mesh.vertexoffset)){error="invalid VVD vertex";return false;}
                        const size_t global=first+size_t(mesh.vertexoffset)+size_t(local);if(global>=orderedVertices.size()||!finiteVertex(*orderedVertices[global])){error="invalid VVD vertex";return false;}const auto& source=*orderedVertices[global];
                        StudioVertex vertex{source.m_vecPosition,source.m_vecNormal,source.m_vecTexCoord};
                        vertex.material=material;
                        if(!result.bones.empty()){const auto& bw=source.m_BoneWeights;if(bw.numbones<1||bw.numbones>3){error="invalid bone influence count";return false;}float sum=0;vertex.influences=bw.numbones;
                            for(unsigned b=0;b<vertex.influences;++b){if(bw.bone[b]>=result.bones.size()||!std::isfinite(bw.weight[b])||bw.weight[b]<0||bw.weight[b]>1){error="invalid bone weight or index";return false;}vertex.bones[b]=bw.bone[b];vertex.weights[b]=bw.weight[b];sum+=bw.weight[b];}if(std::abs(sum-1)>.01f){error="bone weights do not sum to one";return false;}}
                        result.triangles.push_back(vertex);return true;};
                    for(int si=0;si<group->numStrips;++si){const auto* strip=at<OptimizedModel::StripHeader_t>(vtx,stripBase+size_t(si)*stripStride);if(!strip)return fail("strip outside file");const unsigned topology=strip->flags&(OptimizedModel::STRIP_IS_TRILIST|OptimizedModel::STRIP_IS_TRISTRIP);
                        if((topology!=OptimizedModel::STRIP_IS_TRILIST&&topology!=OptimizedModel::STRIP_IS_TRISTRIP)||strip->numIndices<3||strip->indexOffset<0||strip->indexOffset>group->numIndices||strip->numIndices>group->numIndices-strip->indexOffset||
                            (topology==OptimizedModel::STRIP_IS_TRILIST&&strip->numIndices%3))return fail("unsupported or invalid VTX strip");
                        const size_t triangles=topology==OptimizedModel::STRIP_IS_TRILIST?size_t(strip->numIndices/3):size_t(strip->numIndices-2);if(result.triangles.size()/3+triangles>maximumModelTriangles)return fail("model triangle limit exceeded");
                        if(topology==OptimizedModel::STRIP_IS_TRILIST){for(int ii=0;ii<strip->numIndices;++ii)if(!emit(indices[strip->indexOffset+ii]))return false;}
                        else for(int ii=2;ii<strip->numIndices;++ii){unsigned a=indices[strip->indexOffset+ii-2],b=indices[strip->indexOffset+ii-1],c=indices[strip->indexOffset+ii];if(ii&1)std::swap(a,b);if(a==b||b==c||a==c)continue;if(!emit(a)||!emit(b)||!emit(c))return false;}
                    }
                }++result.meshes;
            }
        }
    }
    if(result.triangles.empty())return fail("model contains no triangles");result.sourceVertices=totalVertices;output=std::move(result);error.clear();return true;
}
bool skinStudioModel(const StudioMesh& model,const std::vector<Quaternion>& rotations,std::vector<StudioVertex>& output,const std::vector<Vector>& positions){
    if(!rotations.empty()&&rotations.size()!=model.bones.size())return false;
    if(!positions.empty()&&positions.size()!=model.bones.size())return false;
    std::vector<matrix3x4_t> world(model.bones.size()),skin(model.bones.size());
    for(size_t i=0;i<model.bones.size();++i){const auto& bone=model.bones[i];if(bone.parent < -1||bone.parent>=int(i))return false;const auto& q=rotations.empty()?bone.rotation:rotations[i];float norm=0;for(int j=0;j<4;++j){if(!std::isfinite(q[j]))return false;norm+=q[j]*q[j];}if(std::abs(norm-1)>.01f)return false;
        const auto& position=positions.empty()?bone.position:positions[i];if(!position.IsValid())return false;matrix3x4_t local;QuaternionMatrix(q,position,local);if(bone.parent>=0)ConcatTransforms(world[bone.parent],local,world[i]);else MatrixCopy(local,world[i]);ConcatTransforms(world[i],bone.poseToBone,skin[i]);}
    auto staged=model.triangles;for(auto& vertex:staged){if(!vertex.influences)continue;if(vertex.influences>3)return false;Vector position(0,0,0),normal(0,0,0);
        for(unsigned b=0;b<vertex.influences;++b){if(vertex.bones[b]>=skin.size()||!std::isfinite(vertex.weights[b]))return false;Vector p,n;VectorTransform(vertex.position,skin[vertex.bones[b]],p);VectorRotate(vertex.normal,skin[vertex.bones[b]],n);position+=p*vertex.weights[b];normal+=n*vertex.weights[b];}
        if(!position.IsValid()||!normal.IsValid()||normal.LengthSqr()<1e-8f)return false;VectorNormalize(normal);vertex.position=position;vertex.normal=normal;}
    output=std::move(staged);return true;
}
bool sampleStudioAnimation(const StudioMesh& model,unsigned animation,double seconds,StudioPose& pose){
    if(animation>=model.animations.size()||!std::isfinite(seconds)||seconds<0)return false;const auto& clip=model.animations[animation];if(clip.frames.empty())return false;
    const size_t last=clip.frames.size()-1;double frame=0;if(last){const double duration=double(last)/clip.fps;frame=clip.looping?std::fmod(seconds,duration)*clip.fps:std::min(seconds,duration)*clip.fps;frame=std::min(frame,double(last));}
    const size_t first=size_t(frame),next=std::min(first+1,last);const float blend=float(frame-first);auto staged=clip.frames[first];
    for(size_t b=0;b<model.bones.size();++b){QuaternionBlend(clip.frames[first].rotations[b],clip.frames[next].rotations[b],blend,staged.rotations[b]);staged.positions[b]=clip.frames[first].positions[b]*(1-blend)+clip.frames[next].positions[b]*blend;}
    pose=std::move(staged);return true;
}
}
