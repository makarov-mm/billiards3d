#include "Prediction.h"

#include <cmath>
#include <cstddef>

namespace billiards {

namespace {

constexpr double Step = 0.005;

// Leaves out the points that lie within `tolerance` of the line through their neighbours.
std::vector<V2> Simplify(const std::vector<V2>& points, double tolerance)
{
    if (points.size() < 3)
        return points;
    std::vector<V2> result{points.front()};
    std::size_t anchor = 0;
    for (std::size_t i = 2; i < points.size(); ++i) {
        // Does the chord from the anchor to i pass near every point between?
        const V2 a = points[anchor], b = points[i];
        const V2 ab = b - a;
        const double length = Length(ab);
        bool straight = true;
        for (std::size_t k = anchor + 1; k < i && straight; ++k) {
            const double off = length > 1e-9 ? std::abs(Cross(ab, points[k] - a)) / length : Length(points[k] - a);
            straight = off < tolerance;
        }
        if (!straight) {
            result.push_back(points[i - 1]);
            anchor = i - 1;
        }
    }
    result.push_back(points.back());
    return result;
}

double PathLength(const std::vector<V2>& points)
{
    double length = 0;
    for (std::size_t i = 1; i < points.size(); ++i)
        length += Length(points[i] - points[i - 1]);
    return length;
}

// Cuts a path off once it is `reach` long.
void Clip(std::vector<V2>& points, double reach)
{
    double length = 0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const double segment = Length(points[i] - points[i - 1]);
        if (length + segment >= reach) {
            const double t = segment > 1e-12 ? (reach - length) / segment : 0;
            points[i] = points[i - 1] + (points[i] - points[i - 1]) * t;
            points.resize(i + 1);
            return;
        }
        length += segment;
    }
}

// Where a ball was at an event in the step just taken, from where it is now and how it moves since.
V2 BackTo(const Ball& ball, double now, double time)
{
    return ball.OnTable() ? ball.p - ball.v * (now - time) : ball.p;
}

} // namespace

Prediction Predict(const Simulation& position, int struck, V2 direction, double speed, double side, double vertical, double reachAfter, double reachObject)
{
    Prediction prediction;
    Simulation sim = position;
    sim.turnBalls = false;
    const auto ball = [&](int i) -> const Ball& { return sim.balls[static_cast<std::size_t>(i)]; };
    prediction.path.push_back(ball(struck).p);
    sim.Strike(struck, direction, speed, side, vertical);

    std::vector<Event> events;
    // Up to the first ball met.
    while (prediction.hit < 0 && prediction.struckPocket < 0 && sim.Moving() && sim.time < 20) {
        const std::size_t first = events.size();
        sim.Advance(Step, &events);
        for (std::size_t k = first; k < events.size() && prediction.hit < 0; ++k) {
            const Event& event = events[k];
            if (event.a != struck && !(event.type == EventType::BallBall && event.b == struck))
                continue;
            if (event.type == EventType::BallBall) {
                prediction.hit = event.a == struck ? event.b : event.a;
                prediction.contact = BackTo(ball(struck), sim.time, event.time);
                prediction.path.push_back(prediction.contact);
                prediction.object.push_back(BackTo(ball(prediction.hit), sim.time, event.time));
                prediction.after.push_back(prediction.contact);
            }
            else if (event.type == EventType::Cushion) {
                prediction.path.push_back(BackTo(ball(struck), sim.time, event.time));
            }
            else {
                prediction.struckPocket = event.b;
                prediction.path.push_back(ball(struck).p);
                break;
            }
        }
        if (prediction.hit < 0 && prediction.struckPocket < 0)
            prediction.path.push_back(ball(struck).p);
    }

    if (prediction.hit >= 0) {
        // Both balls on from the contact: the struck one until its next contact, the other until a cushion, a ball
        // or a pocket.
        const int hit = prediction.hit;
        bool afterDone = false, objectDone = false;
        const double end = sim.time + 6;
        while (!(afterDone && objectDone) && sim.Moving() && sim.time < end) {
            const std::size_t first = events.size();
            sim.Advance(Step, &events);
            for (std::size_t k = first; k < events.size(); ++k) {
                const Event& event = events[k];
                const bool struckEvent = event.a == struck || (event.type == EventType::BallBall && event.b == struck);
                const bool hitEvent = event.a == hit || (event.type == EventType::BallBall && event.b == hit);
                if (struckEvent && !afterDone) {
                    prediction.after.push_back(event.type == EventType::Pocket ? ball(struck).p : BackTo(ball(struck), sim.time, event.time));
                    afterDone = true;
                }
                if (hitEvent && !objectDone) {
                    prediction.object.push_back(event.type == EventType::Pocket ? ball(hit).p : BackTo(ball(hit), sim.time, event.time));
                    if (event.type == EventType::Pocket)
                        prediction.objectPocket = event.b;
                    objectDone = true;
                }
            }
            if (!afterDone) {
                prediction.after.push_back(ball(struck).p);
                afterDone = PathLength(prediction.after) > reachAfter || !ball(struck).OnTable();
            }
            if (!objectDone)
                prediction.object.push_back(ball(hit).p);
        }
    }

    prediction.path = Simplify(prediction.path, 0.0015);
    prediction.after = Simplify(prediction.after, 0.0015);
    Clip(prediction.after, reachAfter);
    prediction.object = Simplify(prediction.object, 0.0015);
    Clip(prediction.object, reachObject);
    return prediction;
}

int PocketAlong(const TableSpec& table, V2 from, V2 direction)
{
    const V2 d = Normalized(direction);
    int best = -1;
    double bestScore = 1e30;
    for (int i = 0; i < 6; ++i) {
        const V2 to = table.pockets[static_cast<std::size_t>(i)].mouth - from;
        const double along = Dot(to, d);
        if (along <= 0)
            continue;
        const double score = std::abs(Cross(d, to)) / along;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

} // namespace billiards
