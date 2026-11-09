// A ball: pool balls in their colors with stripes and numbered circles, the cue ball with red dots, or the
// ivory balls of the Russian game with their numbers and an amber cue ball. Polished, with a clear coat.

in vec3 vWorld;
in vec3 vNormal;
in vec3 vLocal;
flat in vec4 vParams;
flat in vec4 vExtra;
flat in vec3 vCenter;

layout(binding = 6) uniform sampler2D uNumbers; // 4 × 4 cells: the numbers 1 to 15

uniform vec4 uBallSurface; // roughness, clear coat roughness, ivory tint strength, 0

layout(location = 0) out vec4 fragColor;

const float CircleAngle = 0.36; // the numbered circle, as an angle from its middle

vec3 PoolColor(int number)
{
    int k = (number - 1) % 8;
    if (k == 0) return vec3(0.93, 0.52, 0.0);
    if (k == 1) return vec3(0.0, 0.055, 0.38);
    if (k == 2) return vec3(0.68, 0.006, 0.006);
    if (k == 3) return vec3(0.085, 0.012, 0.2);
    if (k == 4) return vec3(0.95, 0.15, 0.0);
    if (k == 5) return vec3(0.0, 0.17, 0.042);
    if (k == 6) return vec3(0.2, 0.012, 0.01);
    return vec3(0.01);
}

// The number as ink, 0 to 1, on the two sides of the ball facing ±x; and the circle round it.
float NumberInk(vec3 n, int number, out float circle)
{
    float side = n.x >= 0.0 ? 1.0 : -1.0;
    float c = abs(n.x);
    float edge = cos(CircleAngle);
    float width = max(fwidth(c), 1e-4);
    circle = smoothstep(edge - width, edge + width, c);
    if (c < edge - width)
        return 0.0;
    vec2 local = vec2(-side * n.z, n.y) / sin(CircleAngle); // -1 to 1 across the circle, upright
    vec2 cell = vec2(float((number - 1) % 4), float((number - 1) / 4));
    vec2 uv = (cell + 0.5 + vec2(0.5, -0.5) * local * 0.86) / 4.0;
    return texture(uNumbers, uv).r * circle;
}

void main()
{
    vec3 N = normalize(vNormal);
    float spread = NormalSpread(N);
    vec3 n = normalize(vLocal);
    int number = int(vParams.x + 0.5);
    bool russian = vParams.y > 0.5;
    vec3 white = vec3(0.86, 0.85, 0.81);
    vec3 albedo = white;
    float circle = 0.0;

    if (russian) {
        // Ivory, a little warm and uneven; the cue ball amber.
        float cloud = Fbm(n * 3.0 + vParams.x * 1.7);
        albedo = number == 0 ? vec3(0.8, 0.42, 0.05) * (0.94 + 0.12 * cloud) : vec3(0.8, 0.74, 0.58) * (0.95 + 0.1 * cloud * uBallSurface.z);
        if (number > 0) {
            float ink = NumberInk(n, number, circle);
            albedo = mix(albedo, vec3(0.18, 0.015, 0.012), ink);
        }
    }
    else if (number == 0) {
        // Red dots on the six axes, to show the spin.
        float d = max(max(abs(n.x), abs(n.y)), abs(n.z));
        float edge = cos(0.1);
        float w = max(fwidth(d), 1e-4);
        albedo = mix(white, vec3(0.55, 0.01, 0.015), smoothstep(edge - w, edge + w, d));
    }
    else {
        vec3 color = PoolColor(number);
        if (number >= 9) {
            float w = max(fwidth(n.y), 1e-4);
            albedo = mix(color, white, smoothstep(0.58 - w, 0.58 + w, abs(n.y)));
        }
        else {
            albedo = color;
        }
        float ink = NumberInk(n, number, circle);
        albedo = mix(albedo, white, circle);
        albedo = mix(albedo, vec3(0.01), ink);
    }

    vec3 V = normalize(uEye.xyz - vWorld);
    Material m = DefaultMaterial(albedo, WidenRoughness(uBallSurface.x, spread));
    m.clearcoat = 1.0;
    m.clearcoatRoughness = WidenRoughness(uBallSurface.y, spread);
    m.subsurface = russian ? 0.2 : 0.05;
    // The cloth darkens the underside, most near the point it stands on.
    float above = vWorld.y - vExtra.x;
    m.occlusion = mix(0.25, 1.0, smoothstep(0.0, 1.4 * length(vWorld - vCenter), above + 0.0001));
    float shadow = uEye.w > 0.5 ? 1.0 : Shadow(vWorld, N);
    vec3 color = Shade(m, vWorld, N, V, shadow);

    // A highlighted ball glows at its rim.
    float rim = pow(1.0 - Saturate(dot(N, V)), 2.5);
    color += uAccent.rgb * vParams.w * (0.15 + 1.4 * rim);
    fragColor = vec4(ApplyFog(color, vWorld), vParams.z);
}
