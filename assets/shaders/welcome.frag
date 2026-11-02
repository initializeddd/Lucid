#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform WelcomePush {
    vec2 viewport;
    float time;
    float opacity;
    vec4 primary;
    vec4 secondary;
    float softness;
    float dim;
} push;

float fastWave(float phase) {
    float x = phase * 0.15915494 + 0.25;
    x = x - floor(x);
    const float triangle = 1.0 - 4.0 * abs(x - 0.5);
    return triangle * (1.5 - 0.5 * triangle * triangle);
}

float band(float distance, float width) {
    const float x = max(0.0, 1.0 - abs(distance) * width);
    return x * x * (3.0 - 2.0 * x);
}

void main() {
    const float aspect = max(push.viewport.x, 1.0) /
                         max(push.viewport.y, 1.0);
    const vec2 p = (vUv - vec2(0.5)) * vec2(aspect, 1.0);
    const float t = push.time;

    vec2 flow = p;
    flow.x += 0.20 * fastWave(p.y * 2.4 + t * 0.11);
    flow.y += 0.10 * fastWave(p.x * 1.6 - t * 0.08);
    const float y =
        flow.y + 0.22 * flow.x + 0.045 * fastWave(flow.x * 2.8 + t * 0.16);

    const float wave = t * 0.14;
    const float center1 = 0.04 + 0.14 * fastWave(flow.x * 1.7 + wave);
    const float center2 = -0.36 + 0.13 * fastWave(flow.x * 1.3 - wave + 1.8);
    const float center3 = 0.38 + 0.11 * fastWave(flow.x * 2.1 + wave + 3.4);
    const float width = 1.0 - 0.55 * push.softness;
    const float ribbon1 = band(y - center1, 8.0 * width);
    const float ribbon2 = band(y - center2, 9.0 * width);
    const float ribbon3 = band(y - center3, 10.0 * width);
    const float ribbon = 0.55 * ribbon1 + 0.32 * ribbon2 + 0.24 * ribbon3;
    const float glowRadius = mix(1.12, 1.55, push.softness);
    const float glow =
        1.0 - smoothstep(0.38, glowRadius, length(p * vec2(0.90, 1.28)));
    const float alpha = ribbon * glow * push.opacity;
    const vec3 color = mix(push.primary.rgb, push.secondary.rgb,
                           0.05 + 0.10 * ribbon3) * push.dim;

    outColor = vec4(color, alpha);
}
