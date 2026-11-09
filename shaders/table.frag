// The table, the floor, the pockets and the cue: cloth, rubber under cloth, polished wood, leather or net, and
// the marks drawn on the cloth: the spots, the head string, and the lines of the aim.

in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;
in vec3 vLocal;

// 0 cloth, 1 cushions, 2 rails, 3 frame and legs, 4 pockets, 5 floor, 6 cue
uniform int uKind;

uniform vec3 uCloth;
uniform vec4 uClothStyle;  // roughness, nap strength, sheen, wear
uniform vec3 uRailColor;
uniform vec4 uRailStyle;   // roughness, clear coat, pattern (0 wood, 1 lacquer), wood seed
uniform vec3 uFrameColor;
uniform vec4 uFrameStyle;  // roughness, clear coat, pattern, seed
uniform vec3 uPocketColor;
uniform vec4 uPocketStyle; // roughness, 1 for nets, 0, 0
uniform vec3 uFloorColor;
uniform vec4 uFloorStyle;  // roughness, pattern (0 boards, 1 carpet, 2 stone), 0, 0
uniform vec3 uSightColor;
uniform vec4 uSightStyle;  // metallic, roughness, shape (0 diamonds, 1 dots), glow
uniform vec4 uTable;       // length, width, ball radius, the x of the head string
uniform vec4 uMarks;       // x: 1 to show the head string, y: 1 for the Russian table
uniform vec4 uHoles[6];    // the pockets' holes in the cloth: xz, radius
uniform vec4 uCups[6];     // the openings in the rails: xz, radius
uniform vec4 uSights[24];  // xz, size
uniform int uSightCount;
uniform vec4 uGuides[16];  // segments of the aim: from xz, to xz
uniform int uGuideCount;
uniform vec4 uGuideColor;  // rgb, strength
uniform vec4 uPocketMark;  // a ring round the pocket called: xz, radius, strength
uniform vec4 uBalls[16];   // the balls on the cloth: xz, radius
uniform int uBallCount;

layout(location = 0) out vec4 fragColor;

// Wood with its grain along x: growth lines of uneven spacing and strength, fine fibres and a soft figure.
// Detail finer than the footprint, the size of a pixel in p, fades to its average instead of flickering.
vec3 Wood(vec2 p, vec3 color, float seed, float footprint)
{
    p += vec2(seed * 31.7, seed * 17.3);
    // The growth lines: their spacing drifts across the grain, and they wander a little along it.
    float t = p.y * 7.0 + 2.5 * Fbm(vec2(p.y * 0.8, seed * 5.0)) + 0.8 * Fbm(vec2(p.x * 0.3, p.y * 1.2)) + 0.2 * sin(p.x * 1.1 + seed * 6.0);
    float ring = floor(t);
    float phase = t - ring;
    float width = 0.2 + 0.3 * Hash12(vec2(ring, seed));
    float late = smoothstep(0.0, 0.08, phase) * (1.0 - smoothstep(width, width + 0.25, phase));
    late *= 0.35 + 0.9 * Hash12(vec2(ring, seed + 7.0));
    late = mix(late, 0.3, smoothstep(0.04, 0.12, footprint));
    float fibers = 0.5 * Noise(vec2(p.x * 3.0, p.y * 260.0)) + 0.5 * Noise(vec2(p.x * 7.0, p.y * 600.0));
    fibers = mix(fibers, 0.5, smoothstep(0.001, 0.004, footprint));
    float figure = Noise(p * vec2(0.8, 2.2) + seed * 9.0);
    return color * (1.0 - 0.2 * late) * (0.9 + 0.16 * fibers) * (0.9 + 0.2 * figure);
}

// Polished stone: a cloudy ground with faint veins wandering through it.
vec3 Marble(vec2 p, vec3 color, vec3 veinColor, float seed)
{
    p += seed * 10.0;
    float warp = Fbm(p * 1.1) * 3.0;
    float wave = abs(sin((p.x * 1.2 + p.y * 0.7 + warp) * 2.2));
    float vein = pow(1.0 - wave, 26.0) + 0.3 * pow(1.0 - abs(sin((p.x * 2.6 - p.y * 1.9 + warp * 1.4) * 2.4)), 40.0);
    vec3 c = color * (0.85 + 0.3 * Fbm(p * 2.2 + 5.0));
    return mix(c, veinColor, Saturate(vein) * 0.35);
}

