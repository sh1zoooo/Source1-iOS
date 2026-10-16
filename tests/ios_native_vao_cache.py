#!/usr/bin/env python3
"""Bounded-cache lifetime, state isolation and failure tests with GL mock."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).parent.parent / 'src/source'
source = r'''
#include "IOSNativeVAOCache.hpp"
#include <cassert>
#include <map>
#include <set>
using Cache = IOSNativeVAOCache<1, 2>;
struct GL {
    struct State {unsigned element = 0, mask = 0; Cache::Attribute a[16];};
    std::map<unsigned,State> vao;
    unsigned current = 0, array = 91, next = 1, draws = 0;
    unsigned expectedElement = 0, expectedBuffer = 0;
    bool fail = false;
    void glGenVertexArrays(int, unsigned *out) { *out = fail ? 0 : next++; }
    void glDeleteVertexArrays(int, const unsigned *v) { assert(current != *v); vao.erase(*v); }
    void glBindVertexArray(unsigned v) { current = v; }
    void glBindBuffer(unsigned target, unsigned b) {
        if(target == 0x8892) array = b; else vao[current].element = b;
    }
    void glVertexAttribPointer(unsigned i, int c, unsigned t, bool n, int s, const void *p) {
        auto &a = vao[current].a[i];
        a.buffer=array; a.components=c; a.type=t; a.normalized=n; a.stride=s;
        a.offset=reinterpret_cast<std::uintptr_t>(p);
    }
    void glEnableVertexAttribArray(unsigned i) {vao[current].mask |= 1u<<i;}
    void glDrawElements(unsigned,int,unsigned,const void*) {
        ++draws;
        assert(vao[current].element == expectedElement);
        assert(vao[current].a[0].buffer == expectedBuffer);
    }
} gl;
int main() {
    Cache cache;
    Cache::Key key;
    key.mask=1; key.indexBuffer=5;
    key.attributes[0].buffer=6; key.attributes[0].type=0x1406;
    key.attributes[0].components=2; key.attributes[0].stride=8;
    auto canonical = [&](const Cache::Key &k) {
        assert(gl.current==0);
        gl.vao[0].element=k.indexBuffer;
        gl.vao[0].a[0]=k.attributes[0];
        gl.expectedElement=k.indexBuffer; gl.expectedBuffer=k.attributes[0].buffer;
    };
    auto draw = [&](const Cache::Key &k) {
        canonical(k);
        cache.draw(gl,k,91,4,6,0x1403,nullptr);
        assert(gl.current==0 && gl.array==91);
        assert(gl.vao[0].element==k.indexBuffer);
        assert(gl.vao[0].a[0]==k.attributes[0]);
    };
    // Thousands of never-repeated offsets bypass without GL allocation churn.
    for (unsigned i=1; i<=1000; ++i) {key.attributes[0].offset=i*8; draw(key);}
    assert(gl.next==1 && cache.counters.admissions==0);
    key.attributes[0].offset=0;
    const auto priorMisses=cache.counters.misses;
    draw(key); draw(key); draw(key);
    assert(cache.counters.hits==1 && cache.counters.misses==priorMisses+2);
    // Inactive descriptors never change the cache identity.
    key.attributes[9].buffer=999; draw(key);
    assert(cache.counters.hits==2);
    // Offset, type/format, index buffer and buffer identity must distinguish.
    for(unsigned i=1;i<6;++i) {key.attributes[0].offset=i*8; draw(key); draw(key);}
    assert(cache.counters.evictions==4 && gl.vao.size()==3); // two cached + canonical.
    // A cached VAO retains storage references: remove all before buffer deletion.
    cache.invalidateBuffer(gl,6);
    assert(gl.vao.size()==1 && cache.counters.invalidations==2);
    // Reusing the same GLuint requires a newly populated VAO.
    const auto misses=cache.counters.misses;
    draw(key); assert(cache.counters.misses==misses+1);
    // Neither index nor vertex zero/client arrays are ever cached.
    key.indexBuffer=0; draw(key);
    key.indexBuffer=5; key.attributes[0].buffer=0; draw(key);
    assert(cache.counters.misses==misses+1);
    cache.clear(gl); assert(gl.vao.size()==1);
    key.attributes[0].buffer=6;
    gl.fail=true; draw(key); draw(key); // glGen failure keeps VAO 0 and its state untouched.
    assert(gl.current==0 && gl.vao.size()==1);
    cache.clear(gl);
}
'''
with tempfile.TemporaryDirectory(prefix='ios-vao-unit-') as folder:
    path = Path(folder)
    (path/'test.cpp').write_text(source)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(root), str(path/'test.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
print('Native VAO boundedness, eviction, inactive attributes, invalidation/reuse, state isolation and allocation failure passed.')
