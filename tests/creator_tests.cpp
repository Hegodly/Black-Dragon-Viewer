#include "kami/features/creator.hpp"
#include <limits>
int main(){
 using namespace kami;
 auto mesh=world::build_test_scene(16);
 auto result=creator::inspect(mesh);
 if(!result.valid||result.triangles!=mesh.vertices.size()/3)return 1;
 mesh.vertices[1].px=std::numeric_limits<float>::quiet_NaN();
 if(creator::inspect(mesh).valid)return 2;
 mesh=world::build_test_scene(8);
 mesh.vertices[1].px=mesh.vertices[0].px;
 mesh.vertices[1].py=mesh.vertices[0].py;
 mesh.vertices[1].pz=mesh.vertices[0].pz;
 if(creator::inspect(mesh).valid)return 3;
 return 0;
}
