#include "renderer/scene/Camera.hpp"
#include <cmath>
namespace lucid::renderer::scene {
namespace {
Vector3 subtract(Vector3 a, Vector3 b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
float dot(Vector3 a, Vector3 b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
Vector3 cross(Vector3 a, Vector3 b) { return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]}; }
bool normalize(Vector3& v) { const float length=std::sqrt(dot(v,v)); if (!std::isfinite(length) || length < 1e-6F) return false; for (auto& n:v) n/=length; return true; }
}
bool viewProjection(const Camera& c, float aspect, std::array<float,16>& result) {
    if (!std::isfinite(aspect) || aspect<=0 || !std::isfinite(c.verticalFovRadians) || c.verticalFovRadians<=0 || c.verticalFovRadians>=3.14F || !std::isfinite(c.nearPlane) || !std::isfinite(c.farPlane) || c.nearPlane<=0 || c.farPlane<=c.nearPlane) return false;
    Vector3 f=subtract(c.target,c.position); if (!normalize(f)) return false;
    Vector3 r=cross(f,c.up); if (!normalize(r)) return false;
    const Vector3 u=cross(r,f);
    const std::array<float,16> view={r[0],u[0],-f[0],0, r[1],u[1],-f[1],0, r[2],u[2],-f[2],0, -dot(r,c.position),-dot(u,c.position),dot(f,c.position),1};
    const float focal=1.0F/std::tan(c.verticalFovRadians*0.5F);
    const float z=c.farPlane/(c.nearPlane-c.farPlane);
    const std::array<float,16> projection={focal/aspect,0,0,0, 0,-focal,0,0, 0,0,z,-1, 0,0,z*c.nearPlane,0};
    std::array<float,16> combined{};
    for (int col=0;col<4;++col) for (int row=0;row<4;++row) for (int k=0;k<4;++k) combined[static_cast<std::size_t>(col*4+row)]+=projection[static_cast<std::size_t>(k*4+row)]*view[static_cast<std::size_t>(col*4+k)];
    for(float n:combined) if(!std::isfinite(n)) return false;
    result=combined; return true;
}
}
