#include "Ai.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>

namespace billiards {

namespace {

struct LevelSpec {
    int candidates;    // pots looked at closely
    int speeds, spins; // variants of each
    double aim;        // the spread of the aim, degrees
    double speed;      // of the speed, as a fraction
    double spin;       // of the point struck, as a fraction of the radius
    int finalists, samples;
};

constexpr std::array<LevelSpec, 5> Levels{{
    {4, 1, 1, 1.5, 0.12, 0.14, 1, 3},
    {8, 1, 1, 0.85, 0.08, 0.1, 2, 4},
    {14, 2, 3, 0.42, 0.05, 0.06, 3, 5},
    {26, 3, 3, 0.18, 0.03, 0.035, 5, 8},
    {40, 3, 3, 0.07, 0.015, 0.015, 6, 12},
}};

constexpr double Pi = std::numbers::pi;

V2 Rotate(V2 v, double angle)
{
    const double c = std::cos(angle), s = std::sin(angle);
    return {v.x * c - v.z * s, v.x * s + v.z * c};
}

double DistanceToSegment(V2 p, V2 a, V2 b)
{
    const V2 ab = b - a;
    const double length = Dot(ab, ab);
    const double t = length > 1e-12 ? std::clamp(Dot(p - a, ab) / length, 0.0, 1.0) : 0.0;
    return Length(p - (a + ab * t));
}

// No ball but the two named comes within `clearance` of the line from a to b.
bool Clear(const Simulation& sim, V2 a, V2 b, int skip1, int skip2, double clearance)
{
    for (int i = 0; i < sim.count; ++i) {
        const Ball& ball = sim.balls[static_cast<std::size_t>(i)];
        if (i == skip1 || i == skip2 || !ball.OnTable())
            continue;
        if (DistanceToSegment(ball.p, a, b) < clearance)
            return false;
    }
    return true;
}

// Enough speed for the struck ball to send the other `toPocket` metres, still rolling at the pocket, after a cut
// whose cosine is `cut`.
double PotSpeed(const TableSpec& t, double toGhost, double toPocket, double cut)
{
    constexpr double g = 9.81;
    const double objectSpeed = std::sqrt(2 * t.rollFriction * g * (toPocket + 0.35)) * 1.3 + 0.15;
    const double atContact = objectSpeed / std::max(0.2, cut * (1 + t.ballRestitution) / 2);
    const double start = std::sqrt(atContact * atContact + 2 * t.rollFriction * g * toGhost * 1.5);
    return std::clamp(start, 0.5, MaxSpeed(t.kind) * 0.85);
}

std::vector<int> Strikable(const Simulation& sim, const Match& match)
{
    std::vector<int> balls;
    for (int i = 0; i < sim.count; ++i)
        if (match.MayStrike(sim, i))
            balls.push_back(i);
    return balls;
}

// What a stroke is worth to `me`, from where the balls stopped: winning or losing, fouls, balls scored, and the
// next shot, mine or the other side's.
double Worth(const Match& match, const Simulation& after, const Shot& shot, const std::vector<Event>& events, int me)
{
    Match m = match;
    Simulation s = after;
    const Verdict verdict = Judge(m, s, Record(shot.struck, events, shot.calledPocket));
    if (m.winner >= 0)
        return m.winner == me ? 1000 : -1000;
    double worth = 0;
    if (verdict.foul != Foul::None)
        worth -= match.type == GameType::Americanka ? 110 : 170;
    if (match.type == GameType::Americanka)
        worth += 110 * verdict.scored - (verdict.forfeited >= 0 ? 60 : 0) + 25 * (m.Score(me) - m.Score(1 - me));
    if (verdict.keepsTurn)
        worth += 100 + 80 * Ai::BestChance(s, m, me);
    else
        worth -= 75 * Ai::BestChance(s, m, 1 - me);
    return worth;
}

Shot WithNoise(Shot shot, const LevelSpec& level, std::mt19937_64& random, TableKind kind)
{
    std::normal_distribution<double> normal;
    shot.direction = Rotate(shot.direction, normal(random) * level.aim * Pi / 180);
    shot.speed = std::clamp(shot.speed * (1 + normal(random) * level.speed), 0.2, MaxSpeed(kind));
    shot.side += normal(random) * level.spin;
    shot.vertical += normal(random) * level.spin;
    const double offset = std::hypot(shot.side, shot.vertical);
    if (offset > 0.5) {
        shot.side *= 0.5 / offset;
        shot.vertical *= 0.5 / offset;
    }
    return shot;
}

} // namespace

Simulation Play(const Simulation& position, const Shot& shot, std::vector<Event>* events)
{
    Simulation sim = position;
    sim.turnBalls = false;
    sim.time = 0;
    if (shot.place) {
        Ball& ball = sim.balls[static_cast<std::size_t>(shot.struck)];
        ball = Ball{};
        ball.p = shot.spot;
    }
    sim.Strike(shot.struck, shot.direction, shot.speed, shot.side, shot.vertical);
    sim.Run(events);
    return sim;
}

std::optional<std::pair<V2, double>> Ai::Line(const Simulation& sim, int struck, int target, int pocket)
{
    const TableSpec& t = sim.Table();
    const double R = t.ballRadius;
    const Ball& s = sim.balls[static_cast<std::size_t>(struck)];
    const Ball& o = sim.balls[static_cast<std::size_t>(target)];
    const Pocket& p = t.pockets[static_cast<std::size_t>(pocket)];
    const V2 aim = p.mouth + p.axis * (0.25 * R);
    V2 u = aim - o.p;
    const double toPocket = Length(u);
    if (toPocket < 1e-6)
        return std::nullopt;
    u = u * (1 / toPocket);
    // A pocket takes a ball from in front; a middle pocket refuses steep ones.
    const double approach = Dot(u, p.axis);
    if (approach < (p.corner ? 0.4 : 0.62))
        return std::nullopt;
    const V2 ghost = o.p - u * (2 * R);
    V2 line = ghost - s.p;
    const double toGhost = Length(line);
    if (toGhost < 1e-6)
        return std::nullopt;
    line = line * (1 / toGhost);
    const double cut = Dot(line, u);
    if (cut < 0.17)
        return std::nullopt;
    if (std::abs(ghost.x) > t.length / 2 - R * 0.9 || std::abs(ghost.z) > t.width / 2 - R * 0.9)
        return std::nullopt;
    if (!Clear(sim, s.p, ghost, struck, target, 2 * R - 1e-4) || !Clear(sim, o.p, aim, target, struck, 2 * R - 1e-4))
        return std::nullopt;
    const double ease = std::pow(cut, 1.6) * std::pow(approach, 0.6) / (1 + 0.8 * toGhost + 1.0 * toPocket);
    return std::pair{line, std::min(1.0, ease * 1.6)};
}

double Ai::BestChance(const Simulation& sim, const Match& match, int player)
{
    if (match.Over())
        return 0;
    if (match.inHand != InHand::None)
        return 0.9;
    double best = 0;
    const std::vector<int> targets = match.Targets(sim, player);
    for (const int struck : Strikable(sim, match))
        for (const int target : targets)
            if (target != struck)
                for (int pocket = 0; pocket < 6; ++pocket)
                    if (const auto line = Line(sim, struck, target, pocket))
                        best = std::max(best, line->second);
    return best;
}

Shot Ai::Choose(const Simulation& position, const Match& match, int level, std::uint64_t seed, std::stop_token stop)
{
    const LevelSpec& spec = Levels[static_cast<std::size_t>(std::clamp(level, 1, 5) - 1)];
    std::mt19937_64 random(seed);
    std::uniform_real_distribution<double> uniform(-1, 1);
    const TableSpec& t = position.Table();
    const double R = t.ballRadius;
    const int me = match.toPlay;

    // The break: from the kitchen into the head of the rack, hard.
    if (match.breakShot) {
        Shot shot;
        shot.struck = 0;
        shot.place = true;
        const bool pool = t.kind == TableKind::Pool;
        shot.spot = {t.HeadString() - 0.04, (pool ? 0.22 : 0.08) * uniform(random) * t.width / 2};
        V2 apex = t.FootSpot();
        for (int i = 1; i < position.count; ++i)
            if (position.balls[static_cast<std::size_t>(i)].OnTable() && position.balls[static_cast<std::size_t>(i)].p.x < apex.x + R)
                apex = position.balls[static_cast<std::size_t>(i)].p;
        shot.direction = Normalized(apex + V2{0, uniform(random) * R * 0.15} - shot.spot);
        shot.speed = pool ? 8.0 : 5.5;
        shot.vertical = -0.12;
        return WithNoise(shot, spec, random, t.kind);
    }

    struct Option {
        Shot shot;
        double quality = 0;
        double worth = -1e9;
    };
    std::vector<Option> options;

    // Where to put a ball in hand: behind each target, in line with each pocket, a little off it.
    std::vector<std::optional<V2>> spots{std::nullopt};
    const bool inHand = match.inHand != InHand::None && position.balls[0].OnTable();
    if (inHand) {
        spots.clear();
        std::vector<std::pair<double, V2>> ranked;
        Simulation probe = position;
        for (const int target : match.Targets(position, me)) {
            if (target == 0)
                continue;
            for (int pocket = 0; pocket < 6; ++pocket) {
                const Pocket& p = t.pockets[static_cast<std::size_t>(pocket)];
                const V2 u = Normalized(p.mouth - position.balls[static_cast<std::size_t>(target)].p);
                for (const double distance : {0.15, 0.32, 0.55}) {
                    for (const double angle : {0.0, 0.3, -0.3}) {
                        const V2 spot = position.balls[static_cast<std::size_t>(target)].p - Rotate(u, angle) * (2 * R + distance);
                        if ((match.inHand == InHand::Kitchen && spot.x > t.HeadString() - R) || !position.FreeSpot(spot, 0))
                            continue;
                        probe.balls[0].p = spot;
                        if (const auto line = Line(probe, 0, target, pocket))
                            ranked.emplace_back(line->second, spot);
                    }
                }
            }
        }
        std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first > b.first; });
        for (std::size_t i = 0; i < std::min<std::size_t>(ranked.size(), 10); ++i)
            spots.emplace_back(ranked[i].second);
        if (spots.empty())
            spots.emplace_back(position.balls[0].p);
    }

    // The pots, easiest first.
    for (const auto& spot : spots) {
        Simulation base = position;
        if (spot)
            base.balls[0].p = *spot;
        const std::vector<int> targets = match.Targets(base, me);
        for (const int struck : Strikable(base, match)) {
            for (const int target : targets) {
                if (target == struck)
                    continue;
                for (int pocket = 0; pocket < 6; ++pocket) {
                    const auto line = Line(base, struck, target, pocket);
                    if (!line)
                        continue;
                    const Pocket& p = t.pockets[static_cast<std::size_t>(pocket)];
                    const V2 ghost = base.balls[static_cast<std::size_t>(target)].p - Normalized(p.mouth - base.balls[static_cast<std::size_t>(target)].p) * (2 * R);
                    Shot shot;
                    shot.struck = struck;
                    shot.direction = line->first;
                    shot.speed = PotSpeed(t, Length(ghost - base.balls[static_cast<std::size_t>(struck)].p),
                                          Length(p.mouth - base.balls[static_cast<std::size_t>(target)].p), Dot(line->first, Normalized(p.mouth - ghost)));
                    shot.calledPocket = match.type == GameType::EightBall && target == 8 ? pocket : -1;
                    if (spot) {
                        shot.place = true;
                        shot.spot = *spot;
                    }
                    options.push_back({shot, line->second});
                }
            }
        }
    }
    std::ranges::sort(options, [](const Option& a, const Option& b) { return a.quality > b.quality; });
    if (options.size() > static_cast<std::size_t>(spec.candidates))
        options.resize(static_cast<std::size_t>(spec.candidates));

    // Each played out with a few speeds and spins, for the position after it.
    std::vector<Option> tried;
    std::vector<Event> events;
    const std::array<double, 3> speedFactors = spec.speeds == 1 ? std::array{1.0, 1.0, 1.0} : spec.speeds == 2 ? std::array{0.85, 1.25, 1.25} : std::array{0.75, 1.0, 1.4};
    const std::array<double, 3> spins{0, 0.35, -0.35};
    for (const Option& option : options) {
        for (int s = 0; s < spec.speeds; ++s) {
            for (int k = 0; k < spec.spins; ++k) {
                if (stop.stop_requested())
                    return option.shot;
                Option variant = option;
                variant.shot.speed = std::clamp(option.shot.speed * speedFactors[static_cast<std::size_t>(s)], 0.4, MaxSpeed(t.kind));
                variant.shot.vertical = spins[static_cast<std::size_t>(k)];
                events.clear();
                const Simulation after = Play(position, variant.shot, &events);
                variant.worth = Worth(match, after, variant.shot, events, me);
                tried.push_back(variant);
            }
        }
    }

    // Nothing promising: play safe, touching a ball the rules allow and leaving the other side little.
    const double bestPot = tried.empty() ? -1e9 : std::ranges::max(tried, {}, &Option::worth).worth;
    if (bestPot < 60) {
        std::vector<int> struckBalls = Strikable(position, match);
        if (inHand)
            struckBalls = {0};
        const std::vector<int> targets = match.Targets(position, me);
        int budget = 160;
        for (const int struck : struckBalls) {
            for (const int target : targets) {
                if (target == struck)
                    continue;
                const V2 from = inHand ? position.balls[0].p : position.balls[static_cast<std::size_t>(struck)].p;
                const V2 to = position.balls[static_cast<std::size_t>(target)].p;
                const V2 across = Perpendicular(Normalized(to - from));
                auto attempt = [&](V2 direction, double speed) {
                    if (budget-- <= 0 || stop.stop_requested())
                        return;
                    Option option;
                    option.shot.struck = struck;
                    option.shot.direction = direction;
                    option.shot.speed = speed;
                    events.clear();
                    const Simulation after = Play(position, option.shot, &events);
                    option.worth = Worth(match, after, option.shot, events, me);
                    tried.push_back(option);
                };
                for (const double thickness : {-0.8, -0.35, 0.0, 0.35, 0.8})
                    for (const double speed : {0.9, 1.6, 2.6})
                        attempt(Normalized(to + across * (thickness * 2 * R) - from), speed);
                // Hidden behind other balls: off a cushion, aiming at the target's image in it.
                if (!Clear(position, from, to, struck, target, 2 * R - 1e-4)) {
                    const double x = t.length / 2 - t.contactRadius, z = t.width / 2 - t.contactRadius;
                    for (const V2 image : {V2{2 * x - to.x, to.z}, V2{-2 * x - to.x, to.z}, V2{to.x, 2 * z - to.z}, V2{to.x, -2 * z - to.z}})
                        for (const double speed : {1.6, 2.8})
                            attempt(Normalized(image - from), speed);
                }
            }
        }
    }
    if (tried.empty()) {
        // Hemmed in: at least hit something.
        Shot shot;
        shot.struck = Strikable(position, match).empty() ? 0 : Strikable(position, match).front();
        const std::vector<int> targets = match.Targets(position, me);
        const int target = targets.empty() ? shot.struck : targets.front();
        shot.direction = Normalized(position.balls[static_cast<std::size_t>(target)].p - position.balls[static_cast<std::size_t>(shot.struck)].p);
        shot.speed = 2.5;
        return WithNoise(shot, spec, random, t.kind);
    }

    // The best few tested against the computer's own unsteadiness.
    std::ranges::sort(tried, [](const Option& a, const Option& b) { return a.worth > b.worth; });
    const std::size_t finalists = std::min<std::size_t>(tried.size(), static_cast<std::size_t>(spec.finalists));
    double bestScore = -1e18;
    Shot best = tried.front().shot;
    for (std::size_t i = 0; i < finalists; ++i) {
        double total = 0;
        for (int sample = 0; sample < spec.samples; ++sample) {
            if (stop.stop_requested())
                return best;
            const Shot noisy = WithNoise(tried[i].shot, spec, random, t.kind);
            events.clear();
            const Simulation after = Play(position, noisy, &events);
            total += Worth(match, after, noisy, events, me);
        }
        const double score = 0.35 * tried[i].worth + 0.65 * total / spec.samples;
        if (score > bestScore) {
            bestScore = score;
            best = tried[i].shot;
        }
    }
    return WithNoise(best, spec, random, t.kind);
}

} // namespace billiards
