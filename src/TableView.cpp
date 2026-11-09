#include "TableView.h"

#include <algorithm>
#include <cmath>

using namespace math;
using billiards::Simulation;

namespace {

constexpr float Gravity = 9.81f;
constexpr float FallTime = 0.55f;
constexpr float AppearTime = 0.35f;

struct Ring {
    int count;
    float radius;
    float rise; // from one layer to the next
};

// How the balls lie in a cup of the given radius: as many round a ring as fit, the next layer in the gaps.
Ring RingFor(float cup, float R)
{
    const float radius = std::max(0.0f, cup - R - 0.002f);
    if (radius < R * 1.02f) {
        // One at a time, leaning to alternate sides.
        const float lean = std::min(radius, 0.8f * R);
        return {1, lean, std::sqrt(std::max(0.0f, 4 * R * R - 4 * lean * lean))};
    }
    const int count = std::clamp(static_cast<int>(Pi / std::asin(std::min(1.0f, R / radius))), 1, 5);
    const float gap = 2 * radius * std::sin(Pi / static_cast<float>(2 * count));
    return {count, radius, std::sqrt(std::max(0.0f, 4 * R * R - gap * gap))};
}

} // namespace

Mat4 BallMatrix(Vec3 position, const billiards::Quat& q, float radius)
{
    const float w = static_cast<float>(q.w), x = static_cast<float>(q.x), y = static_cast<float>(q.y), z = static_cast<float>(q.z);
    Mat4 m = Mat4::Identity();
    m(0, 0) = (1 - 2 * (y * y + z * z)) * radius;
    m(0, 1) = 2 * (x * y - w * z) * radius;
    m(0, 2) = 2 * (x * z + w * y) * radius;
    m(1, 0) = 2 * (x * y + w * z) * radius;
    m(1, 1) = (1 - 2 * (x * x + z * z)) * radius;
    m(1, 2) = 2 * (y * z - w * x) * radius;
    m(2, 0) = 2 * (x * z - w * y) * radius;
    m(2, 1) = 2 * (y * z + w * x) * radius;
    m(2, 2) = (1 - 2 * (x * x + y * y)) * radius;
    m(0, 3) = position.x;
    m(1, 3) = position.y;
    m(2, 3) = position.z;
    return m;
}

void TableView::SetTable(const billiards::TableSpec& table)
{
    m_table = &table;
    m_visuals = {};
}

int TableView::Capacity(int pocket) const
{
    const billiards::Pocket& p = m_table->pockets[static_cast<std::size_t>(pocket)];
    const float R = static_cast<float>(m_table->ballRadius);
    const Ring ring = RingFor(static_cast<float>(p.cupRadius), R);
    // Layers whose balls stay below the cloth.
    const float bottom = -static_cast<float>(p.depth) + R;
    int layers = 0;
    while (bottom + static_cast<float>(layers) * ring.rise + R < -0.004f && layers < 6)
        ++layers;
    return std::max(1, layers) * ring.count;
}

Vec3 TableView::SlotPosition(int pocket, int slot) const
{
    const billiards::Pocket& p = m_table->pockets[static_cast<std::size_t>(pocket)];
    const float R = static_cast<float>(m_table->ballRadius);
    const Vec3 hole{static_cast<float>(p.hole.x), 0, static_cast<float>(p.hole.z)};
    const float bottom = -static_cast<float>(p.depth) + R;
    if (slot >= Capacity(pocket))
        return hole + Vec3{0, bottom - 3 * R, 0}; // gone deeper, out of sight
    const Ring ring = RingFor(static_cast<float>(p.cupRadius), R);
    const int layer = slot / ring.count, index = slot % ring.count;
    // The ring turned half a gap on each layer, and toward the table's middle at the start, so the first ball is
    // the one best seen.
    const float start = std::atan2(-static_cast<float>(p.axis.z), -static_cast<float>(p.axis.x));
    const float angle = start + (static_cast<float>(index) + 0.5f * static_cast<float>(layer % 2)) * 2 * Pi / static_cast<float>(ring.count) +
                        (ring.count == 1 ? Pi * static_cast<float>(layer % 2) : 0.0f);
    return hole + Vec3{std::cos(angle) * ring.radius, bottom + static_cast<float>(layer) * ring.rise, std::sin(angle) * ring.radius};
}

int TableView::FreeSlot(int pocket) const
{
    for (int slot = 0;; ++slot) {
        bool taken = false;
        for (const Visual& v : m_visuals)
            taken |= v.pocket == pocket && v.slot == slot;
        if (!taken)
            return slot;
    }
}

void TableView::Land(int ball, int pocket)
{
    Visual& v = m_visuals[static_cast<std::size_t>(ball)];
    v.pocket = pocket;
    v.slot = FreeSlot(pocket);
}

