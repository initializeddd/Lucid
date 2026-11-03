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
        // A normalized Gaussian truncated at three standard deviations.
        // More closely spaced samples avoid the hard, box-like edges of a
        // sparse kernel, especially on standard-density displays.
        const float weights[13] = float[13](
            0.09990836, 0.09683450, 0.08816882, 0.07541479,
            0.06059748, 0.04574138, 0.03243549, 0.02160670,
            0.01352113, 0.00794866, 0.00438967, 0.00227733,
            0.00110988);
        const vec2 texel = 1.0 / push.viewport;
        const vec2 direction = push.horizontal > 0.5
            ? vec2(texel.x, 0.0)
            : vec2(0.0, texel.y);
        const vec2 step = direction * (push.radius / 12.0);

        color = texture(sceneTexture, vUv) * weights[0];
        for (int i = 1; i < 13; ++i) {
            const vec2 offset = step * float(i);
            color += texture(sceneTexture, vUv + offset) * weights[i];
            color += texture(sceneTexture, vUv - offset) * weights[i];
        }
    }

    color.rgb *= (1.0 - push.dim);
    outColor = color;
}
