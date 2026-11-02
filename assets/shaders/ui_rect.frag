#version 450

layout(location = 0) in vec2 vPixel;
layout(location = 0) out vec4 outColor;

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

float roundedBoxDistance(vec2 point, vec2 halfSize, float cornerRadius) {
    const vec2 q = abs(point) - halfSize + vec2(cornerRadius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - cornerRadius;
}

void main() {
    const vec2 center = (push.rectMin + push.rectMax) * 0.5;
    const vec2 halfSize = (push.rectMax - push.rectMin) * 0.5;
    const float distance = roundedBoxDistance(vPixel - center, halfSize, push.radius);

    const float antialias = max(fwidth(distance), 0.0001);
    const float softness = max(antialias, push.blur);
    const float coverage = 1.0 - smoothstep(-softness, softness, distance);

    float borderMask = 0.0;
    if (push.borderWidth > 0.0) {
        const float borderSoftness = max(antialias, push.blur * 0.75);
        borderMask = smoothstep(-push.borderWidth - borderSoftness,
                                -push.borderWidth + borderSoftness,
                                distance);
    }

    vec4 color = mix(push.fillColor, push.borderColor, borderMask);
    color.a *= coverage * push.opacity;

    if (color.a <= 0.0) {
        discard;
    }

    outColor = color;
}
