#!/usr/bin/env python3
"""Apply nonblocking iOS event-query polling after the graphical patches.

ES3 sync is already enabled by glentrypoints.cpp after extension detection.
Do not confuse its preliminary ARB-extension diagnostic with effective caps.
"""
from pathlib import Path
import sys
root = Path(sys.argv[1])
def replace(path, old, new):
    p = root / path
    text = p.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f'Fence polling patch context changed: {path}')
    p.write_text(text.replace(old, new))
replace('togles/linuxwin/cglmquery.cpp',
    '\t\t\t\tif (gGL->m_bHave_GL_ARB_sync)\n\t\t\t\t\tm_done = (gGL->glClientWaitSync( m_syncobj, 0, 0 ) == GL_ALREADY_SIGNALED);',
    '''\t\t\t\tif (gGL->m_bHave_GL_ARB_sync)
                {
                    const GLenum status = gGL->glClientWaitSync(m_syncobj, 0, 0);
                    m_done = status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED;
#ifdef SOURCE_IOS
                    static bool reportedWaitFailure = false;
                    if (status == GL_WAIT_FAILED && !reportedWaitFailure) {
                        reportedWaitFailure = true;
                        Warning("iOS event query polling failed: error=0x%x\\n", gGL->glGetError());
                    }
#endif
                }''')
replace('togles/linuxwin/dxabstract.cpp',
    '''\t\t\t\tdone = m_query->IsDone();
\t\t\t\tif ( ( done ) || ( flush ) )
\t\t\t\t{
\t\t\t\t\tm_query->Complete(NULL);\t// this will block on pre-SLGU''',
    '''#ifdef SOURCE_IOS
                // D3DGETDATA_FLUSH submits pending work; it does not require
                // CPU completion. Keep engine caller deadlines effective.
                if (flush) gGL->glFlush();
                done = m_query->IsDone();
                if (done)
#else
\t\t\t\tdone = m_query->IsDone();
\t\t\t\tif ( ( done ) || ( flush ) )
#endif
\t\t\t\t{
\t\t\t\t\tm_query->Complete(NULL);\t// iOS only consumes an already completed fence''')
p = root / 'togles/linuxwin/dxabstract.cpp'
text = p.read_text()
start = text.index('HRESULT IDirect3DQuery9::GetData(')
start = text.index('case\tD3DQUERYTYPE_EVENT:', start)
end = text.index('\n\treturn result;', start)
region = text[start:end]
assert region.count('Assert( !flush );') == 1
text = text[:start] + region.replace('Assert( !flush );', '#ifndef SOURCE_IOS\n                    Assert( !flush );\n#endif') + text[end:]
p.write_text(text)
