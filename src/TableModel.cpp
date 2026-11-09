#include "TableModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace model {

using namespace math;
using billiards::TableSpec;
using billiards::V2;

namespace {

Vec3 At(V2 p, double y) { return {static_cast<float>(p.x), static_cast<float>(y), static_cast<float>(p.z)}; }
Vec2 Uv(V2 p) { return {static_cast<float>(p.x), static_cast<float>(p.z)}; }

// Adds a triangle, wound counterclockwise as seen from the side its normals face.
void AddTriangle(Mesh& mesh, const Vertex& a, const Vertex& b, const Vertex& c)
{
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), {a, b, c});
    const Vec3 facing = Cross(b.position - a.position, c.position - a.position);
    if (Dot(facing, a.normal + b.normal + c.normal) >= 0)
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2});
    else
        mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1});
}

void AddQuad(Mesh& mesh, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    AddTriangle(mesh, a, b, c);
    AddTriangle(mesh, a, c, d);
}

void Append(Mesh& target, const Mesh& source, const Mat4& transform)
{
    const auto base = static_cast<std::uint32_t>(target.vertices.size());
    for (Vertex v : source.vertices) {
        v.position = transform.TransformPoint(v.position);
        v.normal = Normalize(transform.TransformDirection(v.normal));
        target.vertices.push_back(v);
    }
    for (const std::uint32_t index : source.indices)
        target.indices.push_back(base + index);
}

struct ProfilePoint {
    float r, y;
};

// An outline turned round the y axis; `inside` for a cup seen from within. uv: the turn, and the distance along
// the outline.
Mesh Lathe(const std::vector<ProfilePoint>& profile, int segments, bool inside)
{
    Mesh mesh;
    const int count = static_cast<int>(profile.size());
    std::vector<float> along(profile.size(), 0.0f);
    for (int i = 1; i < count; ++i)
        along[static_cast<std::size_t>(i)] = along[static_cast<std::size_t>(i - 1)] +
                                             std::hypot(profile[static_cast<std::size_t>(i)].r - profile[static_cast<std::size_t>(i - 1)].r,
                                                        profile[static_cast<std::size_t>(i)].y - profile[static_cast<std::size_t>(i - 1)].y);
    for (int i = 0; i < count; ++i) {
        const ProfilePoint& before = profile[static_cast<std::size_t>(std::max(i - 1, 0))];
        const ProfilePoint& after = profile[static_cast<std::size_t>(std::min(i + 1, count - 1))];
        float nr = after.y - before.y, ny = -(after.r - before.r);
        const float length = std::max(std::hypot(nr, ny), 1e-6f);
        nr /= length;
        ny /= length;
        if (inside) {
            nr = -nr;
            ny = -ny;
        }
        const ProfilePoint& point = profile[static_cast<std::size_t>(i)];
        for (int j = 0; j <= segments; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(segments);
            const float c = std::cos(2 * Pi * t), s = std::sin(2 * Pi * t);
            mesh.vertices.push_back({{point.r * c, point.y, point.r * s}, Normalize(Vec3{nr * c, ny, nr * s}), {t, along[static_cast<std::size_t>(i)]}});
        }
    }
    auto index = [&](int i, int j) { return static_cast<std::uint32_t>(i * (segments + 1) + j); };
    for (int i = 0; i + 1 < count; ++i) {
        for (int j = 0; j < segments; ++j) {
            const std::uint32_t a = index(i, j), b = index(i, j + 1), c = index(i + 1, j + 1), d = index(i + 1, j);
            // Outward the outline runs downward on its right; flipped when seen from inside.
            if (!inside)
                mesh.indices.insert(mesh.indices.end(), {a, c, b, a, d, c});
            else
                mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
        }
    }
    // The winding follows the normals, whichever way the outline was given.
    for (std::size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[k]];
        const Vertex& b = mesh.vertices[mesh.indices[k + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[k + 2]];
        const Vec3 facing = Cross(b.position - a.position, c.position - a.position);
        if (Length(facing) > 1e-12f && Dot(facing, a.normal + b.normal + c.normal) < 0)
            std::swap(mesh.indices[k + 1], mesh.indices[k + 2]);
    }
    return mesh;
}

