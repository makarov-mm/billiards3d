#include "Themes.h"

namespace {

// The lamp over the table, and a dim room round it.
void Lamp(Theme& t, math::Vec3 radiance, math::Vec3 fill)
{
    t.panels[0] = {.direction = {0.0f, 1.0f, 0.0f}, .halfSize = {0.75f, 0.32f}, .corner = 0.05f, .softness = 0.08f, .radiance = radiance};
    t.panels[1] = {.direction = {0.0f, 1.0f, 0.0f}, .halfSize = {1.4f, 0.8f}, .corner = 0.3f, .softness = 0.6f, .radiance = radiance * 0.08f};
    t.panels[2] = {.direction = {0.9f, 0.25f, 0.35f}, .halfSize = {0.8f, 0.3f}, .corner = 0.1f, .softness = 0.3f, .radiance = fill};
    t.panels[3] = {.direction = {-0.9f, 0.25f, -0.35f}, .halfSize = {0.8f, 0.3f}, .corner = 0.1f, .softness = 0.3f, .radiance = fill * 0.7f};
    t.panels[4] = {.direction = {0.1f, 0.3f, 0.95f}, .halfSize = {1.2f, 0.25f}, .corner = 0.1f, .softness = 0.4f, .radiance = fill * 0.5f};
    t.panelCount = 5;
}

// Green cloth, mahogany, pearl sights, a wooden floor.
Theme Classic()
{
    Theme t{
        .nameEnglish = "Classic",
        .nameRussian = "Классика",
        .cloth = {0.016f, 0.125f, 0.044f},
        .rail = {0.2f, 0.05f, 0.022f},
        .railSeed = 0.37f,
        .frame = {0.12f, 0.03f, 0.014f},
        .pocket = {0.025f, 0.018f, 0.014f},
        .floor = {0.12f, 0.07f, 0.04f},
        .floorRoughness = 0.45f,
        .sight = {0.85f, 0.83f, 0.78f},
        .sightRoughness = 0.15f,
        .keyDirection = {0.1f, 1.0f, 0.22f},
        .keyRadiance = {5.0f, 4.75f, 4.3f},
        .ceiling = {0.01f, 0.01f, 0.01f},
        .horizon = {0.03f, 0.024f, 0.018f},
        .ground = {0.012f, 0.01f, 0.008f},
        .diffuseLight = 0.8f,
        .background = 0.4f,
        .fog = {0.012f, 0.01f, 0.008f},
        .exposure = 0.95f,
        .accent = {0.35f, 0.78f, 1.0f},
    };
    Lamp(t, {9.0f, 8.4f, 7.4f}, {0.12f, 0.1f, 0.08f});
    return t;
}

// Blue cloth, black lacquer and chrome, a grey carpet: a tournament hall.
Theme Tournament()
{
    Theme t{
        .nameEnglish = "Tournament",
        .nameRussian = "Турнир",
        .cloth = {0.012f, 0.065f, 0.23f},
        .clothRoughness = 0.85f,
        .rail = {0.012f, 0.012f, 0.014f},
        .railRoughness = 0.15f,
        .railClearcoat = 1.0f,
        .railPattern = 1,
        .frame = {0.01f, 0.01f, 0.012f},
        .frameRoughness = 0.2f,
        .framePattern = 1,
        .pocket = {0.012f, 0.012f, 0.014f},
        .floor = {0.03f, 0.032f, 0.036f},
        .floorRoughness = 0.95f,
        .floorPattern = 1,
        .sight = {0.8f, 0.82f, 0.85f},
        .sightMetallic = 1,
        .sightRoughness = 0.12f,
        .keyDirection = {0.08f, 1.0f, 0.18f},
        .keyRadiance = {5.2f, 5.2f, 5.3f},
        .keySoftness = 0.2f,
        .ceiling = {0.012f, 0.013f, 0.016f},
        .horizon = {0.03f, 0.032f, 0.04f},
        .ground = {0.01f, 0.01f, 0.012f},
        .diffuseLight = 0.9f,
        .background = 0.4f,
        .fog = {0.01f, 0.011f, 0.014f},
        .exposure = 1.0f,
        .accent = {0.4f, 0.8f, 1.0f},
    };
    Lamp(t, {9.5f, 9.5f, 9.7f}, {0.1f, 0.11f, 0.14f});
    return t;
}

// Wine-red cloth, walnut, brass dots, a stone floor: a club room.
Theme Club()
{
    Theme t{
        .nameEnglish = "Club",
        .nameRussian = "Клуб",
        .cloth = {0.17f, 0.018f, 0.026f},
        .rail = {0.11f, 0.055f, 0.025f},
        .railSeed = 0.71f,
        .frame = {0.075f, 0.036f, 0.017f},
        .frameSeed = 0.2f,
        .pocket = {0.06f, 0.03f, 0.015f},
        .floor = {0.05f, 0.045f, 0.04f},
        .floorRoughness = 0.3f,
        .floorPattern = 2,
        .sight = {0.95f, 0.7f, 0.33f},
        .sightMetallic = 1,
        .sightRoughness = 0.25f,
        .sightShape = 1,
        .keyDirection = {0.12f, 1.0f, 0.25f},
        .keyRadiance = {5.0f, 4.5f, 3.8f},
        .ceiling = {0.012f, 0.009f, 0.007f},
        .horizon = {0.035f, 0.025f, 0.016f},
        .ground = {0.012f, 0.009f, 0.007f},
        .diffuseLight = 0.8f,
        .background = 0.4f,
        .fog = {0.014f, 0.01f, 0.007f},
        .exposure = 1.08f,
        .accent = {1.0f, 0.72f, 0.35f},
    };
    Lamp(t, {9.0f, 8.0f, 6.6f}, {0.14f, 0.1f, 0.07f});
    return t;
}

// Dark teal cloth, glowing sights and a cool, dim room.
Theme Night()
{
    Theme t{
        .nameEnglish = "Night",
        .nameRussian = "Ночь",
        .cloth = {0.006f, 0.06f, 0.07f},
        .clothSheen = 0.5f,
        .rail = {0.02f, 0.02f, 0.025f},
        .railRoughness = 0.2f,
        .railClearcoat = 1.0f,
        .railPattern = 1,
        .frame = {0.012f, 0.012f, 0.016f},
        .framePattern = 1,
        .pocket = {0.01f, 0.01f, 0.012f},
        .floor = {0.012f, 0.014f, 0.02f},
        .floorRoughness = 0.9f,
        .floorPattern = 1,
        .sight = {0.2f, 0.85f, 1.0f},
        .sightRoughness = 0.3f,
        .sightShape = 1,
        .sightGlow = 2.5f,
        .keyDirection = {0.05f, 1.0f, 0.15f},
        .keyRadiance = {3.6f, 3.9f, 4.4f},
        .keySoftness = 0.12f,
        .ceiling = {0.004f, 0.005f, 0.01f},
        .horizon = {0.01f, 0.015f, 0.03f},
        .ground = {0.003f, 0.004f, 0.006f},
        .diffuseLight = 1.0f,
        .reflectedLight = 1.1f,
        .background = 0.5f,
        .fog = {0.004f, 0.006f, 0.012f},
        .exposure = 1.15f,
        .bloom = 0.07f,
        .vignette = 0.55f,
        .accent = {0.2f, 0.85f, 1.0f},
    };
    Lamp(t, {6.8f, 7.4f, 8.4f}, {0.05f, 0.08f, 0.16f});
    t.panels[5] = {.direction = {-0.3f, 0.2f, 0.93f}, .halfSize = {1.0f, 0.03f}, .corner = 0.01f, .softness = 0.02f, .radiance = {1.0f, 3.0f, 4.0f}};
    t.panelCount = 6;
    return t;
}

} // namespace

const std::array<Theme, ThemeCount>& Themes()
{
    static const std::array<Theme, ThemeCount> themes{Classic(), Tournament(), Club(), Night()};
    return themes;
}
