#include "kami/features/creator_studio.hpp"
#include "kami/features/creator_limits.hpp"
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
    // Blender-style OBJ export with UV/normal/material entries and a
    // duplicate-index face must retain valid geometry, dropping only the
    // invalid zero-area triangle.
    std::istringstream imperfect(
        "mtllib sample.mtl\n"
        "o Example\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\nusemtl Gold\n"
        "f 1/1/1 2/2/1 3/3/1\n"
        "f 1/1/1 1/1/1 4/4/1\n");
    if(!import_obj(imperfect,scene,error)||
       scene.audit.triangles!=1||
       scene.skipped_degenerate_triangles!=1||
       scene.source_polygons!=2)return 7;
    if(kami::creator::kMaxPreviewVertices!=
       kami::creator::kMaxPreviewTriangles*3U||
       kami::creator::kMaxPreviewVertices>=1000000U)return 8;
    if(import_obj(zero,scene,error)||
       error.find("degenerate")==std::string::npos)return 9;
    return 0;
}