void TableView::Reset(const Simulation& sim, bool rack)
{
    m_visuals = {};
    for (int i = 0; i < sim.count; ++i) {
        const billiards::Ball& ball = sim.balls[static_cast<std::size_t>(i)];
        Visual& v = m_visuals[static_cast<std::size_t>(i)];
        if (!ball.OnTable() && ball.pocket >= 0)
            Land(i, ball.pocket);
        if (rack)
            v.appear = -0.035f * static_cast<float>(i);
    }
}

void TableView::Update(const Simulation& sim, float seconds)
{
    for (int i = 0; i < sim.count; ++i) {
        const billiards::Ball& ball = sim.balls[static_cast<std::size_t>(i)];
        Visual& v = m_visuals[static_cast<std::size_t>(i)];
        if (ball.OnTable() && v.pocket >= 0) {
            // Back on the table.
            v.pocket = v.slot = -1;
            v.appear = 0;
        }
        else if (!ball.OnTable() && v.pocket < 0 && ball.pocket >= 0) {
            const float R = static_cast<float>(m_table->ballRadius);
            Land(i, ball.pocket);
            v.fall = 0;
            v.from = {static_cast<float>(ball.p.x), R, static_cast<float>(ball.p.z)};
            v.velocity = {static_cast<float>(ball.v.x), 0, static_cast<float>(ball.v.z)};
        }
        v.fall += seconds;
        v.appear = std::min(1.0f, v.appear + seconds / AppearTime);
    }
}

bool TableView::Animating() const
{
    return std::ranges::any_of(m_visuals, [](const Visual& v) { return (v.pocket >= 0 && v.fall < FallTime) || v.appear < 1; });
}

Vec3 TableView::Position(const Simulation& sim, int ball) const
{
    const billiards::Ball& b = sim.balls[static_cast<std::size_t>(ball)];
    const Visual& v = m_visuals[static_cast<std::size_t>(ball)];
    const float R = static_cast<float>(m_table->ballRadius);
    if (b.OnTable() || v.pocket < 0) {
        // Set down from a little above as it fades in.
        const float settle = 1 - SmoothStep(0, 1, std::max(v.appear, 0.0f));
        return {static_cast<float>(b.p.x), R + settle * 1.5f * R, static_cast<float>(b.p.z)};
    }
    const Vec3 slot = SlotPosition(v.pocket, v.slot);
    if (v.fall >= FallTime)
        return slot;
    // It drops over the hole, carried on a little by its speed, and comes to rest in its place.
    const float t = v.fall;
    const float s = SmoothStep(0, 1, t / (FallTime * 0.75f));
    const billiards::Pocket& pocket = m_table->pockets[static_cast<std::size_t>(v.pocket)];
    Vec3 carried = v.from + v.velocity * std::min(t, 0.05f);
    const Vec3 hole{static_cast<float>(pocket.hole.x), 0, static_cast<float>(pocket.hole.z)};
    Vec3 offset = carried - hole;
    offset.y = 0;
    const float room = static_cast<float>(pocket.cupRadius) - R;
    if (Length(offset) > room)
        carried = hole + offset * (room / Length(offset));
    Vec3 p = Lerp(carried, slot, s);
    const float drop = R - 0.5f * Gravity * t * t;
    const float landing = std::sqrt(2 * std::max(0.0f, R - slot.y) / Gravity);
    if (t < landing) {
        p.y = std::max(slot.y, drop);
    }
    else {
        const float after = t - landing;
        p.y = slot.y + 0.25f * R * std::abs(std::sin(after * 26.0f)) * std::exp(-after * 14.0f);
    }
    return p;
}

bool TableView::Visible(const Simulation& sim, int ball) const
{
    const billiards::Ball& b = sim.balls[static_cast<std::size_t>(ball)];
    const Visual& v = m_visuals[static_cast<std::size_t>(ball)];
    if (b.OnTable())
        return v.appear > 0;
    return v.pocket >= 0 && (v.slot < Capacity(v.pocket) || v.fall < FallTime);
}

void TableView::AddBalls(const Simulation& sim, const std::array<float, billiards::MaxBalls>& highlight, std::vector<render::BallDraw>& balls) const
{
    const float R = static_cast<float>(m_table->ballRadius);
    const bool russian = m_table->kind == billiards::TableKind::Pyramid;
    for (int i = 0; i < sim.count; ++i) {
        if (!Visible(sim, i))
            continue;
        const billiards::Ball& b = sim.balls[static_cast<std::size_t>(i)];
        const Visual& v = m_visuals[static_cast<std::size_t>(i)];
        const Vec3 p = Position(sim, i);
        render::BallDraw draw;
        draw.model = BallMatrix(p, b.q, R);
        draw.number = i;
        draw.russian = russian;
        draw.opacity = b.OnTable() ? SmoothStep(0, 1, std::max(v.appear, 0.0f)) : 1.0f;
        draw.highlight = highlight[static_cast<std::size_t>(i)];
        // What it stands on: the cloth, or the bottom of the pocket or the balls below it.
        draw.ground = b.OnTable() ? 0.0f : std::min(p.y - R, 0.0f);
        if (draw.opacity > 0.01f)
            balls.push_back(draw);
    }
}
