#!/usr/bin/env python3
"""Headless EGL/Mesa test of KAMI's ACTUAL shipped final world supersample shader.

The test presents a 64x64 source to a 16x16 physical output. Thin 1-of-4
pixel stripes disappear when sampled once at the center but survive as
~25% brightness when the final shader integrates all 4x4 subpixel samples.
Tests real shader compile/link, GPU uniforms, drawing and glReadPixels.
This is a correctness check, not a physical RTX 5070 FPS benchmark.
"""
import ctypes as C
from pathlib import Path

E = C.CDLL("libEGL.so.1")
G = C.CDLL("libGL.so.1")
P = C.c_void_p
I = C.c_int
U = C.c_uint
F = C.c_float

def fn(lib, name, result, types):
    f = getattr(lib, name)
    f.restype = result
    f.argtypes = types
    return f

get = fn(E, "eglGetProcAddress", P, [C.c_char_p])
platform = C.CFUNCTYPE(P, U, P, C.POINTER(I))(get(b"eglGetPlatformDisplayEXT"))
display = platform(0x31DD, None, None)
assert fn(E, "eglInitialize", U, [P,P,P])(display, None, None)
assert fn(E, "eglBindAPI", U, [U])(0x30A2)
config = P()
count = I()
attrs = (I*9)(0x3033,1,0x3040,8,0x3024,8,0x3038,0,0)
assert fn(E, "eglChooseConfig", U, [P,C.POINTER(I),C.POINTER(P),I,C.POINTER(I)])(
    display, attrs, C.byref(config), 1, C.byref(count)) and count.value
ctxattrs = (I*7)(0x3098,3,0x30FB,3,0x30FD,1,0x3038)
ctx = fn(E, "eglCreateContext", P, [P,P,P,C.POINTER(I)])(
    display, config, None, ctxattrs)
assert ctx and fn(E, "eglMakeCurrent", U, [P,P,P,P])(display,None,None,ctx)

def gl(name, res, *types):
    return fn(G, "gl" + name, res, list(types))

root = Path(__file__).resolve().parents[2]
fragfile = root / "indra/newview/app_settings/shaders/class1/deferred/postDeferredNoDoFF.glsl"
frag = fragfile.read_text(encoding="utf-8-sig")
frag = """#version 330 core
#define HAS_DOF_CHROMA 0
vec3 clampHDRRange(vec3 color) { return clamp(color, 0.0, 1.0); }
""" + frag.replace("/*[EXTRA_CODE_HERE]*/", "")
vertex = """#version 330 core
out vec2 vary_fragcoord;
void main() {
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vary_fragcoord = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
"""

make_shader = gl("CreateShader",U,U)
src = gl("ShaderSource",None,U,I,C.POINTER(C.c_char_p),P)
compile_shader = gl("CompileShader",None,U)
check_shader = gl("GetShaderiv",None,U,U,C.POINTER(I))
shader_log = gl("GetShaderInfoLog",None,U,I,P,P)
def shader(kind, code):
    s = make_shader(kind)
    b = C.c_char_p(code.encode("utf-8"))
    src(s,1,C.byref(b),None)
    compile_shader(s)
    okay = I()
    check_shader(s,0x8B81,C.byref(okay))
    if not okay.value:
        log=C.create_string_buffer(8192)
        shader_log(s,8192,None,log)
        raise AssertionError(log.value.decode("utf-8","replace"))
    return s

prog = gl("CreateProgram",U)()
for kind,code in ((0x8B31,vertex),(0x8B30,frag)):
    gl("AttachShader",None,U,U)(prog,shader(kind,code))
gl("LinkProgram",None,U)(prog)
linked=I()
gl("GetProgramiv",None,U,U,C.POINTER(I))(prog,0x8B82,C.byref(linked))
if not linked.value:
    log=C.create_string_buffer(8192)
    gl("GetProgramInfoLog",None,U,I,P,P)(prog,8192,None,log)
    raise AssertionError(log.value.decode("utf-8","replace"))

make_tex=gl("GenTextures",None,I,C.POINTER(U))
bind_tex=gl("BindTexture",None,U,U)
image=gl("TexImage2D",None,U,I,I,I,I,I,U,U,P)
param=gl("TexParameteri",None,U,U,I)
def texture(w,h,data=None):
    n=U();make_tex(1,C.byref(n))
    bind_tex(0x0DE1,n.value)
    for pname,pvalue in ((0x2801,0x2601),(0x2800,0x2601),
                         (0x2802,0x812F),(0x2803,0x812F)):
        param(0x0DE1,pname,pvalue)
    buf=(F*(w*h*4))(*data) if data is not None else None
    image(0x0DE1,0,0x8814,w,h,0,0x1908,0x1406,buf)
    return n.value

SW=SH=64
OW=OH=16
pattern=[]
for y in range(SH):
    for x in range(SW):
        v=1.0 if x%4==0 else 0.0
        pattern.extend((v,v,v,1.0))
source_tex=texture(SW,SH,pattern)
output_tex=texture(OW,OH)
fbo=U()
gl("GenFramebuffers",None,I,C.POINTER(U))(1,C.byref(fbo))
gl("BindFramebuffer",None,U,U)(0x8D40,fbo.value)
gl("FramebufferTexture2D",None,U,U,U,U,I)(0x8D40,0x8CE0,0x0DE1,output_tex,0)
assert gl("CheckFramebufferStatus",U,U)(0x8D40)==0x8CD5
vao=U()
gl("GenVertexArrays",None,I,C.POINTER(U))(1,C.byref(vao))
gl("BindVertexArray",None,U)(vao.value)
gl("Viewport",None,I,I,I,I)(0,0,OW,OH)
gl("UseProgram",None,U)(prog)
gl("ActiveTexture",None,U)(0x84C0)
bind_tex(0x0DE1,source_tex)
location=gl("GetUniformLocation",I,U,C.c_char_p)
one=gl("Uniform1i",None,I,I)
two=gl("Uniform2f",None,I,F,F)
one(location(prog,b"diffuseRect"),0)
one(location(prog,b"depthMap"),0)
two(location(prog,b"screen_res"),SW,SH)
two(location(prog,b"kami_present_resolution"),OW,OH)
gl("Uniform1f",None,I,F)(location(prog,b"chroma_str"),0.0)
read=gl("ReadPixels",None,I,I,I,I,U,U,P)

def draw(grid):
    one(location(prog,b"kami_resolve_grid"),grid)
    gl("DrawArrays",None,U,I,I)(0x0004,0,3)
    data=(F*(OW*OH*4))()
    read(0,0,OW,OH,0x1908,0x1406,data)
    return [float(data[i]) for i in range(0,len(data),4)]

one_tap=draw(1)
four_by_four=draw(4)
avg_a=sum(one_tap)/len(one_tap)
avg_b=sum(four_by_four)/len(four_by_four)
assert avg_a < 0.03, f"One-tap sample unexpectedly averaged stripes: {avg_a}"
assert 0.19 < avg_b < 0.31, f"Supersampled scene detail lost: {avg_b}"
assert avg_b-avg_a > 0.17, "Increasing source pixels did not change presented image"
assert gl("GetError",U)() == 0
print(f"PASS: actual final GLSL compiled and resolved fine source details: "
      f"single-tap {avg_a:.3f} vs 4x4 area {avg_b:.3f}")
