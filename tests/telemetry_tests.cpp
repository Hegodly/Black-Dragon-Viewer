#include "kami/features/telemetry.hpp"
#include <cmath>
int main(){
 kami::telemetry::Monitor monitor;
 const auto initial=monitor.sample(nullptr);
 if(initial.gpu_readable)return 1;
 if(!std::isfinite(initial.process_cpu_percent))return 2;
 const auto next=monitor.sample(nullptr);
 if(next.process_cpu_percent<0.f||next.process_cpu_percent>100.f)return 3;
 return 0;
}
