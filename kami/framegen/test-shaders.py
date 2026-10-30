#!/usr/bin/env python3
"""Headless Mesa/EGL execution of the actual shipped GLSL (Linux QA only)."""
import ctypes as C
import re
from pathlib import Path
E=C.CDLL('libEGL.so.1');G=C.CDLL('libGL.so.1')
def fn(lib,name,restype,args):
 f=getattr(lib,name);f.restype=restype;f.argtypes=args;return f
P=C.c_void_p;I=C.c_int;U=C.c_uint;F=C.c_float
get=fn(E,'eglGetProcAddress',P,[C.c_char_p])
platform=C.CFUNCTYPE(P,U,P,C.POINTER(I))(get(b'eglGetPlatformDisplayEXT'))
display=platform(0x31DD,None,None)
assert fn(E,'eglInitialize',U,[P,P,P])(display,None,None)
assert fn(E,'eglBindAPI',U,[U])(0x30A2)
attrs=(I*9)(0x3033,1,0x3040,8,0x3024,8,0x3038,0,0)
config=P();count=I()
assert fn(E,'eglChooseConfig',U,[P,C.POINTER(I),C.POINTER(P),I,C.POINTER(I)])(display,attrs,C.byref(config),1,C.byref(count)) and count.value
ctxattrs=(I*7)(0x3098,3,0x30FB,3,0x30FD,1,0x3038)
ctx=fn(E,'eglCreateContext',P,[P,P,P,C.POINTER(I)])(display,config,None,ctxattrs)
assert ctx and fn(E,'eglMakeCurrent',U,[P,P,P,P])(display,None,None,ctx)
def gl(name,r,*args):return fn(G,'gl'+name,r,list(args))
createShader=gl('CreateShader',U,U);source=gl('ShaderSource',None,U,I,C.POINTER(C.c_char_p),P)
compileShader=gl('CompileShader',None,U);getShader=gl('GetShaderiv',None,U,U,C.POINTER(I))
shaderLog=gl('GetShaderInfoLog',None,U,I,P,P)
text=(Path(__file__).parent/'kami_framegen.h').read_text()
codes=dict(re.findall(r'const char\* (\w+) = R"GLSL\((.*?)\)GLSL";',text,re.S))
def shader(kind,code):
 s=createShader(kind);v=C.c_char_p(code.encode());source(s,1,C.byref(v),None);compileShader(s);ok=I();getShader(s,0x8B81,C.byref(ok))
 if not ok.value:
  b=C.create_string_buffer(8192);shaderLog(s,8192,None,b);raise AssertionError(b.value)
 return s
def program(fragment):
 p=gl('CreateProgram',U)()
 for kind,code in [(0x8B31,codes['vertex']),(0x8B30,codes[fragment])]:gl('AttachShader',None,U,U)(p,shader(kind,code))
 gl('LinkProgram',None,U)(p);ok=I();gl('GetProgramiv',None,U,U,C.POINTER(I))(p,0x8B82,C.byref(ok));assert ok.value
 return p
flowProgram=program('flowFragment');outProgram=program('interpolateFragment')
genTex=gl('GenTextures',None,I,C.POINTER(U));bindTex=gl('BindTexture',None,U,U)
texImage=gl('TexImage2D',None,U,I,I,I,I,I,U,U,P);param=gl('TexParameteri',None,U,U,I)
w=h=64
previous=[];current=[]
for y in range(h):
 for x in range(w):
  previous.extend([float(16<=x<32 and 16<=y<48)]*3+[1.])
  current.extend([float(24<=x<40 and 16<=y<48)]*3+[1.])
def texture(data=None):
 t=U();genTex(1,C.byref(t));bindTex(0x0DE1,t.value)
 for n,v in [(0x2801,0x2601),(0x2800,0x2601),(0x2802,0x812F),(0x2803,0x812F)]:param(0x0DE1,n,v)
 buf=(F*(w*h*4))(*data) if data is not None else None
 texImage(0x0DE1,0,0x8814,w,h,0,0x1908,0x1406,buf);return t.value
textures=[texture(previous),texture(current),texture(),texture()]
fbo=U();gl('GenFramebuffers',None,I,C.POINTER(U))(1,C.byref(fbo));gl('BindFramebuffer',None,U,U)(0x8D40,fbo.value)
vao=U();gl('GenVertexArrays',None,I,C.POINTER(U))(1,C.byref(vao));gl('BindVertexArray',None,U)(vao.value)
gl('Viewport',None,I,I,I,I)(0,0,w,h)
uniform=gl('GetUniformLocation',I,U,C.c_char_p);one=gl('Uniform1i',None,I,I)
def bind(p):
 gl('UseProgram',None,U)(p)
 for i,name in enumerate([b'previousFrame',b'currentFrame',b'flow']):
  gl('ActiveTexture',None,U)(0x84C0+i);bindTex(0x0DE1,textures[i]);one(uniform(p,name),i)
def target(t):
 gl('FramebufferTexture2D',None,U,U,U,U,I)(0x8D40,0x8CE0,0x0DE1,t,0)
 assert gl('CheckFramebufferStatus',U,U)(0x8D40)==0x8CD5
read=gl('ReadPixels',None,I,I,I,I,U,U,P)
def pixels():
 b=(F*(w*h*4))();read(0,0,w,h,0x1908,0x1406,b);return list(b)
bind(flowProgram);target(textures[2]);gl('Uniform2f',None,I,F,F)(uniform(flowProgram,b'pixel'),1/w,1/h)
gl('DrawArrays',None,U,I,I)(4,0,3);flows=pixels()
assert max(flows[0::4])>0.1, 'Moving edge must produce nonzero forward optical flow'
bind(outProgram);target(textures[3]);one(uniform(outProgram,b'original'),1);gl('DrawArrays',None,U,I,I)(4,0,3)
a=pixels();assert max(abs(x-y) for x,y in zip(a,current))<1e-5, 'Native frame restoration must be exact'
one(uniform(outProgram,b'original'),0);gl('DrawArrays',None,U,I,I)(4,0,3);b=pixels()
assert any(abs(x-y)>.01 for x,y in zip(b,current)), 'Generated frame must differ from current image'
assert b[(32*w+22)*4]>.3, 'Moving edge should advance toward midpoint'
assert b[(5*w+5)*4]<1e-5, 'Static content must stay unchanged'
assert gl('GetError',U)()==0
print('PASS: GLSL compile/link, real flow dispatch, exact native restoration, interpolated moving edge, static content, GL errors')
