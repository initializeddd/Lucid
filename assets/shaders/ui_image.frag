#version 450
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D imageTexture;
layout(push_constant) uniform TextPush { vec2 viewport; float srgbTarget; float opacity; } push;
void main() {
    vec4 color = texture(imageTexture, vUv) * vColor;
    if (push.srgbTarget > 0.5) {
        bvec3 low = lessThanEqual(color.rgb, vec3(0.04045));
        color.rgb = mix(pow((color.rgb + 0.055) / 1.055, vec3(2.4)), color.rgb / 12.92, low);
    }
    outColor = vec4(color.rgb, color.a * push.opacity);
}
