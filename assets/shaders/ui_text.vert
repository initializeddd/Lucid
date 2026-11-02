#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;

layout(push_constant) uniform TextPush {
    vec2 viewport;
    float blur;
    float opacity;
} push;

void main() {
    vUv = aUv;
    vColor = aColor;

    const vec2 ndc = vec2(
        aPosition.x / push.viewport.x * 2.0 - 1.0,
        aPosition.y / push.viewport.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
