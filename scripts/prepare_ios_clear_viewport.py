#!/usr/bin/env python3
"""Use Source's existing explicit viewport clear path consistently on iOS.

Generic D3D Clear and stencil-obeying clears remain untouched. Source already
constructs the current-view rectangle and uses this path for partial targets;
select it for full targets too instead of inheriting unrelated GUI clip state.
"""
from pathlib import Path
import sys
p=Path(sys.argv[1])/'materialsystem/shaderapidx9/shaderapidx8.cpp'
s=p.read_text()
start=s.index('void CShaderAPIDx8::ClearBuffers( bool bClearColor,')
end=s.index('//-----------------------------------------------------------------------------\n// Bind',start)
region=s[start:end]
old='\t\tif ( bRenderTargetMatchesViewport )'
assert region.count(old)==1
new='''#ifdef SOURCE_IOS
        // Source intends to clear the current view (rectangle above). Always
        // use its existing rectangle path on iOS, including a full-size view;
        // the generic D3D Clear scissor semantics remain unchanged.
        bRenderTargetMatchesViewport = false;
        // Input-state diagnostics only: no GL queries or per-draw logging.
        // A hard cap prevents alternating render targets from flooding logs.
        static unsigned iosClearLogs = 0;
        static int iosLastClear[6] = {-1,-1,-1,-1,-1,-1};
        bool iosClearChanged = clear.x1 != iosLastClear[0] || clear.y1 != iosLastClear[1] ||
            clear.x2 != iosLastClear[2] || clear.y2 != iosLastClear[3] ||
            renderTargetWidth != iosLastClear[4] || renderTargetHeight != iosLastClear[5];
        if (iosClearLogs < 3 || (iosClearLogs < 32 && iosClearChanged)) {
            ++iosClearLogs;
            Warning("iOS viewport clear: sample=%u flags=0x%x rect=%d,%d..%d,%d target=%dx%d desired_scissor=%u\\n",
                iosClearLogs, (unsigned)mask, clear.x1, clear.y1, clear.x2, clear.y2,
                renderTargetWidth, renderTargetHeight,
                (unsigned)m_DesiredState.m_RenderState[D3DRS_SCISSORTESTENABLE]);
        }
        iosLastClear[0] = clear.x1; iosLastClear[1] = clear.y1;
        iosLastClear[2] = clear.x2; iosLastClear[3] = clear.y2;
        iosLastClear[4] = renderTargetWidth; iosLastClear[5] = renderTargetHeight;
#endif
\t\tif ( bRenderTargetMatchesViewport )'''
s=s[:start]+region.replace(old,new)+s[end:]
p.write_text(s)
