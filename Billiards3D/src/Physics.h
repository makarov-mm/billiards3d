#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// The physics of the balls on the table: sliding and rolling on the cloth with spin, collisions between the
// balls with throw, cushions struck above the ball's middle, jaws, and pockets. Distances are in metres, the
// table is the plane y = 0 with its long side along x, and a ball's position is its middle's (x, z).
namespace billiards {

struct V2 {
    double x = 0, z = 0;
};

constexpr V2 operator+(V2 a, V2 b) { return {a.x + b.x, a.z + b.z}; }
constexpr V2 operator-(V2 a, V2 b) { return {a.x - b.x, a.z - b.z}; }
constexpr V2 operator-(V2 a) { return {-a.x, -a.z}; }
constexpr V2 operator*(V2 a, double s) { return {a.x * s, a.z * s}; }
constexpr V2 operator*(double s, V2 a) { return a * s; }
constexpr double Dot(V2 a, V2 b) { return a.x * b.x + a.z * b.z; }
constexpr double Cross(V2 a, V2 b) { return a.x * b.z - a.z * b.x; }
inline double Length(V2 a) { return std::sqrt(Dot(a, a)); }
inline V2 Normalized(V2 a)
{
    const double length = Length(a);
    return length > 1e-12 ? a * (1 / length) : V2{1, 0};
}
// Turned a quarter to the left, as seen from above with x to the right and z toward the viewer.
constexpr V2 Perpendicular(V2 a) { return {a.z, -a.x}; }

struct V3 {
    double x = 0, y = 0, z = 0;
};

constexpr V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
constexpr double Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline double Length(V3 a) { return std::sqrt(Dot(a, a)); }

// A rotation, kept for drawing the balls turning.
struct Quat {
    double w = 1, x = 0, y = 0, z = 0;
};

enum class TableKind : std::uint8_t { Pool, Pyramid };

struct Pocket {
    V2 mouth;           // the middle of the mouth, between the jaws
    V2 axis;            // outward, into the pocket
    V2 hole;            // where the balls drop: they fall once their middles are over it
    double holeRadius = 0;
    double cupRadius = 0; // of the opening in the rails, for drawing
    double depth = 0;     // of the pocket below the cloth, for drawing
    bool corner = true;
};

// A straight piece of cushion: a ball's middle keeps the contact radius away from it. The normal points into
// the table.
struct Segment {
    V2 a, b;
    V2 normal;
};

// A rounded jaw, where a cushion turns into a pocket.
struct Tip {
    V2 center;
    double radius = 0;
};

struct TableSpec {
    TableKind kind = TableKind::Pool;
    double length = 2.54, width = 1.27; // between the noses of the cushions
    double ballRadius = 0.028575;
    double ballMass = 0.17;
    double cushionHeight = 0.0363; // the nose above the cloth
    double contactRadius = 0;      // of the ball where it meets a cushion's nose
    // For drawing: the cushions from their noses to the rails, the rails, and the frame below.
    double cushionWidth = 0.05, railWidth = 0.13, railHeight = 0.045;

    // The cloth and the balls.
    double slideFriction = 0.2;
    double rollFriction = 0.01;
    double spinDeceleration = 10; // of spin about the vertical, rad/s²
    double ballRestitution = 0.95;
    double ballFriction = 0.06;
    double cushionRestitution = 0.85;
    double cushionFriction = 0.18;

    std::array<Pocket, 6> pockets{}; // the corners, then the middles
    std::vector<Segment> segments;
    std::vector<Tip> tips;

    double HeadString() const { return -length / 4; }
    V2 FootSpot() const { return {length / 4, 0}; }

    static TableSpec Make(TableKind kind);
};

enum class BallState : std::uint8_t { OnTable, Pocketed };

struct Ball {
    V2 p, v;
    V3 w; // angular velocity
    Quat q;
    BallState state = BallState::OnTable;
    int pocket = -1;

    bool OnTable() const { return state == BallState::OnTable; }
};

enum class EventType : std::uint8_t { BallBall, Cushion, Pocket };

struct Event {
    EventType type = EventType::BallBall;
    int a = -1, b = -1; // the balls; for a cushion, b is -1; for a pocket, b is the pocket
    double speed = 0;   // of the impact
    double time = 0;
};

constexpr int MaxBalls = 16;

class Simulation {
public:
    Simulation() = default;
    explicit Simulation(const TableSpec& table) : m_table(&table) {}

    const TableSpec& Table() const { return *m_table; }

    // A cue strikes `ball` along `direction` at `speed` (of the ball, m/s), `side` to the right of the middle and
    // `vertical` above it, both as fractions of the radius.
    void Strike(int ball, V2 direction, double speed, double side, double vertical);
    // Moves the balls on by `seconds`, noting what happens.
    void Advance(double seconds, std::vector<Event>* events);
    // Until every ball lies still, or the limit.
    void Run(std::vector<Event>* events, double limit = 40);
    bool Moving() const;
    // A ball placed at `p` would touch no other and stay on the cloth.
    bool FreeSpot(V2 p, int except) const;
    // Inside the cushions, as far as a ball can go.
    bool OnCloth(V2 p) const;

    std::array<Ball, MaxBalls> balls{};
    int count = 0;
    double time = 0;
    bool turnBalls = true; // keep the orientations up to date, for drawing

private:
    struct Contact {
        double t = 1e30;
        int kind = -1; // 0 ball, 1 segment, 2 tip, 3 pocket
        int a = -1, b = -1;
    };

    bool BallMoving(const Ball& ball) const;
    void Move(Ball& ball, double t) const;
    void MoveAll(double t);
    Contact FindContact(double horizon) const;
    void Collide(int i, int j, std::vector<Event>* events);
    void Bounce(int i, V2 normal, std::vector<Event>* events);
    void Drop(int i, int pocket, std::vector<Event>* events);

    const TableSpec* m_table = nullptr;
};

} // namespace billiards
