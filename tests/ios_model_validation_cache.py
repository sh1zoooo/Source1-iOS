#!/usr/bin/env python3
"""Compile and execute the exact proposed validation/reset snippets."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile

proposal = Path(__file__).parent.parent / 'scripts/prepare_ios_model_validation_cache.py'
spec = importlib.util.spec_from_file_location('patch', proposal)
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)
root = proposal.parent.parent
production = patch.apply((root / 'third_party/clientmod/datacache/mdlcache.cpp').read_text())
signature = 'vertexFileHeader_t *CMDLCache::BuildAndCacheVertexData('
start = production.index(signature)
end = production.index(patch.CHECK_NEW, start) + len(patch.CHECK_NEW)
validation_prefix = production[start:end]
assert 'MDLHandle_t\thandle = VoidPtrToMDLHandle( pStudioHdr->VirtualModel() );' in validation_prefix
member_declaration = validation_prefix[:validation_prefix.index('\n{')].replace('CMDLCache::', '') + ';'
fixture = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#define Assert(x) assert(x)
#define MdlCacheMsg(...) ((void)0)
#define MODEL_VERTEX_FILE_ID 1
#define MODEL_VERTEX_FILE_VERSION 4
using MDLHandle_t = unsigned;
unsigned VoidPtrToMDLHandle(void *p) { return (unsigned)(uintptr_t)p; }
#define SOURCE_IOS 1
enum { STUDIODATA_FLAGS_NO_STUDIOMESH = 8,
       STUDIODATA_FLAGS_NO_VERTEX_DATA = 16,
       STUDIODATA_FLAGS_IOS_BAD_VVD_CHECKSUM = 128,
       unrelated = 32 };
struct studiohdr_t { int checksum; unsigned handle = 0; const char *pszName() const { return "test"; } void *VirtualModel() { return (void*)(uintptr_t)handle; } };
struct vertexFileHeader_t { int checksum; int id = MODEL_VERTEX_FILE_ID, version = MODEL_VERTEX_FILE_VERSION; };
using Studio = studiohdr_t;
using Vvd = vertexFileHeader_t;
struct Data { unsigned short m_nFlags = 0; };
int warnings = 0;
template<class... Args> void Warning(Args...) { ++warnings; }
Data data[2];
class CMDLCache {
public:
Data *m_MDLDict[2] = { &data[0], &data[1] };
@@MEMBER@@
};
@@GENUINE_PREFIX@@
(void)pVvdHdr;
return pRawVvdHdr;
}
CMDLCache cache;
void reset(Data *pStudioData) {
@@RESET@@
}
int reads[2] = {};
Vvd* load(unsigned handle, Studio *hdr, Vvd *vvd, bool fileReady = true) {
    if (data[handle].m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA) return nullptr;
    ++reads[handle];
    if (!fileReady) return nullptr;
    hdr->handle = handle;
    return cache.BuildAndCacheVertexData(hdr, vvd);
}
int main() {
    Studio mdl{10}; Vvd wrong{20}, right{10};
    assert(!load(0, &mdl, &wrong));
    for (int frame = 0; frame < 10000; ++frame) assert(!load(0, &mdl, &wrong));
    assert(reads[0] == 1 && warnings == 1);
    // A failed model does not suppress another handle or successful geometry.
    assert(load(1, &mdl, &right) == &right);
    // Hardware load derives this flag when it sees the negative VVD result.
    data[0].m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH | unrelated;
    reset(&data[0]);
    assert(data[0].m_nFlags == unrelated);
    assert(load(0, &mdl, &right) == &right);
    // Unready file is retried; this proposal never creates transient negatives.
    assert(!load(1, &mdl, &right, false));
    assert(load(1, &mdl, &right) == &right);
    // Reset never clears unrelated legacy failure states.
    data[1].m_nFlags |= STUDIODATA_FLAGS_NO_VERTEX_DATA;
    reset(&data[1]);
    assert(data[1].m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA);
    // Reusing a model handle creates a fresh zero-initialized studiodata_t.
    data[0] = Data{};
    assert(load(0, &mdl, &right) == &right);
}
'''.replace('@@GENUINE_PREFIX@@', validation_prefix).replace('@@MEMBER@@', member_declaration).replace('@@RESET@@', patch.RESET)
with tempfile.TemporaryDirectory(prefix='ios-model-cache-') as tmp:
    src = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test'
    src.write_text(fixture)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('VVD negative cache checks passed (10,000 invalid draws, reload, handle isolation, transient retry).')
