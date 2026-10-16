#!/usr/bin/env python3
"""Stop repeated reads of checksum-rejected VVD files on iOS.

Apply to a prepared ClientMod tree, after graphics preparation. No checksum is
bypassed. Only the deterministic VVD checksum failure receives a negative entry;
file-not-ready, transient I/O and allocation failures retain existing behavior.
"""
from pathlib import Path
import sys

FLAG_OLD = '\tSTUDIODATA_FLAGS_LOCKED_MDL\t\t\t= 0x0040,'
FLAG_NEW = FLAG_OLD + '''
#ifdef SOURCE_IOS
    STUDIODATA_FLAGS_IOS_BAD_VVD_CHECKSUM = 0x0080,
#endif'''

CHECK_OLD = '''	if ( pRawVvdHdr->checksum != pStudioHdr->checksum )
	{
		Warning( "Error Vertex File for '%s' checksum %d should be %d\\n", pStudioHdr->pszName(), pRawVvdHdr->checksum, pStudioHdr->checksum );
		return NULL;
	}'''
CHECK_NEW = CHECK_OLD.replace('\t\treturn NULL;', '''#ifdef SOURCE_IOS
        // The file is present but belongs to a different MDL generation. Retry
        // after explicit flush/map reload, rather than read it every draw.
        m_MDLDict[handle]->m_nFlags |= STUDIODATA_FLAGS_IOS_BAD_VVD_CHECKSUM |
                                     STUDIODATA_FLAGS_NO_VERTEX_DATA;
#endif
		return NULL;''')

RESET = '''#ifdef SOURCE_IOS
    if (pStudioData->m_nFlags & STUDIODATA_FLAGS_IOS_BAD_VVD_CHECKSUM) {
        pStudioData->m_nFlags &= ~(STUDIODATA_FLAGS_IOS_BAD_VVD_CHECKSUM |
                                  STUDIODATA_FLAGS_NO_VERTEX_DATA |
                                  STUDIODATA_FLAGS_NO_STUDIOMESH);
    }
#endif'''

FLUSH_OLD = '''	bool bIgnoreLock = ( nFlushFlags & MDLCACHE_FLUSH_IGNORELOCK ) != 0;
'''
FLUSH_NEW = FLUSH_OLD + '''
    // Explicit reload/content replacement starts a new validation generation.
    if (nFlushFlags & (MDLCACHE_FLUSH_STUDIOHDR | MDLCACHE_FLUSH_VERTEXES |
                       MDLCACHE_FLUSH_STUDIOHWDATA)) {
''' + RESET + '''
    }
'''
MAP_OLD = '''		pStudioData = m_MDLDict[i];
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_LOCKED_MDL )'''
MAP_NEW = '''		pStudioData = m_MDLDict[i];
        // Map search paths may have changed, including embedded asset packs.
''' + RESET + '''
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_LOCKED_MDL )'''

def apply(text):
    for old, new in ((FLAG_OLD, FLAG_NEW), (CHECK_OLD, CHECK_NEW),
                     (FLUSH_OLD, FLUSH_NEW), (MAP_OLD, MAP_NEW)):
        assert text.count(old) == 1, (old, text.count(old))
        text = text.replace(old, new)
    return text

if __name__ == '__main__':
    target = Path(sys.argv[1]) / 'datacache/mdlcache.cpp'
    target.write_text(apply(target.read_text()))
