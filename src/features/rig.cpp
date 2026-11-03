#include "kami/features/rig.hpp"
#include <algorithm>
#include <cmath>

namespace kami::rig {
namespace {
constexpr std::array<JointInfo,kJointCount> kBones{{
    {"Root",-1,{}},
    {"mPelvis",0,{0.f,.94f,0.f}},
    {"mChest",1,{0.f,1.03f,0.f}},
    {"LEFT_PEC",2,{-.225f,.16f,.268f}},
    {"RIGHT_PEC",2,{.225f,.16f,.268f}},
    {"BELLY",1,{0.f,.56f,.263f}},
    {"BUTT",1,{0.f,.36f,-.265f}},
    {"mHead",2,{0.f,.80f,0.f}}
}};
Matrix mul(Matrix a,Matrix b) noexcept {
    Matrix out{};
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)
        for(int k=0;k<4;++k)
            out.elements[4*i+j]+=a.elements[4*i+k]*b.elements[4*k+j];
    return out;
}
Matrix translation(Vec3 p) noexcept {
    Matrix result=identity();
    result.elements[12]=p.x;
    result.elements[13]=p.y;
    result.elements[14]=p.z;
    return result;
}
Matrix rotation(float yaw,float roll) noexcept {
    Matrix result=identity();
    const float y=std::cos(yaw),ys=std::sin(yaw);
    const float r=std::cos(roll),rs=std::sin(roll);
    Matrix around_y=identity(),around_z=identity();
    around_y.elements[0]=y;around_y.elements[2]=-ys;
    around_y.elements[8]=ys;around_y.elements[10]=y;
    around_z.elements[0]=r;around_z.elements[1]=rs;
    around_z.elements[4]=-rs;around_z.elements[5]=r;
    return mul(around_z,around_y);
}
bool finite(Vec3 p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
Vec3 normalized(Vec3 v) noexcept {
    const float n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
    return n>1e-8f&&std::isfinite(n)?
        Vec3{v.x/n,v.y/n,v.z/n}:Vec3{0.f,1.f,0.f};
}
} // namespace
const std::array<JointInfo,kJointCount>& joints() noexcept {return kBones;}
Matrix identity() noexcept {
    Matrix r{};
    for(int i=0;i<4;++i)r.elements[5*i]=1.f;
    return r;
}
Vec3 transform_position(Matrix m,Vec3 p) noexcept {
    return {p.x*m.elements[0]+p.y*m.elements[4]+p.z*m.elements[8]+m.elements[12],
            p.x*m.elements[1]+p.y*m.elements[5]+p.z*m.elements[9]+m.elements[13],
            p.x*m.elements[2]+p.y*m.elements[6]+p.z*m.elements[10]+m.elements[14]};
}
Vec3 transform_normal(Matrix m,Vec3 p) noexcept {
    return normalized({p.x*m.elements[0]+p.y*m.elements[4]+p.z*m.elements[8],
                       p.x*m.elements[1]+p.y*m.elements[5]+p.z*m.elements[9],
                       p.x*m.elements[2]+p.y*m.elements[6]+p.z*m.elements[10]});
}
bool make_palette(const Pose& pose,Palette& output) noexcept {
    std::array<Matrix,kJointCount> bind_world{},posed_world{};
    for(std::size_t i=0;i<kJointCount;++i) {
        const auto& b=kBones[i];
        const auto& p=pose[i];
        if(!finite(p.translation)||!std::isfinite(p.yaw)||!std::isfinite(p.roll)||
           std::abs(p.translation.x)>.4f||std::abs(p.translation.y)>.4f||
           std::abs(p.translation.z)>.4f||
           std::abs(p.yaw)>.8f||std::abs(p.roll)>.8f)return false;
        const Matrix bind_local=translation(b.rest_local);
        const Matrix posed_local=mul(rotation(p.yaw,p.roll),
           translation({b.rest_local.x+p.translation.x,
                        b.rest_local.y+p.translation.y,
                        b.rest_local.z+p.translation.z}));
        bind_world[i]=b.parent<0?bind_local:
            mul(bind_local,bind_world[static_cast<std::size_t>(b.parent)]);
        posed_world[i]=b.parent<0?posed_local:
            mul(posed_local,posed_world[static_cast<std::size_t>(b.parent)]);
        const auto& rest=bind_world[i].elements;
        // Rest bind contains translations only. Inverse bind is simply the
        // negative world-space bind translation, avoiding a fragile inverse.
        output[i]=mul(translation({-rest[12],-rest[13],-rest[14]}),
                      posed_world[i]);
    }
    return true;
}
bool validate_influences(Influences in) noexcept {
    float sum=0.f;
    for(std::size_t i=0;i<kInfluences;++i) {
        const float w=in.weights[i];
        if(!std::isfinite(w)||w<0.f||w>1.f)return false;
        if(w>0.f&&in.joints[i]>=kJointCount)return false;
        sum+=w;
    }
    return sum>1e-6f&&std::isfinite(sum);
}
bool pack_influences(Influences in,Packed& output) noexcept {
    if(!validate_influences(in))return false;
    const float sum=in.weights[0]+in.weights[1]+in.weights[2]+in.weights[3];
    std::array<int,4> fixed{};
    int total=0;
    for(int i=0;i<4;++i) {
        fixed[i]=static_cast<int>(std::floor(in.weights[i]/sum*255.f));
        total+=fixed[i];
    }
    // Distribute remainders to the largest rounding residuals.
    while(total<255) {
        int best=0;float residual=-1.f;
        for(int i=0;i<4;++i) {
            const float r=in.weights[i]/sum*255.f-static_cast<float>(fixed[i]);
            if(r>residual){residual=r;best=i;}
        }
        ++fixed[best];++total;
    }
    Packed packed{};
    for(int i=0;i<4;++i) {
        packed.joints|=static_cast<std::uint32_t>(in.joints[i])<<(8*i);
        packed.weights|=static_cast<std::uint32_t>(fixed[i])<<(8*i);
    }
    output=packed;return true;
}
bool unpack_influences(Packed packed,Influences& output) noexcept {
    Influences in{};
    for(int i=0;i<4;++i) {
        in.joints[i]=static_cast<std::uint8_t>(packed.joints>>(8*i));
        in.weights[i]=static_cast<float>((packed.weights>>(8*i))&255U)/255.f;
    }
    if(!validate_influences(in))return false;
    output=in;return true;
}
bool deform(Vec3 original,Vec3 normal,Packed packed,const Palette& palette,
            Vec3& position_out,Vec3& normal_out) noexcept {
    if(!finite(original)||!finite(normal))return false;
    if(packed.weights==0U) {position_out=original;normal_out=normalized(normal);return true;}
    Influences influences;
    if(!unpack_influences(packed,influences))return false;
    Vec3 pos{},n{};
    for(int i=0;i<4;++i){
        const float w=influences.weights[i];
        if(w<=0.f)continue;
        const auto bone=palette[influences.joints[i]];
        const auto p=transform_position(bone,original);
        const auto v=transform_normal(bone,normal);
        pos.x+=p.x*w;pos.y+=p.y*w;pos.z+=p.z*w;
        n.x+=v.x*w;n.y+=v.y*w;n.z+=v.z*w;
    }
    if(!finite(pos)||!finite(n))return false;
    position_out=pos;normal_out=normalized(n);return true;
}
} // namespace kami::rig
