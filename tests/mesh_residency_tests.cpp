#include "kami/world/mesh_residency.hpp"
#include <cassert>
#include <limits>
int main(){
    kami::world::MeshResidencyPlanner planner;
    planner.begin_generation();
    planner.see_asset("building",100,2400);
    planner.see_asset("chair",10,240);
    planner.see_instance(1,0,"building");
    planner.see_instance(2,0,"chair");
    planner.see_instance(3,0,"chair");
    planner.end_generation();
    const auto candidates=std::vector<kami::world::MeshDrawCandidate>{
        {1,"building",1.f},{2,"chair",2.f},{3,"chair",3.f},
        {2,"chair",2.f},{4,"missing",4.f}};
    const auto schedule=planner.schedule_draws(candidates,25);
    assert(schedule.admitted.size()==2);
    assert(schedule.admitted[0].local_id==2);
    assert(schedule.admitted[1].local_id==3);
    assert(schedule.triangles==20);
    assert(schedule.deferred==3);
    planner.begin_generation();
    planner.see_instance(2,0,"chair");
    planner.end_generation();
    const auto next=planner.schedule_draws(candidates,200);
    assert(next.admitted.size()==1);
    assert(next.admitted[0].local_id==2);
    const auto invalid_distances=planner.schedule_draws({
        {2,"chair",std::numeric_limits<float>::quiet_NaN()},
        {2,"chair",std::numeric_limits<float>::infinity()},
        {2,"chair",-1.f},{2,"chair",1.f}},100);
    assert(invalid_distances.admitted.size()==1);
    assert(invalid_distances.admitted[0].local_id==2);
    assert(invalid_distances.deferred==3);
    return 0;
}
