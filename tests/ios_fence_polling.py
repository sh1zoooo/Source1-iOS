"""Compile the actual patched EVENT GetData branch with deterministic GPU mocks."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(sys.argv[1])
text=(root/'togles/linuxwin/dxabstract.cpp').read_text()
start=text.index('HRESULT IDirect3DQuery9::GetData(')
start=text.index('case\tD3DQUERYTYPE_EVENT:',start)
end=text.index('\n\treturn result;',start)
branch=text[start:end].rstrip()
# Exclude the outer switch's closing brace, retaining the case and break.
branch=branch[:branch.rfind('}')]
query=(root/'togles/linuxwin/cglmquery.cpp').read_text()
start=query.index('const GLenum status = gGL->glClientWaitSync(m_syncobj, 0, 0);')
end=query.index('\n                }',start)
poll=query[start:end]
source=r'''
#include <cassert>
#include <stdexcept>
using GLenum=unsigned;
constexpr unsigned GL_ALREADY_SIGNALED=0x911A, GL_TIMEOUT_EXPIRED=0x911B,
GL_CONDITION_SATISFIED=0x911C, GL_WAIT_FAILED=0x911D;
struct MockGL { unsigned status=GL_TIMEOUT_EXPIRED, flushes=0, polls=0;
void glFlush(){++flushes;}
unsigned glClientWaitSync(void*,unsigned flags,unsigned timeout){assert(flags==0&&timeout==0);++polls;return status;}
unsigned glGetError(){return 0x502;}
} driver,*gGL=&driver;
unsigned warnings=0; void Warning(const char*,...){++warnings;}
struct Query {void *m_syncobj=nullptr; bool m_done=false;unsigned completes=0;
bool IsDone(){
POLL
return m_done;}
void Complete(void*){assert(m_done);++completes;}
};
#define Assert assert
constexpr int D3DQUERYTYPE_EVENT=1,S_FALSE=1,S_OK=0;
int get(Query *m_query,bool flush){int result=S_FALSE;bool done=false;
switch(D3DQUERYTYPE_EVENT){BRANCH
}
return result;}
int main(){ Query q;
// Pending FLUSH must submit and return S_FALSE without consuming/resetting.
assert(get(&q,true)==S_FALSE&&q.completes==0&&driver.flushes==1);
assert(get(&q,false)==S_FALSE&&driver.flushes==1);
for(unsigned status:{GL_ALREADY_SIGNALED,GL_CONDITION_SATISFIED}){
q.m_done=false;driver.status=status;unsigned before=q.completes;
assert(get(&q,false)==S_OK&&q.completes==before+1);}
q.m_done=false;driver.status=GL_WAIT_FAILED;assert(get(&q,true)==S_FALSE);
assert(!q.m_done&&q.completes==2&&warnings==1);
assert(get(&q,true)==S_FALSE&&warnings==1);
assert(driver.polls==6);
}
'''.replace('POLL',poll).replace('BRANCH',branch)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp); (p/'test.cpp').write_text(source)
 subprocess.run(['c++','-std=c++17','-DSOURCE_IOS=1','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production EVENT branch: pending submit/nonblocking poll, both completed statuses, bounded failure reporting passed')
