#pragma once

#include "MathTypes.h"
#include "Physics.h"

#include <cstdint>
#include <vector>

// The meshes of the scene: the table (its cloth, cushions, rails, frame, legs and pockets), the floor, the ball
// and the cue. The cloth is the plane y = 0, and the table's measurements come from its TableSpec.
namespace model {

struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct TableMeshes {
    Mesh bed;      // the cloth, reaching under the cushions; the shader cuts the holes of the pockets
    Mesh cushions; // cloth over rubber, ending in the jaws of the pockets
    Mesh rails;    // the wooden rails; the shader cuts the openings of the pockets
    Mesh frame;    // the apron below the rails, and the legs
    float floorHeight = -0.78f;
};

TableMeshes BuildTable(const billiards::TableSpec& table);
// A pocket's cup or net round the y axis: its rim at `top`, its bottom `depth` below the cloth.
Mesh BuildPocket(float radius, float top, float depth);
Mesh BuildFloor(float size, float height);
// A sphere of radius 1; its positions are its normals.
Mesh BuildBall(int rings, int segments);
// The cue from its tip at the origin along +z; uv.y is the distance from the tip.
Mesh BuildCue();
constexpr float CueLength = 1.45f;

} // namespace model