// A cushion between two jaws: its outline seen from above (the jaws' facings, the rounded tips and the nose),
// walls round it, a narrow bevel, and a top rising from the nose to the rail.
void AddCushion(Mesh& mesh, const TableSpec& table, const billiards::Segment& nose)
{
    const V2 out = -nose.normal;
    const double cw = table.cushionWidth, hn = table.cushionHeight, top = table.railHeight - 0.0015;
    auto depth = [&](V2 p) { return std::clamp(Dot(p - nose.a, out), 0.0, cw); };
    auto height = [&](V2 p) { return hn + (top - hn) * depth(p) / cw; };

    // The jaw at an end of the nose: its tip, and the facing that leads away from it.
    struct Jaw {
        billiards::Tip tip;
        billiards::Segment facing;
    };
    auto jawAt = [&](V2 end) {
        Jaw jaw{};
        for (const billiards::Tip& tip : table.tips)
            if (billiards::Length(tip.center - (end + out * tip.radius)) < 1e-6)
                jaw.tip = tip;
        for (std::size_t k = 0; k + 6 < table.segments.size(); ++k)
            if (std::abs(billiards::Length(table.segments[k].a - jaw.tip.center) - jaw.tip.radius) < 1e-6 && billiards::Length(table.segments[k].a - end) > 1e-6)
                jaw.facing = table.segments[k];
        return jaw;
    };
    const Jaw start = jawAt(nose.a), end = jawAt(nose.b);
    auto backOfFacing = [&](const billiards::Segment& facing) {
        const V2 direction = billiards::Normalized(facing.b - facing.a);
        const double s = (cw - Dot(facing.a - nose.a, out)) / std::max(Dot(direction, out), 0.05);
        return facing.a + direction * s;
    };
    auto arc = [&](std::vector<V2>& outline, const billiards::Tip& tip, V2 from, V2 to) {
        const double a0 = std::atan2(from.z - tip.center.z, from.x - tip.center.x);
        double a1 = std::atan2(to.z - tip.center.z, to.x - tip.center.x);
        while (a1 - a0 > Pi)
            a1 -= 2 * Pi;
        while (a1 - a0 < -Pi)
            a1 += 2 * Pi;
        constexpr int Steps = 8;
        for (int k = 1; k < Steps; ++k) {
            const double a = a0 + (a1 - a0) * k / Steps;
            outline.push_back(tip.center + V2{std::cos(a), std::sin(a)} * tip.radius);
        }
    };
    std::vector<V2> outline;
    outline.push_back(backOfFacing(start.facing));
    outline.push_back(start.facing.a);
    arc(outline, start.tip, start.facing.a, nose.a);
    outline.push_back(nose.a);
    outline.push_back(nose.b);
    arc(outline, end.tip, nose.b, end.facing.a);
    outline.push_back(end.facing.a);
    outline.push_back(backOfFacing(end.facing));

    // The outward normal of each outline point: the mean of its edges', the back edge left out.
    const std::size_t n = outline.size();
    std::vector<V2> normals(n);
    V2 middle{};
    for (const V2 p : outline)
        middle = middle + p * (1.0 / static_cast<double>(n));
    auto edgeNormal = [&](std::size_t i, std::size_t j) {
        V2 normal = billiards::Normalized(billiards::Perpendicular(outline[j] - outline[i]));
        if (Dot(normal, (outline[i] + outline[j]) * 0.5 - middle) < 0)
            normal = -normal;
        return normal;
    };
    for (std::size_t i = 0; i < n; ++i) {
        V2 sum{};
        if (i > 0)
            sum = sum + edgeNormal(i - 1, i);
        if (i + 1 < n)
            sum = sum + edgeNormal(i, i + 1);
        normals[i] = billiards::Normalized(sum);
    }

    constexpr double Bevel = 0.004;
    std::vector<V2> inset(n);
    for (std::size_t i = 0; i < n; ++i)
        inset[i] = outline[i] - normals[i] * Bevel;
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const std::size_t j = i + 1;
        const Vec3 ni = At(normals[i], 0), nj = At(normals[j], 0);
        // The wall, the bevel above it, and the top.
        AddQuad(mesh, {At(outline[i], 0), ni, Uv(outline[i])}, {At(outline[j], 0), nj, Uv(outline[j])},
                {At(outline[j], height(outline[j]) - Bevel), nj, Uv(outline[j])}, {At(outline[i], height(outline[i]) - Bevel), ni, Uv(outline[i])});
        const Vec3 bi = Normalize(ni + Vec3{0, 1, 0}), bj = Normalize(nj + Vec3{0, 1, 0});
        AddQuad(mesh, {At(outline[i], height(outline[i]) - Bevel), bi, Uv(outline[i])}, {At(outline[j], height(outline[j]) - Bevel), bj, Uv(outline[j])},
                {At(inset[j], height(inset[j])), {0, 1, 0}, Uv(inset[j])}, {At(inset[i], height(inset[i])), {0, 1, 0}, Uv(inset[i])});
    }
    const Vec3 up = Normalize(Vec3{static_cast<float>(-out.x * (top - hn) / cw), 1, static_cast<float>(-out.z * (top - hn) / cw)});
    V2 center{};
    for (const V2 p : inset)
        center = center + p * (1.0 / static_cast<double>(n));
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        AddTriangle(mesh, {At(center, height(center)), up, Uv(center)}, {At(inset[i], height(inset[i])), up, Uv(inset[i])},
                    {At(inset[j], height(inset[j])), up, Uv(inset[j])});
    }
}

