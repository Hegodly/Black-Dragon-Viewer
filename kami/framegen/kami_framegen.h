// Copyright (C) 2026 KAMI contributors. SPDX-License-Identifier: MIT
// Experimental image-based optical-flow interpolation. Not DLSS/FSR FG.
#pragma once
#include <chrono>
#include <thread>
#include <algorithm>
namespace KAMIFrameGen {
using Clock = std::chrono::steady_clock;
inline const char* vertex = R"GLSL(#version 330 core
out vec2 uv;
void main(){ uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2); gl_Position=vec4(uv*2.-1.,0,1); }
)GLSL";
inline const char* flowFragment = R"GLSL(#version 330 core
in vec2 uv; out vec4 result;
uniform sampler2D previousFrame, currentFrame;
uniform vec2 pixel;
float errorAt(vec2 d){
 float e=0.;
 for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x){
  vec2 o=vec2(x,y)*pixel*3.;
  vec3 a=texture(previousFrame,uv+o).rgb;
  vec3 b=texture(currentFrame,uv+o+d).rgb;
  e+=dot(abs(a-b),vec3(.299,.587,.114));
 } return e/9.;
}
void main(){
 vec2 best=vec2(0); float initial=errorAt(best), e=initial;
 // Coarse-to-fine block search, in full-resolution pixels.
 for(int level=0;level<3;++level){
  float stepSize=level==0?8.:(level==1?2.:1.);
  vec2 center=best;
  for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x){
   vec2 d=center+vec2(x,y)*pixel*stepSize; float v=errorAt(d);
   if(v<e){e=v;best=d;}
  }
 }
 // Reject changed/occluded blocks rather than making long ghost trails.
 float confidence=1.-smoothstep(.035,.16,e);
 result=vec4(best,confidence,initial);
}
)GLSL";
inline const char* interpolateFragment = R"GLSL(#version 330 core
in vec2 uv; out vec4 result;
uniform sampler2D previousFrame,currentFrame,flow;
uniform int original;
void main(){
 vec4 now=texture(currentFrame,uv);
 if(original!=0){result=now;return;}
 vec4 f=texture(flow,uv);
 vec4 a=texture(previousFrame,uv-f.xy*.5);
 vec4 b=texture(currentFrame,uv+f.xy*.5);
 float mismatch=dot(abs(a.rgb-b.rgb),vec3(.299,.587,.114));
 float trust=f.z*(1.-smoothstep(.06,.2,mismatch));
 // Identical pixels, including static HUD, stay exact. Uncertain pixels use
 // current-frame content. Moving UI is subject to optical-flow artifacts.
 result=mix(now,(a+b)*.5,trust);
}
)GLSL";
struct State {
 GLint program,vao,draw,read,drawBuffer,readBuffer,active,unpack,polygon[2],viewport[4],texture[3],sampler[3];
 GLboolean blend,depth,scissor,cull,srgb,stencil,discard,mask[4];
 State(){
  glGetIntegerv(GL_CURRENT_PROGRAM,&program); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
  glGetIntegerv(GL_DRAW_BUFFER,&drawBuffer); glGetIntegerv(GL_READ_BUFFER,&readBuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
  glGetIntegerv(GL_ACTIVE_TEXTURE,&active); glGetIntegerv(GL_VIEWPORT,viewport);
  glGetIntegerv(GL_POLYGON_MODE,polygon);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  stencil=glIsEnabled(GL_STENCIL_TEST);discard=glIsEnabled(GL_RASTERIZER_DISCARD);
  glDisable(GL_STENCIL_TEST);glDisable(GL_RASTERIZER_DISCARD);
  blend=glIsEnabled(GL_BLEND);depth=glIsEnabled(GL_DEPTH_TEST);scissor=glIsEnabled(GL_SCISSOR_TEST);
  cull=glIsEnabled(GL_CULL_FACE);srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);glGetBooleanv(GL_COLOR_WRITEMASK,mask);
  for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture[i]);glGetIntegeri_v(GL_SAMPLER_BINDING,i,&sampler[i]);glBindSampler(i,0);}
  glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 }
 ~State(){
  glUseProgram(program);glBindVertexArray(vao);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glDrawBuffer(drawBuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glReadBuffer(readBuffer);
  glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
  for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,texture[i]);glBindSampler(i,sampler[i]);}
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);glActiveTexture(active);glColorMask(mask[0],mask[1],mask[2],mask[3]);
  glPolygonMode(GL_FRONT_AND_BACK,polygon[0]);restore(GL_STENCIL_TEST,stencil);restore(GL_RASTERIZER_DISCARD,discard);
  restore(GL_BLEND,blend);restore(GL_DEPTH_TEST,depth);restore(GL_SCISSOR_TEST,scissor);restore(GL_CULL_FACE,cull);restore(GL_FRAMEBUFFER_SRGB,srgb);
 }
 static void restore(GLenum e,GLboolean enabled){if(enabled)glEnable(e);else glDisable(e);}
};
class Generator {
 GLuint frames[2]={},flow=0,fbo=0,vao=0,flowProgram=0,outputProgram=0;
 int width=0,height=0,fw=0,fh=0,current=0;
 bool history=false,failed=false;
 Clock::time_point last={};
 double overhead=0.;
 static GLuint shader(GLenum type,const char* code){
  GLuint s=glCreateShader(type);glShaderSource(s,1,&code,nullptr);glCompileShader(s);
  GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){glDeleteShader(s);return 0;}return s;
 }
 static GLuint program(const char* fragment){
  GLuint v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);
  if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
  GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
  glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(!ok){glDeleteProgram(p);return 0;}return p;
 }
 static void texture(GLuint id,int w,int h,GLint format){
  glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
 }
 void bindInputs(GLuint p){
  glUseProgram(p);
  const GLuint ids[]={frames[1-current],frames[current],flow};
  const char* names[]={"previousFrame","currentFrame","flow"};
  for(int i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,ids[i]);glUniform1i(glGetUniformLocation(p,names[i]),i);}
 }
