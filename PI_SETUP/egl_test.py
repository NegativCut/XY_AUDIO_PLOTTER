#!/usr/bin/env python3
# Minimal EGL/GLES2 fullscreen test — clears screen to black, exits on Ctrl+C

import ctypes
import OpenGL.EGL as egl
import OpenGL.GLES2.VERSION.GLES2_2_0 as gl

def get_egl_display():
    # Try GBM device first
    import os, glob
    cards = sorted(glob.glob('/dev/dri/renderD*'))
    for card in cards:
        try:
            gbm = ctypes.CDLL('libgbm.so.1')
            fd = os.open(card, os.O_RDWR | os.O_CLOEXEC)
            dev = gbm.gbm_create_device(fd)
            if dev:
                dpy = egl.eglGetDisplay(ctypes.cast(dev, ctypes.c_void_p))
                if dpy != egl.EGL_NO_DISPLAY:
                    return dpy, fd, dev, gbm
            os.close(fd)
        except Exception:
            pass
    return None, None, None, None

def main():
    dpy, fd, gbm_dev, gbm = get_egl_display()
    if dpy is None:
        print('No EGL display found')
        return

    major, minor = egl.EGLint(), egl.EGLint()
    egl.eglInitialize(dpy, ctypes.pointer(major), ctypes.pointer(minor))
    print(f'EGL {major.value}.{minor.value}')

    attribs = (egl.EGLint * 13)(
        egl.EGL_RED_SIZE,   8,
        egl.EGL_GREEN_SIZE, 8,
        egl.EGL_BLUE_SIZE,  8,
        egl.EGL_RENDERABLE_TYPE, egl.EGL_OPENGL_ES2_BIT,
        egl.EGL_SURFACE_TYPE, egl.EGL_PBUFFER_BIT,
        egl.EGL_NONE
    )
    config = egl.EGLConfig()
    num = egl.EGLint()
    egl.eglChooseConfig(dpy, attribs, ctypes.pointer(config), 1, ctypes.pointer(num))
    if num.value == 0:
        print('No EGL config found')
        return

    pbuf_attribs = (egl.EGLint * 5)(
        egl.EGL_WIDTH,  1024,
        egl.EGL_HEIGHT, 768,
        egl.EGL_NONE
    )
    surface = egl.eglCreatePbufferSurface(dpy, config, pbuf_attribs)
    if surface == egl.EGL_NO_SURFACE:
        print('No EGL surface')
        return

    ctx_attribs = (egl.EGLint * 3)(egl.EGL_CONTEXT_CLIENT_VERSION, 2, egl.EGL_NONE)
    egl.eglBindAPI(egl.EGL_OPENGL_ES_API)
    ctx = egl.eglCreateContext(dpy, config, egl.EGL_NO_CONTEXT, ctx_attribs)
    if ctx == egl.EGL_NO_CONTEXT:
        print('No EGL context')
        return

    egl.eglMakeCurrent(dpy, surface, surface, ctx)
    print('EGL context active')

    gl.glClearColor(0.0, 0.0, 0.0, 1.0)
    gl.glClear(gl.GL_COLOR_BUFFER_BIT)
    egl.eglSwapBuffers(dpy, surface)
    print('Frame rendered — GLES2 pipeline working')

    egl.eglDestroyContext(dpy, ctx)
    egl.eglDestroySurface(dpy, surface)
    egl.eglTerminate(dpy)

if __name__ == '__main__':
    main()