// A straight rail: its top, the rounded outer edge, the outer side down to the apron, and the inner side.
// `inner` and `outer` run along the rail at the cushion's back and at the rail's outside; `out` points outward.
void AddRail(Mesh& mesh, V2 innerA, V2 innerB, V2 out, double width, double top, double bottom, bool caps)
{
    constexpr double Round = 0.016;
    constexpr int Steps = 6;
    struct Ring {
        double d, y;
        V2 normal;
        double ny;
    };
    std::vector<Ring> profile;
    profile.push_back({0, 0, -out, 0});
    profile.push_back({0, top, -out, 0});
    profile.push_back({0, top, {0, 0}, 1});
    for (int k = 0; k <= Steps; ++k) {
        const double a = (Pi / 2) * static_cast<double>(k) / Steps;
        profile.push_back({width - Round + Round * std::sin(a), top - Round + Round * std::cos(a), out * std::sin(a), std::cos(a)});
    }
    profile.push_back({width, bottom, out, 0});
    const V2 along = innerB - innerA;
    for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
        const Ring& p = profile[i];
        const Ring& q = profile[i + 1];
        if (std::abs(p.d - q.d) < 1e-9 && std::abs(p.y - q.y) < 1e-9)
            continue;
        auto vertex = [&](const Ring& r, double s) {
            const V2 position = innerA + along * s + out * r.d;
            return Vertex{At(position, r.y), Normalize(Vec3{static_cast<float>(r.normal.x), static_cast<float>(r.ny), static_cast<float>(r.normal.z)}), Uv(position)};
        };
        AddQuad(mesh, vertex(p, 0), vertex(p, 1), vertex(q, 1), vertex(q, 0));
    }
    if (!caps)
        return;
    // The ends of a rail that reaches round the corners.
    for (const double s : {0.0, 1.0}) {
        const V2 normal = billiards::Normalized(along) * (s == 0 ? -1.0 : 1.0);
        const V2 base = innerA + along * s;
        const Vec3 n3 = At(normal, 0);
        for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
            auto point = [&](const Ring& r) {
                const V2 p = base + out * r.d;
                return Vertex{At(p, r.y), n3, Uv(p)};
            };
            AddTriangle(mesh, point(Ring{width * 0.5, bottom, {}, 0}), point(profile[i]), point(profile[i + 1]));
        }
    }
}

