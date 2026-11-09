#include "Physics.h"

#include <algorithm>
#include <cstddef>
#include <numbers>

namespace billiards {

namespace {

constexpr double Gravity = 9.81;
constexpr double Pi = std::numbers::pi;
constexpr double Step = 0.005; // the longest stretch of motion between looks for contacts

V3 Flat(V2 a) { return {a.x, 0, a.z}; }
V2 Horizontal(V3 a) { return {a.x, a.z}; }

// The rotation by `angle` about the unit `axis`, applied before `q`.
Quat Turn(const Quat& q, V3 axis, double angle)
{
    const double s = std::sin(angle / 2), c = std::cos(angle / 2);
    const Quat d{c, axis.x * s, axis.y * s, axis.z * s};
    Quat r{d.w * q.w - d.x * q.x - d.y * q.y - d.z * q.z, d.w * q.x + d.x * q.w + d.y * q.z - d.z * q.y,
           d.w * q.y - d.x * q.z + d.y * q.w + d.z * q.x, d.w * q.z + d.x * q.y - d.y * q.x + d.z * q.w};
    const double n = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
    r.w /= n;
    r.x /= n;
    r.y /= n;
    r.z /= n;
    return r;
}

struct PocketShape {
    double mouth;     // between the jaws along the cushion lines
    double deviation; // of a jaw's facing from the line of its cushion, toward the outside
    double tipRadius; // the rounding of the jaw
    double facing;    // the length of the facing
    double shelf;     // from the mouth to the edge of the hole
    double hole, cup, depth;
};

// A jaw: its rounded tip on a cushion's line at `tip`, the cushion running toward the pocket along `along`,
// the outside of the table in direction `out`.
void AddJaw(TableSpec& table, V2 tip, V2 along, V2 out, const PocketShape& shape)
{
    const double c = std::cos(shape.deviation), s = std::sin(shape.deviation);
    const V2 center = tip + out * shape.tipRadius;
    const V2 facing = along * c + out * s;
    const V2 throat = along * s - out * c; // the facing's normal, toward the pocket's throat
    const V2 start = center + throat * shape.tipRadius;
    table.tips.push_back({center, shape.tipRadius});
    table.segments.push_back({start, start + facing * shape.facing, throat});
}

} // namespace

TableSpec TableSpec::Make(TableKind kind)
{
    TableSpec t;
    t.kind = kind;
    PocketShape corner{}, middle{};
    if (kind == TableKind::Pool) {
        // A nine-foot pool table with balls of 2¼ inches.
        corner = {0.1143, 38 * Pi / 180, 0.006, 0.08, 0.03, 0.0585, 0.072, 0.115};
        middle = {0.13, 76 * Pi / 180, 0.006, 0.045, 0.012, 0.062, 0.074, 0.115};
    }
    else {
        // A twelve-foot Russian table: bigger balls, rounded jaws and pockets barely wider than a ball.
        t.length = 3.569;
        t.width = 1.778;
        t.ballRadius = 0.034;
        t.ballMass = 0.28;
        t.cushionWidth = 0.055;
        t.railWidth = 0.15;
        t.railHeight = 0.054;
        t.rollFriction = 0.012;
        t.spinDeceleration = 8;
        t.cushionRestitution = 0.8;
        t.cushionFriction = 0.2;
        corner = {0.104, 44 * Pi / 180, 0.012, 0.06, 0.018, 0.05, 0.062, 0.13};
        middle = {0.112, 80 * Pi / 180, 0.012, 0.04, 0.006, 0.052, 0.064, 0.13};
    }
    const double R = t.ballRadius;
    t.cushionHeight = 0.635 * 2 * R;
    const double lift = (t.cushionHeight - R) / R;
    t.contactRadius = R * std::sqrt(1 - lift * lift);

    const double hl = t.length / 2, hw = t.width / 2;
    const double a = corner.mouth / std::numbers::sqrt2;
    // The pockets: the near side (z > 0) from the left, then the far side.
    int index = 0;
    for (const double sz : {1.0, -1.0}) {
        for (const int column : {-1, 0, 1}) {
            Pocket& pocket = t.pockets[static_cast<std::size_t>(index++)];
            if (column != 0) {
                const double sx = column;
                const V2 corner_{sx * hl, sz * hw};
                const V2 onLong = corner_ - V2{sx * a, 0}, onShort = corner_ - V2{0, sz * a};
                AddJaw(t, onLong, {sx, 0}, {0, sz}, corner);
                AddJaw(t, onShort, {0, sz}, {sx, 0}, corner);
                pocket.corner = true;
                pocket.mouth = (onLong + onShort) * 0.5;
                pocket.axis = Normalized({sx, sz});
                pocket.hole = pocket.mouth + pocket.axis * (corner.shelf + corner.hole);
                pocket.holeRadius = corner.hole;
                pocket.cupRadius = corner.cup;
                pocket.depth = corner.depth;
            }
            else {
                const V2 left{-middle.mouth / 2, sz * hw}, right{middle.mouth / 2, sz * hw};
                AddJaw(t, left, {1, 0}, {0, sz}, middle);
                AddJaw(t, right, {-1, 0}, {0, sz}, middle);
                pocket.corner = false;
                pocket.mouth = {0, sz * hw};
                pocket.axis = {0, sz};
                pocket.hole = pocket.mouth + pocket.axis * (middle.shelf + middle.hole);
                pocket.holeRadius = middle.hole;
                pocket.cupRadius = middle.cup;
                pocket.depth = middle.depth;
            }
        }
    }
    // The noses of the cushions between the jaws.
    for (const double sz : {1.0, -1.0}) {
        const V2 normal{0, -sz};
        t.segments.push_back({{-hl + a, sz * hw}, {-middle.mouth / 2, sz * hw}, normal});
        t.segments.push_back({{middle.mouth / 2, sz * hw}, {hl - a, sz * hw}, normal});
    }
    for (const double sx : {1.0, -1.0})
        t.segments.push_back({{sx * hl, -hw + a}, {sx * hl, hw - a}, {-sx, 0}});
    return t;
}

// The motion -------------------------------------------------------------------------------------

bool Simulation::BallMoving(const Ball& ball) const
{
    if (!ball.OnTable())
        return false;
    const double R = m_table->ballRadius;
    const V2 slip{ball.v.x + R * ball.w.z, ball.v.z - R * ball.w.x};
    return Dot(ball.v, ball.v) > 1e-18 || Dot(slip, slip) > 1e-12;
}

bool Simulation::Moving() const
{
    for (int i = 0; i < count; ++i)
        if (BallMoving(balls[static_cast<std::size_t>(i)]))
            return true;
    return false;
}

// Sliding, the friction of the cloth turns the slip of the contact point into rolling; rolling, the ball slows
// gently to a stop. Each stage has constant accelerations, so it is followed exactly.
void Simulation::Move(Ball& ball, double t) const
{
    const TableSpec& s = *m_table;
    const double R = s.ballRadius;
    while (t > 1e-12) {
        const V2 slip{ball.v.x + R * ball.w.z, ball.v.z - R * ball.w.x};
        const double slipSpeed = Length(slip);
        const double speed = Length(ball.v);
        const V3 before = ball.w;
        double h = t;
        bool still = false;
        if (slipSpeed > 1e-6) {
            const V2 direction = slip * (1 / slipSpeed);
            const double toRoll = slipSpeed / (3.5 * s.slideFriction * Gravity);
            h = std::min(t, toRoll);
            const V2 acceleration = direction * (-s.slideFriction * Gravity);
            const double angular = 2.5 * s.slideFriction * Gravity / R;
            ball.p = ball.p + ball.v * h + acceleration * (0.5 * h * h);
            ball.v = ball.v + acceleration * h;
            ball.w.x += angular * direction.z * h;
            ball.w.z -= angular * direction.x * h;
            if (h >= toRoll) {
                ball.w.x = ball.v.z / R;
                ball.w.z = -ball.v.x / R;
            }
        }
        else if (speed > 1e-9) {
            const V2 direction = ball.v * (1 / speed);
            const double toStop = speed / (s.rollFriction * Gravity);
            h = std::min(t, toStop);
            const V2 acceleration = direction * (-s.rollFriction * Gravity);
            ball.p = ball.p + ball.v * h + acceleration * (0.5 * h * h);
            ball.v = h >= toStop ? V2{} : ball.v + acceleration * h;
            ball.w.x = ball.v.z / R;
            ball.w.z = -ball.v.x / R;
        }
        else {
            ball.v = {};
            ball.w.x = ball.w.z = 0;
            still = true;
        }
        // Spin about the vertical wears away on its own.
        const double wear = s.spinDeceleration * h;
        ball.w.y = std::abs(ball.w.y) <= wear ? 0 : ball.w.y - std::copysign(wear, ball.w.y);
        if (turnBalls) {
            const V3 mean = (before + ball.w) * 0.5;
            const double rate = Length(mean);
            if (rate > 1e-9)
                ball.q = Turn(ball.q, mean * (1 / rate), rate * h);
        }
        t -= h;
        if (still)
            break;
    }
}

void Simulation::MoveAll(double t)
{
    for (int i = 0; i < count; ++i) {
        Ball& ball = balls[static_cast<std::size_t>(i)];
        if (ball.OnTable())
            Move(ball, t);
    }
}

// The first contact within `horizon`, each ball taken to move in a straight line meanwhile: over a few
// milliseconds the curve of its slowing is far smaller than anything that matters.
Simulation::Contact Simulation::FindContact(double horizon) const
{
    const TableSpec& s = *m_table;
    const double R = s.ballRadius, reach = s.contactRadius;
    Contact best;
    best.t = horizon;
    auto consider = [&](double t, int kind, int a, int b) {
        if (t >= 0 && t < best.t) {
            best.t = t;
            best.kind = kind;
            best.a = a;
            best.b = b;
        }
    };
    // The time a point moving from `d` (relative) with `w` first comes within `radius`, if it is approaching.
    auto meet = [](V2 d, V2 w, double radius) {
        const double b = Dot(d, w);
        if (b >= 0)
            return -1.0;
        const double c = Dot(d, d) - radius * radius;
        if (c <= 0)
            return 0.0;
        const double a = Dot(w, w);
        const double disc = b * b - a * c;
        if (disc < 0)
            return -1.0;
        return (-b - std::sqrt(disc)) / a;
    };

    for (int i = 0; i < count; ++i) {
        const Ball& ball = balls[static_cast<std::size_t>(i)];
        if (!BallMoving(ball))
            continue;
        for (int j = 0; j < count; ++j) {
            const Ball& other = balls[static_cast<std::size_t>(j)];
            if (j == i || !other.OnTable() || (j < i && BallMoving(other)))
                continue;
            consider(meet(other.p - ball.p, other.v - ball.v, 2 * R), 0, i, j);
        }
        for (std::size_t k = 0; k < s.segments.size(); ++k) {
            const Segment& segment = s.segments[k];
            const double approach = Dot(ball.v, segment.normal);
            if (approach >= 0)
                continue;
            const double distance = Dot(ball.p - segment.a, segment.normal);
            if (distance < reach * 0.5)
                continue; // behind it, or long past
            const double t = std::max(0.0, (reach - distance) / approach);
            if (t >= best.t)
                continue;
            const V2 along = segment.b - segment.a;
            const double position = Dot(ball.p + ball.v * t - segment.a, along) / Dot(along, along);
            if (position >= 0 && position <= 1)
                consider(t, 1, i, static_cast<int>(k));
        }
        for (std::size_t k = 0; k < s.tips.size(); ++k)
            consider(meet(s.tips[k].center - ball.p, -ball.v, reach + s.tips[k].radius), 2, i, static_cast<int>(k));
        for (std::size_t k = 0; k < s.pockets.size(); ++k) {
            const Pocket& pocket = s.pockets[k];
            consider(meet(pocket.hole - ball.p, -ball.v, pocket.holeRadius), 3, i, static_cast<int>(k));
        }
    }
    return best;
}

// Two balls meet: the push along the line of their middles, and the friction between their surfaces, which
// throws the one struck off that line and passes on some spin.
void Simulation::Collide(int i, int j, std::vector<Event>* events)
{
    const TableSpec& s = *m_table;
    const double R = s.ballRadius;
    Ball& a = balls[static_cast<std::size_t>(i)];
    Ball& b = balls[static_cast<std::size_t>(j)];
    const V2 n = Normalized(b.p - a.p);
    const double approach = Dot(a.v - b.v, n);
    if (approach <= 0)
        return;
    // Exactly touching, should rounding have let them overlap.
    const double overlap = 2 * R - Length(b.p - a.p);
    if (overlap > 0) {
        a.p = a.p - n * (overlap / 2);
        b.p = b.p + n * (overlap / 2);
    }
    const V3 n3 = Flat(n);
    const V3 slip = (Flat(a.v) + Cross(a.w, n3 * R)) - (Flat(b.v) + Cross(b.w, n3 * -R));
    const V3 tangential = slip - n3 * Dot(slip, n3);
    const double impulse = (1 + s.ballRestitution) / 2 * approach;
    a.v = a.v - n * impulse;
    b.v = b.v + n * impulse;
    const double slipSpeed = Length(tangential);
    if (slipSpeed > 1e-9) {
        const double friction = std::min(s.ballFriction * impulse, slipSpeed / 7);
        const V3 along = tangential * (friction / slipSpeed);
        a.v = a.v - Horizontal(along);
        b.v = b.v + Horizontal(along);
        const double turn = 2.5 / (R * R);
        a.w = a.w + Cross(n3 * R, along * -1.0) * turn;
        b.w = b.w + Cross(n3 * -R, along) * turn;
    }
    if (events)
        events->push_back({EventType::BallBall, i, j, approach, time});
}

// A ball meets a cushion's nose a little above its middle: the cushion pushes it back toward the cloth, and its
// friction acts on the ball's slip there, so that spin changes the angle it comes off at.
void Simulation::Bounce(int i, V2 normal, std::vector<Event>* events)
{
    const TableSpec& s = *m_table;
    const double R = s.ballRadius;
    Ball& ball = balls[static_cast<std::size_t>(i)];
    const double approach = -Dot(ball.v, normal);
    if (approach <= 0)
        return;
    const double lift = (s.cushionHeight - R) / R, level = std::sqrt(1 - lift * lift);
    const V3 n3 = Flat(normal);
    const V3 toContact = n3 * (-R * level) + V3{0, R * lift, 0};
    const V3 inward = n3 * level + V3{0, -lift, 0}; // from the cushion into the ball
    const double restitution = std::clamp(s.cushionRestitution - 0.025 * approach, 0.62, s.cushionRestitution);
    const double push = (1 + restitution) * approach / level;
    const V3 slip = Flat(ball.v) + Cross(ball.w, toContact);
    const V3 tangential = slip - inward * Dot(slip, inward);
    V3 impulse = inward * push;
    const double slipSpeed = Length(tangential);
    if (slipSpeed > 1e-9) {
        const double friction = std::min(s.cushionFriction * push, slipSpeed * 2 / 7);
        const V3 along = tangential * (-friction / slipSpeed);
        impulse = impulse + along;
        ball.w = ball.w + Cross(toContact, along) * (2.5 / (R * R));
    }
    ball.v = ball.v + Horizontal(impulse);
    // Never back into the cushion, whatever the friction did.
    const double away = Dot(ball.v, normal);
    if (away < 0)
        ball.v = ball.v - normal * away;
    if (events)
        events->push_back({EventType::Cushion, i, -1, approach, time});
}

void Simulation::Drop(int i, int pocket, std::vector<Event>* events)
{
    Ball& ball = balls[static_cast<std::size_t>(i)];
    ball.state = BallState::Pocketed;
    ball.pocket = pocket;
    if (events)
        events->push_back({EventType::Pocket, i, pocket, Length(ball.v), time});
}

void Simulation::Advance(double seconds, std::vector<Event>* events)
{
    double remaining = seconds;
    int guard = 0;
    while (remaining > 1e-12 && Moving()) {
        double h = std::min(remaining, Step);
        while (h > 1e-12) {
            const Contact contact = FindContact(h);
            const double t = contact.kind < 0 ? h : contact.t;
            MoveAll(t);
            time += t;
            remaining -= t;
            h -= t;
            if (contact.kind == 0)
                Collide(contact.a, contact.b, events);
            else if (contact.kind == 1)
                Bounce(contact.a, m_table->segments[static_cast<std::size_t>(contact.b)].normal, events);
            else if (contact.kind == 2)
                Bounce(contact.a, Normalized(balls[static_cast<std::size_t>(contact.a)].p - m_table->tips[static_cast<std::size_t>(contact.b)].center), events);
            else if (contact.kind == 3)
                Drop(contact.a, contact.b, events);
            // A pile of contacts at one instant is worked through one by one; a runaway is cut short.
            if (contact.kind >= 0 && ++guard > 2000) {
                MoveAll(h);
                time += h;
                remaining -= h;
                h = 0;
            }
        }
    }
    if (!Moving())
        for (int i = 0; i < count; ++i)
            if (balls[static_cast<std::size_t>(i)].OnTable())
                balls[static_cast<std::size_t>(i)].w = {};
}

void Simulation::Run(std::vector<Event>* events, double limit)
{
    const double end = time + limit;
    while (Moving() && time < end)
        Advance(0.5, events);
}

void Simulation::Strike(int index, V2 direction, double speed, double side, double vertical)
{
    Ball& ball = balls[static_cast<std::size_t>(index)];
    const double R = m_table->ballRadius;
    const V2 d = Normalized(direction);
    const V3 d3 = Flat(d);
    const V3 right = Cross(d3, {0, 1, 0});
    ball.v = d * speed;
    // The cue's push along d at the point struck, ω = r × J / I with I = 2/5 m R²: above the middle, topspin;
    // to the right of it, spin to the right.
    const V3 struck = right * (side * R) + V3{0, vertical * R, 0};
    ball.w = Cross(struck, d3) * (2.5 * speed / (R * R));
}

bool Simulation::OnCloth(V2 p) const
{
    const TableSpec& s = *m_table;
    const double R = s.ballRadius;
    if (std::abs(p.x) > s.length / 2 - R || std::abs(p.z) > s.width / 2 - R)
        return false;
    for (const Pocket& pocket : s.pockets)
        if (Length(p - pocket.hole) < pocket.holeRadius + R)
            return false;
    return true;
}

bool Simulation::FreeSpot(V2 p, int except) const
{
    if (!OnCloth(p))
        return false;
    const double R = m_table->ballRadius;
    for (int i = 0; i < count; ++i) {
        const Ball& ball = balls[static_cast<std::size_t>(i)];
        if (i != except && ball.OnTable() && Length(ball.p - p) < 2 * R + 1e-4)
            return false;
    }
    return true;
}

} // namespace billiards
