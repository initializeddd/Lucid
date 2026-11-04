#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vPosition;
layout(push_constant) uniform ScenePush { mat4 mvp; vec4 color; vec4 animation; vec4 background; vec4 material; } scene;
void main() {
    vec3 animated = position;
    if (scene.animation.x > 14.5 && scene.animation.x < 15.5) {
        // Wind bends the tent as a whole while its base stays pegged down.
        // The subdivided canvas billows between its seams on alternating sides.
        float height = clamp((position.y - 0.04) / 1.61, 0.0, 1.0);
        float wind = sin(scene.animation.y * 0.4) + 0.3 * sin(scene.animation.y * 0.8 + 0.7);
        animated.x += height * 0.13 * wind;
        animated.z += height * 0.035 * sin(scene.animation.y * 0.3 + 0.5);
        if (scene.material.y > 22.5 && scene.material.y < 23.5) {
            float across = sin(3.14159265 * height);
            float along = sin(3.14159265 * clamp((position.z + 1.5) / 1.6, 0.0, 1.0));
            float side = sign(position.x - 1.0);
            float billow = 0.13 * across * along * sin(scene.animation.y * 0.8 + position.z * 2.4 + side);
            animated.x += side * billow;
            animated.y += billow * 0.12;
        }
    } else if (scene.animation.x > 12.5 && scene.animation.x < 13.5) {
        float phase = fract(sin(dot(floor(position.xz * 5.0), vec2(127.1,311.7))) * 43758.5453);
        float rise = fract(scene.animation.y * 0.45 + phase);
        animated.y += rise * 1.5;
        animated.x += rise * 0.18 * sin(scene.animation.y * 1.2 + phase * 6.283);
    } else if (scene.animation.x > 10.5 && scene.animation.x < 11.5) {
        animated.y += 0.08 * sin(scene.animation.y * 2.2 + position.x * 1.7);
    } else if (scene.animation.x > 8.5 && scene.animation.x < 9.5) {
        float h = max(position.y - 0.18, 0.0);
        animated.x += h * 0.12 * sin(scene.animation.y * 3.0 + position.z * 4.0);
        animated.y += h * 0.16 * sin(scene.animation.y * 4.3 + position.x * 3.0);
    } else if (scene.animation.x > 5.5 && scene.animation.x < 6.5) {
        // Anchor stems at ground level and let upper branches bend gently.
        float height = clamp(position.y, 0.0, 8.0);
        float phase = position.x * 0.45 + position.z * 0.30;
        animated.x += height * height * 0.008 * (sin(scene.animation.y * 0.38 + phase) + 0.3 * sin(scene.animation.y * 0.81 + phase * 1.7));
        animated.z += height * height * 0.003 * sin(scene.animation.y * 0.27 + phase);
    } else if (scene.animation.x > 2.5 && scene.animation.x < 3.5) {
        float phase = position.x * 0.8 + position.z * 0.6;
        animated.x += 0.11 * sin(scene.animation.y * 0.7 + phase);
        animated.y += 0.17 * sin(scene.animation.y * 0.9 + phase * 1.4);
    } else if (scene.animation.x > 1.5 && scene.animation.x < 2.5) {
        float anchor = floor(position.y + 0.5);
        // Thirty-nine forty-unit cycles per animation clock period. The
        // upper wrap is well above the view so drops do not pop into sight.
        animated.y = mod(anchor - scene.animation.y * 2.4828171, 40.0) + position.y - anchor;
    } else if (scene.animation.x > 0.5 && scene.animation.x < 1.5) {
        // Broad swells and smaller cross-waves move water and foam together.
        float swell = scene.material.y > 11.5 && scene.material.y < 12.5 ? 0.18 : 0.07;
        animated.y += swell * sin(position.z * 0.78 + position.x * 0.18 - scene.animation.y * 0.8)
                    + 0.05 * sin(position.x * 1.1 + position.z * 0.6 + scene.animation.y * 1.2);
    }
    gl_Position = scene.mvp * vec4(animated, 1.0);
    if (scene.animation.x > 7.5 && scene.animation.x < 8.5) gl_Position.z = gl_Position.w * 0.99999;
    float c = cos(scene.animation.w), s = sin(scene.animation.w);
    vNormal = vec3(normal.x * c + normal.z * s, normal.y, -normal.x * s + normal.z * c);
    if (scene.animation.x > 13.5 && scene.animation.x < 14.5) {
        const float ct = cos(0.38), st = sin(0.38);
        vNormal.xy = mat2(ct, st, -st, ct) * vNormal.xy;
    }
    vPosition = position;
}
