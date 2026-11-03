#include "renderer/scene/Camera.hpp"
#include <array>
#include <cmath>
int main() {
    using namespace lucid::renderer::scene;
    Camera c; std::array<float,16> m{};
    if (!viewProjection(c, 1.0F, m)) return 1;
    const float z=m[14], w=m[15];
    if (w<=0 || z/w<0 || z/w>1 || std::abs(m[12])>0.0001F || std::abs(m[13])>0.0001F) return 2;
    const auto previous=m;
    c.target=c.position;
    if (viewProjection(c,1.0F,m) || m!=previous) return 3;
    c=Camera{}; c.up={0,0,1};
    if (viewProjection(c,1.0F,m) || viewProjection(Camera{},0.0F,m)) return 4;
    return 0;
}