// Felt: fine fibres along the nap and a soft mottle, fading to their average where they are finer than a pixel.
vec3 Cloth(vec2 p, float footprint)
{
    float fibres = 0.5 * Noise(vec2(p.x * 900.0, p.y * 280.0)) + 0.5 * Noise(vec2(p.x * 260.0, p.y * 900.0));
    fibres = mix(fibres, 0.5, smoothstep(0.0006, 0.002, footprint));
    float mottle = Fbm(p * 3.0) - 0.5;
    float nap = mix(Noise(vec2(p.x * 40.0, p.y * 6.0)), 0.5, smoothstep(0.004, 0.012, footprint));
    return uCloth * (0.92 + 0.14 * fibres + 0.12 * mottle * uClothStyle.w + 0.06 * (nap - 0.5) * uClothStyle.y);
}

// Coverage of the inside of a circle, over the pixel.
float Inside(vec2 p, vec2 center, float radius, float pixel)
{
    return 1.0 - smoothstep(radius - pixel, radius + pixel, length(p - center));
}

float SegmentDistance(vec2 p, vec2 a, vec2 b)
{
    vec2 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-9), 0.0, 1.0);
    return length(p - a - ab * t);
}

// How much of the light from around a point of the cloth the balls near it keep off: dark under each ball,
// fading within a couple of radii.
float BallOcclusion(vec2 p)
{
    float open = 1.0;
    for (int i = 0; i < uBallCount; ++i) {
        vec2 d = (p - uBalls[i].xy) / uBalls[i].z;
        float q = 1.0 / (1.0 + dot(d, d));
        open *= 1.0 - 0.8 * q * sqrt(q);
    }
    return open;
}

// The ring round the pocket called, the same seen on the cloth, the cushions and the rails.
float PocketRing(vec2 p, float pixel)
{
    if (uPocketMark.w <= 0.0)
        return 0.0;
    return LineCoverage(abs(length(p - uPocketMark.xy) - uPocketMark.z), 0.0022, pixel * 1.5) * uPocketMark.w;
}

