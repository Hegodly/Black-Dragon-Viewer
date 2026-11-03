#include "kami/graphics/adaptive.hpp"
int main(){
 using namespace kami::graphics;
 Settings s{};
 AdaptiveQuality autoq{};
 s.auto_quality=true;
 s.target_fps=60;
 s.fps_limit=0;
 for(int i=0;i<2;i++) if(autoq.sample(s,35.f,1.f))return 1;
 if(!autoq.sample(s,35.f,1.f)||s.render_scale_percent!=75)return 2;
 for(int i=0;i<5;i++)if(autoq.sample(s,35.f,1.f))return 3;
 for(int i=0;i<3;i++) (void)autoq.sample(s,35.f,1.f);
 for(int i=0;i<6;i++) (void)autoq.sample(s,130.f,1.f);
 if(s.render_scale_percent!=100)return 4;
 autoq.reset();
 s.render_scale_percent=100;s.fps_limit=30;s.target_fps=120;
 for(int i=0;i<12;i++)if(autoq.sample(s,20.f,1.f))return 5;
 if(s.render_scale_percent!=100)return 6;
 s.target_fps=60;s.fps_limit=0;
 autoq.reset();
 if(autoq.sample(s,-1.f,1.f))return 7;
 return 0;
}
