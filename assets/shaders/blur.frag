#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;

layout(push_constant) uniform BlurPush {
    vec2 viewport;
    float radius;
    float dim;
    float horizontal;
} push;

void main() {
    vec4 color;
    if (push.radius < 0.01) {
        color = texture(sceneTexture, vUv);
    } else {
        const float weights[9] = float[9](
            0.0875, 0.0858, 0.0808, 0.0731, 0.0636,
            0.0531, 0.0426, 0.0329, 0.0243);
        const vec2 texel = 1.0 / push.viewport;
        const vec2 direction = push.horizontal > 0.5
            ? vec2(texel.x, 0.0)
            : vec2(0.0, texel.y);
        const vec2 step = direction * (push.radius / 8.0);

        color = texture(sceneTexture, vUv) * weights[0];
        for (int i = 1; i < 9; ++i) {
            const vec2 offset = step * float(i);
            color += texture(sceneTexture, vUv + offset) * weights[i];
            color += texture(sceneTexture, vUv - offset) * weights[i];
        }
    }

    color.rgb *= (1.0 - push.dim);
    outColor = color;
}
