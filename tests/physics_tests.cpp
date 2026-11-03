#include "kami/features/physics.hpp"
#include <cmath>
int main() {
 kami::physics::Spring spring;
 for(int i=0;i<240;i++)spring.step(1.f,1.f/120.f);
 if(!std::isfinite(spring.position())||std::abs(spring.position()-1.f)>.015f)return 1;
 spring.reset();
 if(spring.position()!=0.f||spring.velocity()!=0.f)return 2;
 spring.step(10.f,100.f);
 if(!std::isfinite(spring.position()))return 3;
 return 0;
}
