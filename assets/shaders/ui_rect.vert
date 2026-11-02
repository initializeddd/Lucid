#version 450

layout(location = 0) out vec2 vPixel;

layout(push_constant) uniform RectPush {
    vec2 viewport;
    vec2 rectMin;
    vec2 rectMax;
    float radius;
    float borderWidth;
    vec4 fillColor;
    vec4 borderColor;
    float blur;
    float opacity;
} push;

void main() {
    const vec2 corners[6] = vec2[6](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));

    const vec2 uv = corners[gl_VertexIndex];
    const vec2 pixel = mix(push.rectMin, push.rectMax, uv);
    vPixel = pixel;

    const vec2 ndc = vec2(
        pixel.x / push.viewport.x * 2.0 - 1.0,
        pixel.y / push.viewport.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
