#pragma once

#include "MathTypes.h"
#include "Physics.h"
#include "Renderer.h"

#include <array>
#include <vector>

// How the balls are shown: on the cloth where the simulation has them, dropping into the pockets and lying there
// in a heap, and fading in when they are set back on the table or racked.
class TableView {
public:
    void SetTable(const billiards::TableSpec& table);
    // The balls as they are, without animation: those pocketed lie in their pockets. Racked balls drop in one by one.
    void Reset(const billiards::Simulation& sim, bool rack);
    // Follows the simulation: balls just pocketed start to drop, balls back on the table fade in.
    void Update(const billiards::Simulation& sim, float seconds);
    bool Animating() const;
    // The middle of a ball as drawn.
    math::Vec3 Position(const billiards::Simulation& sim, int ball) const;
    bool Visible(const billiards::Simulation& sim, int ball) const;
    // The balls to draw, each with its glow at the rim.
    void AddBalls(const billiards::Simulation& sim, const std::array<float, billiards::MaxBalls>& highlight, std::vector<render::BallDraw>& balls) const;

private:
    struct Visual {
        int pocket = -1, slot = -1;
        float fall = 10;     // seconds since it dropped
        math::Vec3 from;     // where it dropped
        math::Vec3 velocity; // as it dropped
        float appear = 1;    // fading in, 0 to 1; below 0 waiting to
    };

    // Where the `slot`-th ball lies in a pocket: in layers of a ring at the bottom, the deeper ones first. Past the
    // pocket's room, out of sight below.
    math::Vec3 SlotPosition(int pocket, int slot) const;
    int Capacity(int pocket) const;
    int FreeSlot(int pocket) const;
    void Land(int ball, int pocket);

    const billiards::TableSpec* m_table = nullptr;
    std::array<Visual, billiards::MaxBalls> m_visuals{};
};

math::Mat4 BallMatrix(math::Vec3 position, const billiards::Quat& q, float radius);
