#pragma once

#include "MathTypes.h"

#include <array>
#include <string_view>

// The looks of the room and the table: the cloth, the wood, the pockets, the floor, and the lamp over the table.
struct StudioPanel {
    math::Vec3 direction; // toward the panel
    float roll = 0;
    math::Vec2 halfSize;  // as tangents of the angles
    float corner = 0.05f;
    float softness = 0.05f;
    math::Vec3 radiance;
    float falloff = 0.3f;
};

struct Theme {
    std::string_view nameEnglish;
    std::string_view nameRussian;

    math::Vec3 cloth;
    float clothRoughness = 0.9f;
    float clothNap = 1;
    float clothSheen = 0.3f;
    float clothWear = 1;
    math::Vec3 rail;
    float railRoughness = 0.3f;
    float railClearcoat = 0.8f;
    float railPattern = 0; // 0 wood, 1 lacquer
    float railSeed = 0.3f;
    math::Vec3 frame;
    float frameRoughness = 0.35f;
    float frameClearcoat = 0.6f;
    float framePattern = 0;
    float frameSeed = 0.6f;
    math::Vec3 pocket;
    float pocketRoughness = 0.6f;
    math::Vec3 floor;
    float floorRoughness = 0.5f;
    float floorPattern = 0; // 0 boards, 1 carpet, 2 stone
    math::Vec3 sight;
    float sightMetallic = 0;
    float sightRoughness = 0.2f;
    float sightShape = 0; // 0 diamonds, 1 dots
    float sightGlow = 0;
    float ballRoughness = 0.06f;
    float ballCoatRoughness = 0.02f;

    // The light.
    math::Vec3 keyDirection;
    math::Vec3 keyRadiance;
    float keySoftness = 0.15f; // tangent of the light's angular radius
    math::Vec3 ceiling, horizon, ground;
    std::array<StudioPanel, 6> panels{};
    int panelCount = 0;
    float diffuseLight = 1;
    float reflectedLight = 1;
    float background = 0.5f;
    math::Vec3 fog;
    float exposure = 1;
    float bloom = 0.04f;
    float vignette = 0.45f;

    math::Vec3 accent; // highlights, the aim and the interface
};

constexpr int ThemeCount = 4;

const std::array<Theme, ThemeCount>& Themes();