// A box; without its top where the pockets hang inside it.
void AddBox(Mesh& mesh, Vec3 low, Vec3 high, bool top = true)
{
    const Vec3 c[8] = {{low.x, low.y, low.z}, {high.x, low.y, low.z}, {high.x, high.y, low.z}, {low.x, high.y, low.z},
                       {low.x, low.y, high.z}, {high.x, low.y, high.z}, {high.x, high.y, high.z}, {low.x, high.y, high.z}};
    auto face = [&](int a, int b, int d, int e, Vec3 normal) {
        auto v = [&](int i) { return Vertex{c[i], normal, {c[i].x + c[i].y, c[i].z + c[i].y}}; };
        AddQuad(mesh, v(a), v(b), v(d), v(e));
    };
    face(0, 1, 2, 3, {0, 0, -1});
    face(4, 5, 6, 7, {0, 0, 1});
    face(0, 4, 7, 3, {-1, 0, 0});
    face(1, 5, 6, 2, {1, 0, 0});
    face(0, 1, 5, 4, {0, -1, 0});
    if (top)
        face(3, 2, 6, 7, {0, 1, 0});
}

Mesh BuildLeg(float height, float radius)
{
    // A turned leg: a collar under the frame, a swelling, a waist, and a foot.
    const float h = height, r = radius;
    std::vector<ProfilePoint> profile{{0, 0},          {r * 0.9f, 0},      {r * 0.95f, -0.03f * h}, {r * 0.75f, -0.06f * h},
                                      {r * 0.8f, -0.1f * h}, {r * 1.05f, -0.3f * h}, {r * 1.1f, -0.42f * h}, {r * 0.95f, -0.55f * h},
                                      {r * 0.62f, -0.72f * h}, {r * 0.7f, -0.86f * h}, {r * 0.95f, -0.92f * h}, {r * 1.0f, -0.97f * h},
                                      {r * 0.9f, -h},    {0, -h}};
    return Lathe(profile, 40, false);
}

} // namespace

TableMeshes BuildTable(const TableSpec& table)
{
    TableMeshes meshes;
    const double hl = table.length / 2, hw = table.width / 2, cw = table.cushionWidth, rw = table.railWidth, rh = table.railHeight;
    const bool pool = table.kind == billiards::TableKind::Pool;

    // The cloth, as one rectangle; the holes of the pockets are cut in the shader.
    {
        const V2 corners[4] = {{-hl - cw, -hw - cw}, {hl + cw, -hw - cw}, {hl + cw, hw + cw}, {-hl - cw, hw + cw}};
        auto v = [&](V2 p) { return Vertex{At(p, 0), {0, 1, 0}, Uv(p)}; };
        AddQuad(meshes.bed, v(corners[0]), v(corners[1]), v(corners[2]), v(corners[3]));
    }

    // The cushions: the last six segments are their noses.
    for (std::size_t k = table.segments.size() - 6; k < table.segments.size(); ++k)
        AddCushion(meshes.cushions, table, table.segments[k]);

    // The rails: the long ones reach round the corners, the short ones fit between them.
    const double railBottom = rh - 0.065;
    for (const double sz : {1.0, -1.0})
        AddRail(meshes.rails, {-hl - cw - rw, sz * (hw + cw)}, {hl + cw + rw, sz * (hw + cw)}, {0, sz}, rw, rh, railBottom, true);
    for (const double sx : {1.0, -1.0})
        AddRail(meshes.rails, {sx * (hl + cw), -hw - cw}, {sx * (hl + cw), hw + cw}, {sx, 0}, rw, rh, railBottom, false);

    // The apron under the rails, and the legs down to the floor.
    const float outerX = static_cast<float>(hl + cw + rw), outerZ = static_cast<float>(hw + cw + rw);
    const float apron = pool ? 0.17f : 0.21f;
    meshes.floorHeight = pool ? -0.78f : -0.8f;
    AddBox(meshes.frame, {-outerX + 0.012f, -apron, -outerZ + 0.012f}, {outerX - 0.012f, static_cast<float>(railBottom), outerZ - 0.012f}, false);
    AddBox(meshes.frame, {-outerX + 0.05f, -apron - 0.03f, -outerZ + 0.05f}, {outerX - 0.05f, -apron, outerZ - 0.05f});
    const float legRadius = pool ? 0.065f : 0.075f;
    const Mesh leg = BuildLeg(-meshes.floorHeight - apron - 0.03f, legRadius);
    std::vector<V2> legs;
    for (const double sx : {-1.0, 1.0})
        for (const double sz : {-1.0, 1.0})
            legs.push_back({sx * (outerX - 0.17), sz * (outerZ - 0.15)});
    if (!pool) {
        legs.push_back({0, outerZ - 0.15});
        legs.push_back({0, -(outerZ - 0.15)});
    }
    for (const V2 p : legs)
        Append(meshes.frame, leg, Mat4::Translation(At(p, -apron - 0.03f)));
    return meshes;
}

