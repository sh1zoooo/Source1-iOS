#include "bitmap/imageformat.h"
#include "bitmap/mobile_astc.h"
#include "vtf/vtf.h"
#include "tier1/utlbuffer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

class IFileSystem;
IFileSystem *g_pFullFileSystem = nullptr; // tests deserialize in-memory buffers

static void solid(unsigned char *p, int bytes) {
    // Valid ASTC LDR void-extent constant: RGBA=(17,93,201,127).
    const unsigned char block[16] = {0xfc,0xfd,0xff,0xff,0xff,0xff,0xff,0xff,
                                   17,17,93,93,201,201,127,127};
    assert(bytes % 16 == 0);
    for (int n=0;n<bytes;n+=16) std::memcpy(p+n,block,16);
}
int main(int argc, char **argv) {
    static_assert(IMAGE_FORMAT_DXT1==13 && IMAGE_FORMAT_DXT5==15, "legacy ABI");
    static_assert(IMAGE_FORMAT_DXT1_RUNTIME==39 && IMAGE_FORMAT_DXT5_RUNTIME==40, "legacy ABI");
    static_assert(IMAGE_FORMAT_ASTC_4x4==43 && IMAGE_FORMAT_ASTC_6x6==47, "CM2 VTF ABI");
    for (int f : {43,47}) {
        const auto format=ImageFormat(f);
        assert(ImageLoader::IsCompressed(format));
        assert(ImageLoader::D3DFormatToImageFormat(ImageLoader::ImageFormatToD3DFormat(format))==format);
        for (int w : {1,2,4,6,7,16,64,512}) for (int h : {1,2,4,6,11,32}) {
            int b=f==43?4:6;
            int expected=((w+b-1)/b)*((h+b-1)/b)*16;
            assert(ImageLoader::GetMemRequired(w,h,1,format,false)==expected);
            std::vector<unsigned char> input(expected), output(size_t(w)*h*4);
            solid(input.data(),expected);
            assert(ImageLoader::ConvertImageFormat(input.data(),format,output.data(),IMAGE_FORMAT_RGBA8888,w,h));
            for (size_t n=0;n<output.size();n+=4) {
                assert(output[n]==17 && output[n+1]==93 && output[n+2]==201 && output[n+3]==127);
            }
            // Padded block rows must copy without changing payload or padding.
            int row=((w+b-1)/b)*16, rows=(h+b-1)/b;
            std::vector<unsigned char> copied((row+16)*rows,0x5a);
            assert(ImageLoader::ConvertImageFormat(input.data(),format,copied.data(),format,w,h,0,row+16));
            for(int y=0;y<rows;++y) {
                assert(!std::memcmp(copied.data()+y*(row+16),input.data()+y*row,row));
                for(int x=row;x<row+16;++x) assert(copied[y*(row+16)+x]==0x5a);
            }
            assert(!MobileASTC::decodeBGRA(input.data(),expected-1,w,h,f,output.data(),output.size()));
        }
        for (int frames : {1,2}) for (int cube : {0,int(TEXTUREFLAGS_ENVMAP)}) {
            IVTFTexture *a=CreateVTFTexture();
            assert(a->Init(16,16,1,format,cube,frames));
            for (int mip=0;mip<a->MipCount();++mip) {
                int w,h,d;a->ComputeMipLevelDimensions(mip,&w,&h,&d);
                const int bytes=ImageLoader::GetMemRequired(w,h,d,format,false);
                assert(a->RowSizeInBytes(mip)==MobileASTC::bytes(w,1,1,f));
                assert(a->FaceSizeInBytes(mip)==bytes);
                for(int frame=0;frame<frames;++frame) for(int face=0;face<a->FaceCount();++face)
                    solid(a->ImageData(frame,face,mip),bytes);
            }
            CUtlBuffer encoded; assert(a->Serialize(encoded));
            IVTFTexture *parsed=CreateVTFTexture(); assert(parsed->Unserialize(encoded));
            assert(parsed->Format()==format && parsed->FrameCount()==frames);
            assert(parsed->ComputeTotalSize()==a->ComputeTotalSize());
            assert(!std::memcmp(parsed->ImageData(),a->ImageData(),a->ComputeTotalSize()));
            encoded.SeekGet(CUtlBuffer::SEEK_HEAD,0);
            IVTFTexture *skipped=CreateVTFTexture(); assert(skipped->Unserialize(encoded,false,2));
            assert(skipped->Width()==4 && skipped->Height()==4);
            parsed->ConvertImageFormat(IMAGE_FORMAT_BGRA8888,false);
            assert(parsed->ImageData()[0]==201 && parsed->ImageData()[2]==17);
            DestroyVTFTexture(skipped);DestroyVTFTexture(parsed);DestroyVTFTexture(a);
        }
    }
    // Malformed ASTC blocks must decode to error pixels without reading outside
    // their 16-byte block. Also run this corpus in the standalone ASan check.
    std::mt19937 rng(123);
    for(int i=0;i<10000;++i) {
        unsigned char input[16];for(auto &v:input) v=static_cast<unsigned char>(rng());
        unsigned char pixels[6*6*4];
        assert(MobileASTC::decodeBGRA(input,16,6,6,47,pixels,sizeof(pixels)));
    }
    // Optional actual CM2 files, kept outside the repository.
    for(int n=1;n<argc;++n) {
        std::ifstream f(argv[n],std::ios::binary);
        std::vector<char> data((std::istreambuf_iterator<char>(f)),{}); assert(!data.empty());
        CUtlBuffer buf(data.data(),int(data.size()),CUtlBuffer::READ_ONLY);
        IVTFTexture *vtf=CreateVTFTexture();assert(vtf->Unserialize(buf));
        assert(vtf->MipCount()==static_cast<unsigned char>(data[56]));
        std::printf("Real VTF: %s %dx%d format=%d mips=%d bytes=%d\n",argv[n],vtf->Width(),vtf->Height(),vtf->Format(),vtf->MipCount(),vtf->ComputeTotalSize());
        vtf->ConvertImageFormat(IMAGE_FORMAT_RGBA8888,false);assert(vtf->Format()==IMAGE_FORMAT_RGBA8888);
        DestroyVTFTexture(vtf);
        CUtlBuffer shortBuf(data.data(),int(data.size()-1),CUtlBuffer::READ_ONLY);
        vtf=CreateVTFTexture();assert(!vtf->Unserialize(shortBuf));DestroyVTFTexture(vtf);
    }
    std::puts("ASTC 4x4/6x6: pixel channels, alpha, block rows, VTF mips/frames/cubes and truncation passed");
}
