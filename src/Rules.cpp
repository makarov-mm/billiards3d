#include "Rules.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <sstream>
#include <utility>

namespace billiards {

namespace {

Quat RandomOrientation(std::mt19937_64& random)
{
    std::normal_distribution<double> normal;
    Quat q{normal(random), normal(random), normal(random), normal(random)};
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

void SetBall(Simulation& sim, int ball, V2 p, std::mt19937_64& random)
{
    Ball& b = sim.balls[static_cast<std::size_t>(ball)];
    b = Ball{};
    b.p = p;
    b.q = RandomOrientation(random);
}

// The cue ball back on the table for a ball in hand: on the head spot or near it, to be moved by the player.
void PlaceCueBall(Simulation& sim)
{
    const TableSpec& t = sim.Table();
    Ball& cue = sim.balls[0];
    const double R = t.ballRadius;
    for (int ring = 0; ring < 40; ++ring) {
        for (int k = 0; k < std::max(1, ring * 6); ++k) {
            const double angle = 2 * std::numbers::pi * k / std::max(1, ring * 6);
            const V2 p = V2{t.HeadString() - 2 * R, 0} + V2{std::cos(angle), std::sin(angle)} * (ring * 2.2 * R);
            if (p.x < t.HeadString() - R && sim.FreeSpot(p, 0)) {
                cue = Ball{};
                cue.p = p;
                return;
            }
        }
    }
}

// No rack is perfect: the balls are a fraction of a millimetre off their places. Without that the break would
// pass straight through rows lined up exactly, and leave most of the rack where it stood.
void Loosen(Simulation& sim, std::mt19937_64& random)
{
    std::normal_distribution<double> off(0.0, 0.0003 * sim.Table().ballRadius / 0.028575);
    for (int i = 1; i < sim.count; ++i)
        sim.balls[static_cast<std::size_t>(i)].p = sim.balls[static_cast<std::size_t>(i)].p + V2{off(random), off(random)};
}

} // namespace

bool ShotRecord::Pocketed(int ball) const
{
    return std::ranges::any_of(pocketed, [&](const auto& entry) { return entry.first == ball; });
}

int ShotRecord::PocketOf(int ball) const
{
    for (const auto& [b, pocket] : pocketed)
        if (b == ball)
            return pocket;
    return -1;
}

ShotRecord Record(int struck, const std::vector<Event>& events, int calledPocket)
{
    ShotRecord record;
    record.struck = struck;
    record.calledPocket = calledPocket;
    for (const Event& event : events) {
        if (event.type == EventType::BallBall) {
            if (record.firstContact < 0 && (event.a == struck || event.b == struck))
                record.firstContact = event.a == struck ? event.b : event.a;
        }
        else if (event.type == EventType::Cushion) {
            if (record.firstContact >= 0)
                record.railAfterContact = true;
        }
        else {
            record.pocketed.emplace_back(event.a, event.b);
        }
    }
    return record;
}

bool Match::MayStrike(const Simulation& sim, int ball) const
{
    if (ball < 0 || ball >= sim.count || !sim.balls[static_cast<std::size_t>(ball)].OnTable())
        return false;
    return type == GameType::Americanka && !breakShot ? true : ball == 0;
}

int Match::BallsLeft(const Simulation& sim, Group group) const
{
    int left = 0;
    for (int i = 1; i < sim.count; ++i)
        left += InGroup(i, group) && sim.balls[static_cast<std::size_t>(i)].OnTable() ? 1 : 0;
    return left;
}

bool Match::OnEight(const Simulation& sim, int player) const
{
    const Group group = groups[static_cast<std::size_t>(player)];
    return type == GameType::EightBall && group != Group::Open && BallsLeft(sim, group) == 0;
}

int Match::Lowest(const Simulation& sim) const
{
    for (int i = 1; i < sim.count; ++i)
        if (sim.balls[static_cast<std::size_t>(i)].OnTable())
            return i;
    return -1;
}

std::vector<int> Match::Targets(const Simulation& sim, int player) const
{
    std::vector<int> targets;
    auto onTable = [&](int i) { return sim.balls[static_cast<std::size_t>(i)].OnTable(); };
    if (type == GameType::NineBall) {
        if (const int lowest = Lowest(sim); lowest > 0)
            targets.push_back(lowest);
    }
    else if (type == GameType::EightBall) {
        const Group group = groups[static_cast<std::size_t>(player)];
        if (OnEight(sim, player)) {
            if (onTable(8))
                targets.push_back(8);
        }
        else {
            for (int i = 1; i < sim.count; ++i)
                if (onTable(i) && (group == Group::Open ? i != 8 || breakShot : InGroup(i, group)))
                    targets.push_back(i);
        }
    }
    else {
        for (int i = 0; i < sim.count; ++i)
            if (onTable(i))
                targets.push_back(i);
    }
    return targets;
}

void Rack(GameType type, Simulation& sim, std::mt19937_64& random)
{
    const TableSpec& t = sim.Table();
    const double R = t.ballRadius, gap = 1.0004;
    const V2 foot = t.FootSpot();
    auto spot = [&](int row, double across) { return foot + V2{row * std::sqrt(3.0) * R * gap, across * 2 * R * gap}; };
    sim.count = type == GameType::NineBall ? 10 : 16;
    sim.time = 0;
    for (Ball& ball : sim.balls)
        ball = Ball{};
    SetBall(sim, 0, {t.HeadString() - 2 * R, 0}, random);

    if (type == GameType::NineBall) {
        // A diamond: the one at the front, the nine in the middle.
        std::vector<int> rest{2, 3, 4, 5, 6, 7, 8};
        std::ranges::shuffle(rest, random);
        constexpr std::array<int, 5> rows{1, 2, 3, 2, 1};
        int next = 0;
        for (int row = 0; row < 5; ++row) {
            for (int k = 0; k < rows[static_cast<std::size_t>(row)]; ++k) {
                const int ball = row == 0 ? 1 : row == 2 && k == 1 ? 9 : rest[static_cast<std::size_t>(next++)];
                SetBall(sim, ball, spot(row, k - (rows[static_cast<std::size_t>(row)] - 1) / 2.0), random);
            }
        }
        Loosen(sim, random);
        return;
    }

    // A triangle of fifteen. In eight-ball the eight in the middle and a solid and a stripe in the back corners.
    std::vector<int> order(15);
    for (int i = 0; i < 15; ++i)
        order[static_cast<std::size_t>(i)] = i + 1;
    std::ranges::shuffle(order, random);
    if (type == GameType::EightBall) {
        std::vector<int> solids{1, 2, 3, 4, 5, 6, 7}, stripes{9, 10, 11, 12, 13, 14, 15};
        std::ranges::shuffle(solids, random);
        std::ranges::shuffle(stripes, random);
        std::vector<int> others;
        for (std::size_t i = 1; i < 7; ++i) {
            others.push_back(solids[i]);
            others.push_back(stripes[i]);
        }
        std::ranges::shuffle(others, random);
        const bool solidLeft = random() % 2 == 0;
        order.clear();
        std::size_t next = 0;
        for (int row = 0; row < 5; ++row) {
            for (int k = 0; k <= row; ++k) {
                if (row == 2 && k == 1)
                    order.push_back(8);
                else if (row == 4 && k == 0)
                    order.push_back(solidLeft ? solids[0] : stripes[0]);
                else if (row == 4 && k == 4)
                    order.push_back(solidLeft ? stripes[0] : solids[0]);
                else
                    order.push_back(others[next++]);
            }
        }
    }
    std::size_t index = 0;
    for (int row = 0; row < 5; ++row)
        for (int k = 0; k <= row; ++k)
            SetBall(sim, order[index++], spot(row, k - row / 2.0), random);
    Loosen(sim, random);
}

void Respot(Simulation& sim, int ball)
{
    const TableSpec& t = sim.Table();
    const double R = t.ballRadius;
    Ball& b = sim.balls[static_cast<std::size_t>(ball)];
    const Quat q = b.q;
    auto place = [&](V2 p) {
        b = Ball{};
        b.p = p;
        b.q = q;
    };
    for (double x = t.FootSpot().x; x < t.length / 2 - R; x += 0.25 * R)
        if (sim.FreeSpot({x, 0}, ball))
            return place({x, 0});
    for (double x = t.FootSpot().x; x > -t.length / 2 + R; x -= 0.25 * R)
        if (sim.FreeSpot({x, 0}, ball))
            return place({x, 0});
    for (double z = 0; z < t.width / 2; z += 0.5 * R)
        for (const double side : {1.0, -1.0})
            if (sim.FreeSpot({t.FootSpot().x, side * z}, ball))
                return place({t.FootSpot().x, side * z});
}

Verdict Judge(Match& match, Simulation& sim, const ShotRecord& shot)
{
    Verdict verdict;
    const int player = match.toPlay, other = 1 - player;
    const bool breakShot = match.breakShot;
    match.breakShot = false;
    bool objectPocketed = false;
    for (const auto& [ball, pocket] : shot.pocketed)
        objectPocketed |= ball != shot.struck || match.type == GameType::Americanka;

    if (match.practice) {
        // No fouls: the cue ball comes back in hand, and the table is cleared at leisure.
        if (match.type != GameType::Americanka && shot.Pocketed(0)) {
            match.inHand = InHand::Anywhere;
            PlaceCueBall(sim);
        }
        else {
            match.inHand = InHand::None;
        }
        int left = 0;
        for (int i = match.type == GameType::Americanka ? 0 : 1; i < sim.count; ++i)
            left += sim.balls[static_cast<std::size_t>(i)].OnTable() ? 1 : 0;
        verdict.keepsTurn = true;
        if (left <= (match.type == GameType::Americanka ? 1 : 0))
            match.winner = verdict.winner = 0;
        return verdict;
    }

    if (match.type == GameType::Americanka) {
        // Any ball may be struck; every ball pocketed by a fair stroke counts, the struck one too.
        if (shot.firstContact < 0)
            verdict.foul = Foul::Miss;
        else if (shot.pocketed.empty() && !shot.railAfterContact)
            verdict.foul = Foul::NoRail;
        auto& taken = match.taken[static_cast<std::size_t>(player)];
        if (verdict.foul == Foul::None) {
            for (const auto& [ball, pocket] : shot.pocketed)
                taken.push_back(ball);
            verdict.scored = static_cast<int>(shot.pocketed.size());
            verdict.keepsTurn = verdict.scored > 0;
            if (match.Score(player) >= Match::Target)
                match.winner = verdict.winner = player;
        }
        else {
            // The balls just pocketed go back, and the offender gives up one of his.
            for (const auto& [ball, pocket] : shot.pocketed)
                verdict.respot.push_back(ball);
            if (!taken.empty()) {
                verdict.forfeited = taken.back();
                verdict.respot.push_back(taken.back());
                taken.pop_back();
            }
        }
        for (const int ball : verdict.respot)
            Respot(sim, ball);
        match.inHand = InHand::None;
        // With one ball left there is nothing to strike it at: the higher score wins.
        int left = 0;
        for (int i = 0; i < sim.count; ++i)
            left += sim.balls[static_cast<std::size_t>(i)].OnTable() ? 1 : 0;
        if (match.winner < 0 && left < 2)
            match.winner = verdict.winner = match.Score(0) >= match.Score(1) ? 0 : 1;
        if (!verdict.keepsTurn)
            match.toPlay = other;
        return verdict;
    }

    const bool scratch = shot.Pocketed(0);
    if (match.type == GameType::NineBall) {
        // The lowest ball on the table before the stroke must be hit first.
        int lowest = 99;
        for (int i = 1; i < sim.count; ++i)
            if (sim.balls[static_cast<std::size_t>(i)].OnTable() || shot.Pocketed(i))
                lowest = std::min(lowest, i);
        if (scratch)
            verdict.foul = Foul::Scratch;
        else if (shot.firstContact < 0)
            verdict.foul = Foul::Miss;
        else if (shot.firstContact != lowest)
            verdict.foul = Foul::WrongBall;
        else if (!breakShot && !objectPocketed && !shot.railAfterContact)
            verdict.foul = Foul::NoRail;
        if (shot.Pocketed(9)) {
            if (verdict.foul == Foul::None)
                match.winner = verdict.winner = player;
            else
                verdict.respot.push_back(9);
        }
        verdict.keepsTurn = verdict.foul == Foul::None && objectPocketed;
    }
    else {
        const Group group = match.groups[static_cast<std::size_t>(player)];
        int groupBefore = 0;
        for (int i = 1; i < sim.count; ++i)
            groupBefore += InGroup(i, group) && (sim.balls[static_cast<std::size_t>(i)].OnTable() || shot.Pocketed(i)) ? 1 : 0;
        const bool onEight = group != Group::Open && groupBefore == 0;
        if (scratch)
            verdict.foul = Foul::Scratch;
        else if (shot.firstContact < 0)
            verdict.foul = Foul::Miss;
        else if (!breakShot && (onEight ? shot.firstContact != 8 : group == Group::Open ? shot.firstContact == 8 : !InGroup(shot.firstContact, group)))
            verdict.foul = Foul::WrongBall;
        else if (!breakShot && !objectPocketed && !shot.railAfterContact)
            verdict.foul = Foul::NoRail;

        if (shot.Pocketed(8)) {
            if (breakShot) {
                verdict.respot.push_back(8);
            }
            else {
                match.winner = verdict.winner = other;
                if (!onEight)
                    verdict.foul = Foul::EightEarly;
                else if (verdict.foul != Foul::None)
                    verdict.foul = Foul::EightOnFoul;
                else if (shot.calledPocket >= 0 && shot.PocketOf(8) != shot.calledPocket)
                    verdict.foul = Foul::EightWrongPocket;
                else
                    match.winner = verdict.winner = player;
            }
        }
        if (match.winner < 0 && verdict.foul == Foul::None) {
            Group mine = group;
            if (group == Group::Open && !breakShot) {
                // The first ball pocketed after the break decides the groups.
                for (const auto& [ball, pocket] : shot.pocketed) {
                    if (ball != 0 && ball != 8) {
                        mine = ball < 8 ? Group::Solids : Group::Stripes;
                        match.groups[static_cast<std::size_t>(player)] = mine;
                        match.groups[static_cast<std::size_t>(other)] = mine == Group::Solids ? Group::Stripes : Group::Solids;
                        verdict.assigned = mine;
                        break;
                    }
                }
            }
            for (const auto& [ball, pocket] : shot.pocketed)
                verdict.keepsTurn |= breakShot ? ball != 0 : InGroup(ball, mine);
        }
    }

    for (const int ball : verdict.respot)
        Respot(sim, ball);
    if (verdict.foul != Foul::None && match.winner < 0) {
        match.inHand = breakShot && scratch ? InHand::Kitchen : InHand::Anywhere;
        if (scratch)
            PlaceCueBall(sim);
    }
    else {
        match.inHand = InHand::None;
    }
    if (!verdict.keepsTurn)
        match.toPlay = other;
    return verdict;
}

// Serialization ----------------------------------------------------------------------------------

std::string Serialize(const Match& match, const Simulation& sim)
{
    std::ostringstream out;
    out.precision(17);
    out << "B1 " << static_cast<int>(match.type) << ' ' << (match.practice ? 1 : 0) << ' ' << match.toPlay << ' ' << static_cast<int>(match.groups[0])
        << ' ' << static_cast<int>(match.groups[1]) << ' ' << (match.breakShot ? 1 : 0) << ' ' << static_cast<int>(match.inHand) << ' ' << match.winner;
    for (const auto& taken : match.taken) {
        out << ' ' << taken.size();
        for (const int ball : taken)
            out << ' ' << ball;
    }
    out << ' ' << sim.count;
    for (int i = 0; i < sim.count; ++i) {
        const Ball& b = sim.balls[static_cast<std::size_t>(i)];
        out << ' ' << static_cast<int>(b.state) << ' ' << b.pocket << ' ' << b.p.x << ' ' << b.p.z << ' ' << b.q.w << ' ' << b.q.x << ' ' << b.q.y << ' ' << b.q.z;
    }
    return out.str();
}

bool Deserialize(std::string_view text, Match& match, Simulation& sim)
{
    std::istringstream in{std::string(text)};
    std::string tag;
    int type = 0, practice = 0, toPlay = 0, group0 = 0, group1 = 0, breakShot = 0, inHand = 0, winner = -1;
    if (!(in >> tag >> type >> practice >> toPlay >> group0 >> group1 >> breakShot >> inHand >> winner) || tag != "B1" || type < 0 || type >= GameTypeCount ||
        toPlay < 0 || toPlay > 1 || group0 < 0 || group0 > 2 || group1 < 0 || group1 > 2 || inHand < 0 || inHand > 2 || winner < -1 || winner > 1)
        return false;
    Match m;
    m.type = static_cast<GameType>(type);
    m.practice = practice != 0;
    m.toPlay = toPlay;
    m.groups = {static_cast<Group>(group0), static_cast<Group>(group1)};
    m.breakShot = breakShot != 0;
    m.inHand = static_cast<InHand>(inHand);
    m.winner = winner;
    for (auto& taken : m.taken) {
        std::size_t size = 0;
        if (!(in >> size) || size > 16)
            return false;
        taken.resize(size);
        for (int& ball : taken)
            if (!(in >> ball) || ball < 0 || ball >= MaxBalls)
                return false;
    }
    int count = 0;
    if (!(in >> count) || count < 2 || count > MaxBalls || sim.Table().kind != TableFor(m.type))
        return false;
    Simulation s = sim;
    s.count = count;
    s.time = 0;
    for (int i = 0; i < count; ++i) {
        Ball& b = s.balls[static_cast<std::size_t>(i)];
        b = Ball{};
        int state = 0;
        if (!(in >> state >> b.pocket >> b.p.x >> b.p.z >> b.q.w >> b.q.x >> b.q.y >> b.q.z) || state < 0 || state > 1 || b.pocket < -1 || b.pocket > 5)
            return false;
        b.state = static_cast<BallState>(state);
        if (b.OnTable() && (!std::isfinite(b.p.x) || !std::isfinite(b.p.z) || !s.OnCloth(b.p)))
            return false;
    }
    match = std::move(m);
    sim = s;
    return true;
}

} // namespace billiards
