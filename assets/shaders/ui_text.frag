#version 450

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(push_constant) uniform TextPush {
    vec2 viewport;
    float blur;
    float opacity;
} push;

float sampleCoverage(vec2 uv) {
    return texture(fontAtlas, uv).r;
}

void main() {
    float coverage;
    if (push.blur > 0.01) {
        const vec2 texel = 1.0 / vec2(textureSize(fontAtlas, 0));
        const vec2 offset = texel * push.blur;

        coverage = sampleCoverage(vUv) * 0.2;
        coverage += sampleCoverage(vUv + vec2(offset.x, 0.0)) * 0.1;
        coverage += sampleCoverage(vUv - vec2(offset.x, 0.0)) * 0.1;
        coverage += sampleCoverage(vUv + vec2(0.0, offset.y)) * 0.1;
        coverage += sampleCoverage(vUv - vec2(0.0, offset.y)) * 0.1;
        coverage += sampleCoverage(vUv + offset) * 0.05;
        coverage += sampleCoverage(vUv - offset) * 0.05;
        coverage += sampleCoverage(vUv + vec2(offset.x, -offset.y)) * 0.05;
        coverage += sampleCoverage(vUv + vec2(-offset.x, offset.y)) * 0.05;
        coverage += sampleCoverage(vUv + vec2(offset.x * 2.0, 0.0)) * 0.05;
        coverage += sampleCoverage(vUv - vec2(offset.x * 2.0, 0.0)) * 0.05;
        coverage += sampleCoverage(vUv + vec2(0.0, offset.y * 2.0)) * 0.05;
        coverage += sampleCoverage(vUv - vec2(0.0, offset.y * 2.0)) * 0.05;
    } else {
        coverage = sampleCoverage(vUv);
    }

    const vec4 color = vec4(vColor.rgb, vColor.a * coverage * push.opacity);

    if (color.a <= 0.0) {
        discard;
    }

    outColor = color;
}
