#version 450

layout(location = 0) out vec2 vUv;

layout(push_constant) uniform BlurPush {
    vec2 viewport;
    float radius;
    float dim;
} push;

void main() {
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    const vec2 position = positions[gl_VertexIndex];
    vUv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