void main()
{
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing)
        N = -N;
    vec3 V = normalize(uEye.xyz - vWorld);
    vec2 p = vWorld.xz;
    float footprint = length(fwidth(p));
    float pixel = max(footprint * 0.7, 1e-5);
    Material m = DefaultMaterial(vec3(0.5), 0.5);
    vec3 glow = vec3(0.0);
    float coverage = 1.0;
    float contact = 1.0; // the shade of the balls close by, on the key light too

    if (uKind == 0 || uKind == 1) {
        m.albedo = Cloth(p, footprint);
        m.roughness = uClothStyle.x;
        m.subsurface = 0.1;
        if (uKind == 1) {
            m.albedo *= 0.92;
        }
        else {
            // The holes of the pockets.
            for (int i = 0; i < 6; ++i)
                coverage *= 1.0 - Inside(p, uHoles[i].xy, uHoles[i].z, pixel);
            // The spots, and the head string when a ball is placed behind it.
            float R = uTable.z;
            float spotRadius = uMarks.y > 0.5 ? R * 0.18 : R * 0.3;
            float spot = 0.0;
            spot = max(spot, Inside(p, vec2(uTable.x * 0.25, 0.0), spotRadius, pixel));
            spot = max(spot, Inside(p, vec2(-uTable.x * 0.25, 0.0), spotRadius, pixel));
            spot = max(spot, Inside(p, vec2(0.0), spotRadius * 0.8, pixel));
            m.albedo = mix(m.albedo, vec3(0.75), spot * 0.85);
            if (uMarks.x > 0.5) {
                float line = LineCoverage(abs(p.x - uTable.w), 0.0012, max(fwidth(p.x), 1e-5)) * step(abs(p.y), uTable.y * 0.5);
                m.albedo = mix(m.albedo, vec3(0.8), line * 0.7);
            }
            // The lines of the aim, glowing on the cloth.
            for (int i = 0; i < uGuideCount; ++i) {
                float d = SegmentDistance(p, uGuides[i].xy, uGuides[i].zw);
                float line = LineCoverage(d, 0.0012, pixel * 1.4);
                glow += uGuideColor.rgb * uGuideColor.a * line;
                m.albedo = mix(m.albedo, uGuideColor.rgb * 0.6, line * 0.5);
            }
        }
        float balls = BallOcclusion(p);
        m.occlusion = balls;
        contact = mix(1.0, balls, 0.6);
        // The sheen of cloth at grazing angles.
        m.emissive += uCloth * uClothStyle.z * pow(1.0 - Saturate(dot(N, V)), 4.0) * 0.4;
    }
    else if (uKind == 2 || uKind == 3) {
        vec4 style = uKind == 2 ? uRailStyle : uFrameStyle;
        vec3 color = uKind == 2 ? uRailColor : uFrameColor;
        // The grain runs along the rail.
        bool alongX = abs(N.x) < 0.7 && (uKind == 3 || abs(p.y) > uTable.y * 0.5 + 0.02);
        vec2 grain = alongX ? vec2(p.x, p.y + vWorld.y * 0.6) : vec2(p.y, p.x + vWorld.y * 0.6);
        if (uKind == 3 && abs(N.y) < 0.5 && length(vLocal.xz) < 0.2 && vWorld.y < -0.15)
            grain = vec2(vWorld.y * 0.3, atan(vLocal.z, vLocal.x) * 0.1); // a turned leg
        m.albedo = style.z > 0.5 ? color * (0.95 + 0.1 * Noise(p * 30.0)) : Wood(grain * vec2(0.6, 1.6), color, style.w, footprint);
        m.roughness = style.x;
        m.clearcoat = style.y;
        m.clearcoatRoughness = 0.09;
        if (uKind == 2) {
            // The openings of the pockets.
            for (int i = 0; i < 6; ++i)
                coverage *= 1.0 - Inside(p, uCups[i].xy, uCups[i].z, pixel);
            // The sights inlaid in the top.
            if (N.y > 0.9) {
                float inlay = 0.0;
                for (int i = 0; i < uSightCount; ++i) {
                    vec2 d = abs(p - uSights[i].xy);
                    float size = uSights[i].z;
                    float shape = uSightStyle.z > 0.5 ? length(d) : (d.x + d.y) * 0.75;
                    inlay = max(inlay, 1.0 - smoothstep(size - pixel, size + pixel, shape));
                }
                float pearl = 0.85 + 0.15 * sin(dot(p, vec2(900.0, 700.0)));
                m.albedo = mix(m.albedo, uSightColor * pearl, inlay);
                m.metallic = mix(m.metallic, uSightStyle.x, inlay);
                m.roughness = mix(m.roughness, uSightStyle.y, inlay);
                glow += uSightColor * uSightStyle.w * inlay;
            }
        }
        // Darker toward the floor.
        m.occlusion = mix(0.55, 1.0, smoothstep(-0.35, 0.02, vWorld.y));
    }
    else if (uKind == 4) {
        // Leather, or below the cloth a net of cord.
        m.albedo = uPocketColor * (0.85 + 0.3 * Noise(vUv * vec2(60.0, 300.0)));
        m.roughness = uPocketStyle.x;
        m.occlusion = mix(0.25, 1.0, smoothstep(-0.1, 0.03, vWorld.y));
        if (uPocketStyle.y > 0.5 && vWorld.y < -0.012) {
            vec2 net = vec2(vUv.x * 40.0, vUv.y * 70.0);
            vec2 cell = abs(fract(net + vec2(0.5 * floor(net.y), 0.0)) - 0.5);
            vec2 width = fwidth(net) * 0.8 + 0.12;
            float cord = max(1.0 - smoothstep(width.x * 0.5, width.x, cell.x), 1.0 - smoothstep(width.y * 0.5, width.y, cell.y));
            coverage = cord;
            m.albedo = uPocketColor * 1.6;
        }
    }
    else if (uKind == 5) {
        int pattern = int(uFloorStyle.y + 0.5);
        if (pattern == 0) {
            // Boards, each its own shade.
            float board = floor(p.y / 0.18);
            vec2 q = vec2(p.x + board * 1.37, p.y);
            m.albedo = Wood(q * vec2(0.5, 2.0), uFloorColor * (0.85 + 0.3 * Hash12(vec2(board, 3.0))), 0.2 + board * 0.13, footprint);
            float seam = 1.0 - smoothstep(0.0, 0.004 + footprint, abs(fract(p.y / 0.18) - 0.0) * 0.18);
            m.albedo *= 1.0 - 0.5 * seam;
        }
        else if (pattern == 1) {
            m.albedo = uFloorColor * (0.85 + 0.3 * mix(Noise(p * 120.0), 0.5, smoothstep(0.004, 0.012, footprint)));
        }
        else {
            // Square slabs of stone, each cut from another part of the block, with thin joints between.
            vec2 tile = floor(p / 0.9);
            vec2 inTile = fract(p / 0.9);
            float shade = 0.85 + 0.3 * Hash12(tile + 7.0);
            m.albedo = Marble(p * 0.7 + tile * 3.7, uFloorColor * shade, uFloorColor * 1.8, 3.0);
            vec2 edge = min(inTile, 1.0 - inTile) * 0.9;
            float joint = 1.0 - smoothstep(0.0015, 0.0015 + footprint, min(edge.x, edge.y));
            m.albedo *= 1.0 - 0.6 * joint;
        }
        m.roughness = uFloorStyle.x;
        // The table's shadow and the darkness away from the lamp.
        float r = length(max(abs(p) - uTable.xy * 0.5, 0.0));
        m.occlusion = mix(0.35, 1.0, smoothstep(0.0, 0.8, r));
        m.albedo *= mix(1.0, 0.1, smoothstep(0.0, 2.2, r));
    }
    else {
        // The cue, by the distance from its tip.
        float z = vLocal.z;
        vec2 grain = vec2(z * 3.0, atan(vLocal.y, vLocal.x) * 0.02);
        if (z < 0.009) {
            m.albedo = vec3(0.06, 0.18, 0.55) * (0.8 + 0.4 * Noise(vLocal.xy * 3000.0));
            m.roughness = 0.9;
        }
        else if (z < 0.03) {
            m.albedo = vec3(0.85, 0.83, 0.76);
            m.roughness = 0.3;
        }
        else if (z < 0.74) {
            m.albedo = Wood(grain * vec2(1.0, 30.0), vec3(0.72, 0.56, 0.36), 0.4, footprint);
            m.roughness = 0.35;
            m.clearcoat = 0.5;
        }
        else if (z < 0.77) {
            m.albedo = vec3(0.75, 0.74, 0.72);
            m.metallic = 1.0;
            m.roughness = 0.2;
        }
        else if (z < 1.04) {
            // The forearm: dark wood with long points of maple.
            float angle = atan(vLocal.y, vLocal.x);
            float point = step(abs(fract(angle / 6.2831853 * 4.0) - 0.5) * 2.0, Saturate((1.04 - z) / 0.22) * 0.7);
            m.albedo = mix(Wood(grain * vec2(1.0, 30.0), vec3(0.07, 0.025, 0.015), 0.8, footprint), vec3(0.7, 0.55, 0.35), point);
            m.roughness = 0.25;
            m.clearcoat = 0.8;
        }
        else if (z < 1.33) {
            // The wrap.
            float weave = 0.5 + 0.5 * sin(z * 900.0) * sin(atan(vLocal.y, vLocal.x) * 40.0);
            m.albedo = vec3(0.025, 0.025, 0.03) * (0.8 + 0.4 * mix(weave, 0.5, smoothstep(0.002, 0.006, footprint)));
            m.roughness = 0.75;
        }
        else {
            m.albedo = Wood(grain * vec2(1.0, 30.0), vec3(0.06, 0.02, 0.012), 0.3, footprint);
            m.roughness = 0.25;
            m.clearcoat = 0.8;
        }
    }

    if (uKind <= 2) {
        float ring = PocketRing(p, pixel);
        glow += uGuideColor.rgb * ring * 1.8;
        m.albedo = mix(m.albedo, uGuideColor.rgb * 0.5, ring * 0.6);
    }
    if (coverage <= 0.002)
        discard;
    float spread = NormalSpread(N);
    m.roughness = WidenRoughness(m.roughness, spread);
    m.clearcoatRoughness = WidenRoughness(m.clearcoatRoughness, spread);
    // The cue is too thin to shade itself cleanly; it still casts its shadow on the table.
    float shadow = (uEye.w > 0.5 || uKind == 6 ? 1.0 : Shadow(vWorld, N)) * contact;
    vec3 color = Shade(m, vWorld, N, V, shadow) + glow;
    fragColor = vec4(ApplyFog(color, vWorld), coverage);
}