Mesh BuildPocket(float radius, float top, float depth)
{
    // A cup with a rounded bottom, seen from within.
    const float r = radius;
    std::vector<ProfilePoint> profile{{r, top}, {r, -depth * 0.45f}};
    for (int k = 1; k <= 8; ++k) {
        const float a = (Pi / 2) * static_cast<float>(k) / 8;
        profile.push_back({r * std::cos(a), -depth * 0.45f - depth * 0.55f * std::sin(a)});
    }
    profile.back().r = 0;
    return Lathe(profile, 48, true);
}

Mesh BuildFloor(float size, float height)
{
    Mesh mesh;
    auto v = [&](float x, float z) { return Vertex{{x, height, z}, {0, 1, 0}, {x, z}}; };
    AddQuad(mesh, v(-size, -size), v(size, -size), v(size, size), v(-size, size));
    return mesh;
}

Mesh BuildBall(int rings, int segments)
{
    Mesh mesh;
    for (int i = 0; i <= rings; ++i) {
        const float theta = Pi * static_cast<float>(i) / static_cast<float>(rings);
        for (int j = 0; j <= segments; ++j) {
            const float phi = 2 * Pi * static_cast<float>(j) / static_cast<float>(segments);
            const Vec3 n{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
            mesh.vertices.push_back({n, n, {static_cast<float>(j) / static_cast<float>(segments), static_cast<float>(i) / static_cast<float>(rings)}});
        }
    }
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const auto a = static_cast<std::uint32_t>(i * (segments + 1) + j), b = a + 1, c = a + static_cast<std::uint32_t>(segments) + 2,
                       d = a + static_cast<std::uint32_t>(segments) + 1;
            // Counterclockwise seen from outside.
            mesh.indices.insert(mesh.indices.end(), {a, c, b, a, d, c});
        }
    }
    for (std::size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[k]];
        const Vertex& b = mesh.vertices[mesh.indices[k + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[k + 2]];
        const Vec3 facing = Cross(b.position - a.position, c.position - a.position);
        if (Length(facing) > 1e-9f && Dot(facing, a.normal) < 0)
            std::swap(mesh.indices[k + 1], mesh.indices[k + 2]);
    }
    return mesh;
}

Mesh BuildCue()
{
    // The radius along the cue: the tip, the ferrule, the shaft tapering out, the joint, the forearm, the wrap and the butt.
    const std::vector<ProfilePoint> outline{{0.0f, 0.0f},       {0.0045f, 0.0f},    {0.0062f, 0.0015f}, {0.0064f, 0.009f},  {0.0066f, 0.009f},
                                            {0.0066f, 0.03f},   {0.0068f, 0.03f},   {0.0082f, 0.3f},    {0.0102f, 0.735f},  {0.0108f, 0.74f},
                                            {0.0108f, 0.765f},  {0.0112f, 0.77f},   {0.0128f, 1.03f},   {0.0132f, 1.04f},   {0.0138f, 1.32f},
                                            {0.0144f, 1.33f},   {0.0148f, 1.44f},   {0.0138f, CueLength}, {0.0f, CueLength}};
    std::vector<ProfilePoint> profile;
    for (const ProfilePoint& p : outline)
        profile.push_back({p.r, -p.y}); // the lathe runs down y; turned onto +z below
    Mesh mesh = Lathe(profile, 28, false);
    // Turned a quarter about x, so that it runs along +z: a rotation, which keeps the winding.
    for (Vertex& v : mesh.vertices) {
        v.position = {v.position.x, v.position.z, -v.position.y};
        v.normal = {v.normal.x, v.normal.z, -v.normal.y};
    }
    return mesh;
}

} // namespace model
