#include "kami/features/creator_studio.hpp"
#include <sstream>
#include <cmath>
int main() {
    using namespace kami::creator;
    const char* valid=
        "# original KAMI test OBJ\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "f 1 2 3 4\n";
    std::istringstream file(valid);
    Preview scene;std::string error;
    if(!import_obj(file,scene,error)||scene.audit.triangles!=2 ||
       scene.source_positions!=4||scene.source_polygons!=1)return 1;
    if(!std::isfinite(scene.scene.vertices[0].px))return 2;
    std::istringstream negative(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n");
    if(!import_obj(negative,scene,error)||scene.audit.triangles!=1)return 3;
    std::istringstream missing("v 0 0 0\nf 1 2 3\n");
    if(import_obj(missing,scene,error))return 4;
    std::istringstream zero("v 0 0 0\nv 0 0 0\nv 0 0 0\nf 1 2 3\n");
    if(import_obj(zero,scene,error))return 5;
    std::istringstream nofaces("v 0 0 0\n");
    if(import_obj(nofaces,scene,error))return 6;
    return 0;
}