public:
 void release(){
  if(frames[0])glDeleteTextures(2,frames);if(flow)glDeleteTextures(1,&flow);
  if(fbo)glDeleteFramebuffers(1,&fbo);if(vao)glDeleteVertexArrays(1,&vao);
  if(flowProgram)glDeleteProgram(flowProgram);if(outputProgram)glDeleteProgram(outputProgram);
  frames[0]=frames[1]=flow=fbo=vao=flowProgram=outputProgram=0;
  width=height=0;history=false;last={};overhead=0.;
 }
 // The caller must flush viewer rendering first. Off means no capture, shaders,
 // memory allocations or additional presentations. Return true if both presented.
 template<class Present> bool present(bool enabled,int w,int h,Present nativePresent){
  if(!enabled){if(width){State saved;release();}failed=false;return false;}
  if(failed||w<32||h<32||w>4096||h>4096){history=false;return false;}
  State saved;
  if(width!=w||height!=h){
   release();width=w;height=h;fw=std::min(480,std::max(32,w/4));fh=std::min(270,std::max(32,h/4));
   flowProgram=program(flowFragment);outputProgram=program(interpolateFragment);
   if(!flowProgram||!outputProgram){release();failed=true;LL_WARNS("FrameGen")<<"KAMI interpolation shader failed; native presentation retained"<<LL_ENDL;return false;}
   glGenTextures(2,frames);glGenTextures(1,&flow);glGenFramebuffers(1,&fbo);glGenVertexArrays(1,&vao);
   glActiveTexture(GL_TEXTURE0);texture(frames[0],w,h,GL_RGBA8);texture(frames[1],w,h,GL_RGBA8);texture(flow,fw,fh,GL_RGBA16F);
   GLint allocated=0;glBindTexture(GL_TEXTURE_2D,frames[1]);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&allocated);
   if(allocated!=w){release();failed=true;return false;}
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,flow,0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
   if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){release();failed=true;return false;}
  }
  auto now=Clock::now();double dt=std::chrono::duration<double>(now-last).count()-overhead;last=now;overhead=0.;
  glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,frames[current]);glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,w,h);
  // Require a usable real-render cadence; skip stalls, minimization and low FPS.
  if(!history||dt<1./240.||dt>1./30.){history=true;current=1-current;return false;}
  glBindVertexArray(vao);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);glDrawBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,fw,fh);
  bindInputs(flowProgram);glUniform2f(glGetUniformLocation(flowProgram,"pixel"),1.f/w,1.f/h);glDrawArrays(GL_TRIANGLES,0,3);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);glViewport(0,0,w,h);
  bindInputs(outputProgram);glUniform1i(glGetUniformLocation(outputProgram,"original"),0);glDrawArrays(GL_TRIANGLES,0,3);
  nativePresent();
  // Space the generated and real frames. This prototype introduces latency;
  // real simulation/input frequency is unchanged. Never count these as engine FPS.
  const auto generatedAt=Clock::now();
  glUniform1i(glGetUniformLocation(outputProgram,"original"),1);glDrawArrays(GL_TRIANGLES,0,3);glFlush();
  std::this_thread::sleep_until(generatedAt+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(dt*.5)));
  nativePresent();overhead=std::chrono::duration<double>(Clock::now()-now).count();current=1-current;return true;
 }
};
inline Generator& generator(){static Generator instance;return instance;}
}
