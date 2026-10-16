"""Exercise ES3 core sync objects without relying on ARB extension strings."""
import ctypes as c
import time
import gles_texture_channels as gl
fence=gl.fn('glFenceSync',gl.P,[gl.U,gl.U])
poll=gl.fn('glClientWaitSync',gl.U,[gl.P,gl.U,c.c_uint64])
flush=gl.fn('glFlush',None,[])
delete=gl.fn('glDeleteSync',None,[gl.P])
success=(0x911A,0x911C) # ALREADY_SIGNALED, CONDITION_SATISFIED
for i in range(32):
    gl.fn('glDrawArrays',None,[gl.U,gl.I,gl.I])(4,0,3)
    sync=fence(0x9117,0) # SYNC_GPU_COMMANDS_COMPLETE
    assert sync and gl.geterror()==0
    # FLUSH submits once; zero-timeout probes may legitimately be pending.
    flush()
    deadline=time.monotonic()+2
    status=poll(sync,0,0)
    while status==0x911B and time.monotonic()<deadline:
        time.sleep(.001)
        status=poll(sync,0,0)
    assert status in success,hex(status)
    delete(sync)
    assert gl.geterror()==0
print('ES3 core fences: 32 draw/submit/nonblocking poll/delete cycles passed')
