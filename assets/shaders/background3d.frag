#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vPosition;
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform ScenePush { mat4 mvp; vec4 color; vec4 animation; vec4 background; vec4 material; } scene;
float cellNoise(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float smoothNoise(vec2 p) {
    vec2 cell = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(cellNoise(cell), cellNoise(cell + vec2(1.0, 0.0)), f.x),
               mix(cellNoise(cell + vec2(0.0, 1.0)), cellNoise(cell + vec2(1.0, 1.0)), f.x), f.y);
}
void main() {
    // Ordered screen-space dithering keeps the fade in the same pixel-art
    // vocabulary as the low-resolution scene. Discarded samples reveal the
    // actual background and do not write depth over geometry behind them.
    float groundVisibility = 1.0;
    // Break up the horizon with broad, gentle waves in world space. Keep
    // the variation inside the extended geometry so the mesh edge stays hidden.
    float horizonWave = 0.55 * sin(vPosition.x * 0.24) + 0.22 * sin(vPosition.x * 0.53 + 1.2);
    float groundDepth = vPosition.z + horizonWave;
    if ((scene.material.y > 2.5 && scene.material.y < 3.5) || (scene.material.y > 14.5 && scene.material.y < 15.5))
        groundVisibility = smoothstep(-44.0, -29.0, groundDepth);
    else if (scene.material.y > 8.5 && scene.material.y < 9.5)
        groundVisibility = smoothstep(-29.0, -20.0, groundDepth);
    else if (scene.material.y > 16.5 && scene.material.y < 17.5)
        groundVisibility = smoothstep(-30.5, -19.0, groundDepth);
    else if (scene.material.y > 19.5 && scene.material.y < 20.5)
        groundVisibility = smoothstep(0.8, 4.8, groundDepth);
    else if (scene.material.y > 10.5 && scene.material.y < 11.5)
        groundVisibility = smoothstep(-13.0, -2.0, groundDepth);
    else if (scene.material.y > 4.5 && scene.material.y < 5.5)
        groundVisibility = smoothstep(-14.0, -8.0, groundDepth);
    else if (scene.material.y > 11.5 && scene.material.y < 12.5)
        groundVisibility = smoothstep(-5.2, -2.0, groundDepth);
    else if (scene.material.y > 21.5 && scene.material.y < 22.5)
        groundVisibility = smoothstep(-7.4, -4.0, groundDepth);
    else if (scene.material.y > 12.5 && scene.material.y < 13.5)
        groundVisibility = smoothstep(-5.5, -1.5, vPosition.z);
    else if (scene.material.y > 13.5 && scene.material.y < 14.5)
        groundVisibility = smoothstep(-10.0, -3.0, vPosition.z);
    if (groundVisibility < 1.0) {
        const float pattern[16] = float[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
        ivec2 pixel = ivec2(gl_FragCoord.xy) & 3;
        float threshold = (pattern[pixel.y * 4 + pixel.x] + 0.5) / 16.0;
        if (groundVisibility < threshold) discard;
    }
    vec3 normal = normalize(vNormal);
    float light = 0.35 + 0.65 * max(dot(normal, normalize(vec3(-0.45, 0.8, 0.5))), 0.0);
    if (scene.animation.x > 3.5 && scene.animation.x < 4.5)
        light = 0.07 + 1.15 * max(dot(normal, normalize(vec3(-0.9, 0.1, 0.20))), 0.0);
    float emission = clamp(scene.material.x, 0.0, 1.0);
    float windowBrightness = 1.0;
    float fireflyBrightness = 1.0;
    if ((scene.material.y > 7.5 && scene.material.y < 8.5) || (scene.material.y > 15.5 && scene.material.y < 16.5)) {
        // Each window has its own slow occupancy and short flicker cycle.
        vec2 cell;
        if (scene.material.y > 15.5) {
            // Train windows repeat within each carriage, whose spacing is
            // independent of the building-window grid.
            float carriage = floor((vPosition.x + 1.25) / 2.5);
            float localX = vPosition.x - carriage * 2.5;
            cell = vec2(carriage * 5.0 + floor((localX + 0.86) / 0.43 + 0.5), 0.0);
        } else {
            cell = floor(vPosition.xy / vec2(0.61, 0.74) + vec2(0.5));
        }
        float seed = cellNoise(cell);
        float cycle = floor(scene.animation.y * 0.025 + seed * 11.0);
        float occupancy = cellNoise(cell + cycle * 13.7);
        float on = smoothstep(0.25, 0.38, occupancy);
        float flicker = 0.82 + 0.18 * sin(scene.animation.y * (1.0 + seed * 2.0) + seed * 40.0);
        windowBrightness = mix(0.16, flicker * (0.65 + seed * 0.35), on);
        emission *= windowBrightness;
    }
    if (scene.material.y > 18.5 && scene.material.y < 19.5) {
        float flicker = 0.88 + 0.12 * sin(scene.animation.y * 0.4 + vPosition.x * 0.3);
        emission *= flicker;
    }
    if (scene.material.y > 17.5 && scene.material.y < 18.5) {
        float pulse = smoothstep(0.65, 0.97, sin(scene.animation.y * 1.2));
        emission *= 0.15 + 0.85 * pulse;
        windowBrightness = 0.35 + 0.65 * pulse;
    }
    if (scene.material.y > 9.5 && scene.material.y < 10.5) {
        float phase = cellNoise(floor(vPosition.xz * 4.0)) * 6.283;
        emission *= 0.65 + 0.35 * sin(scene.animation.y * 0.8 + phase);
    }
    if (scene.animation.x > 9.5 && scene.animation.x < 10.5)
        emission *= 0.35 + 0.65 * smoothstep(0.8, 0.98, sin(scene.animation.y * 2.0));
    if (scene.animation.x > 8.5 && scene.animation.x < 9.5)
        emission *= 0.78 + 0.22 * sin(scene.animation.y * 4.3);
    if (scene.animation.x > 2.5 && scene.animation.x < 3.5) {
        float seed = cellNoise(floor(vPosition.xz * 2.0));
        float rate = 0.7 + 0.1 * floor(seed * 5.0);
        float blink = sin(scene.animation.y * rate + seed * 6.283);
        fireflyBrightness = 0.18 + 0.82 * smoothstep(-0.4, 0.85, blink);
        emission *= fireflyBrightness;
    }
    if (scene.animation.x > 6.5 && scene.animation.x < 7.5)
        emission *= 0.85 + 0.15 * sin(scene.animation.y * 0.4);
    light = mix(light, 1.15, emission);
    vec2 surface = abs(normal.x) > abs(normal.z) ? vPosition.zy : vPosition.xy;
    if (abs(normal.y) > 0.75) surface = vPosition.xz;
    float textureFactor = 1.0;
    if (scene.material.y > 0.5 && scene.material.y < 1.5) {
        vec2 brick = surface * vec2(2.4, 3.4);
        brick.x += mod(floor(brick.y), 2.0) * 0.5;
        vec2 cell = floor(brick), local = fract(brick);
        float joint = max(step(local.x, 0.055), step(local.y, 0.09));
        textureFactor = (0.90 + 0.18 * cellNoise(cell)) * (1.0 - 0.18 * joint);
    } else if ((scene.material.y > 1.5 && scene.material.y < 4.5) || (scene.material.y > 12.5 && scene.material.y < 14.5) || (scene.material.y > 21.5 && scene.material.y < 22.5)) {
        vec2 cell = floor(surface * (scene.material.y > 3.5 ? 9.0 : 5.0));
        textureFactor = 0.87 + 0.23 * cellNoise(cell);
    }
    vec3 shaded = scene.color.rgb * light * textureFactor * windowBrightness * fireflyBrightness;
    if (scene.material.y > 8.5 && scene.material.y < 9.5)
        shaded *= 0.78 + 0.22 * sin(scene.animation.y * 1.3 + vPosition.x * 1.7 + vPosition.z * 0.8);
    if (scene.animation.x > 3.5 && scene.animation.x < 4.5) {
        // A narrow atmospheric limb keeps the distant crescent readable
        // without filling in the planet's unlit side.
        float limb = pow(1.0 - clamp(dot(normal, normalize(vec3(-0.25, 0.28, 0.92))), 0.0, 1.0), 5.0);
        shaded += vec3(0.055, 0.065, 0.095) * limb;
    }
    if (scene.material.y > 6.5 && scene.material.y < 7.5) {
        // Procedural continents and cloud systems follow the rotating globe.
        // Quantized coordinates retain the scene's pixel-art texture.
        vec3 globe = normalize(vPosition - vec3(-42.0, -56.0, -14.0));
        vec2 geo = vec2(atan(globe.z, globe.x) * 2.7, asin(clamp(globe.y, -1.0, 1.0)) * 2.7);
        vec2 landPoint = floor(geo * 22.0) / 22.0;
        float landShape = 0.65 * smoothNoise(landPoint * 1.5) + 0.35 * smoothNoise(landPoint * 3.4 + 8.0);
        float land = smoothstep(0.54, 0.63, landShape);
        vec2 cloudPoint = floor((geo + vec2(0.20 * sin(scene.animation.y * 0.03), 0.0)) * 30.0) / 30.0;
        float cloudShape = 0.65 * smoothNoise(cloudPoint * 3.0 + 22.0) + 0.35 * smoothNoise(cloudPoint * 7.0 + 13.0);
        float clouds = smoothstep(0.55, 0.68, cloudShape) * 0.82;
        vec3 landColor = scene.background.a < 0.0
            ? vec3(0.22, 0.40, 0.21) * (0.55 + 0.45 * light)
            : shaded * 1.65 + vec3(0.035, 0.045, 0.055);
        shaded = mix(shaded, landColor, land);
        shaded = mix(shaded, vec3(0.47, 0.51, 0.59) * (0.55 + 0.45 * light), clouds);
        float rim = pow(1.0 - clamp(dot(normal, normalize(vec3(0.0, 0.38, 0.92))), 0.0, 1.0), 4.0);
        shaded += vec3(0.15, 0.18, 0.29) * rim;
    }
    if (scene.animation.x > 0.5 && scene.animation.x < 1.5) {
        // Eighty-two full texture cycles fit the application's animation clock.
        float ripple = fract(vPosition.z * 1.25 + scene.animation.y * 0.13050705 + sin(vPosition.x * 0.55) * 0.12);
        float foam = step(ripple, 0.05) * step(0.55, cellNoise(floor(vPosition.xz * vec2(1.4, 0.8))));
        shaded += vec3(0.09, 0.10, 0.14) * foam;
    }
    // Recolor the scene from its luminance, retaining its lighting and detail
    // while letting the selected primary color determine the overall hue.
    if (scene.background.a >= 0.0) {
        vec3 primary = clamp(vec3(scene.background.a, scene.material.zw), 0.0, 1.0);
        float peak = max(max(primary.r, primary.g), primary.b);
        vec3 hue = peak > 0.001 ? primary / peak : vec3(1.0);
        float luminance = dot(shaded, vec3(0.2126, 0.7152, 0.0722));
        shaded = luminance * mix(vec3(1.0), hue, 0.72);
    }
    float alpha = scene.color.a;
    if (scene.animation.x > 11.5 && scene.animation.x < 12.5) {
        float phase = cellNoise(floor(vPosition.xz * 1.5));
        float pulse = fract(scene.animation.y * 0.35 + phase);
        alpha *= smoothstep(0.0, 0.18, pulse) * (1.0 - smoothstep(0.35, 0.85, pulse));
    }
    if (scene.animation.x > 4.5 && scene.animation.x < 5.5)
        alpha *= smoothstep(-9.0, -2.0, vPosition.x);
    if (scene.material.y > 5.5 && scene.material.y < 6.5)
        alpha *= smoothstep(0.0, 3.5, vPosition.y);
    outColor = vec4(mix(scene.background.rgb, shaded, scene.animation.z), alpha);
}
