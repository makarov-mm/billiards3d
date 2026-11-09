#pragma once

#include "Physics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The games: eight-ball and nine-ball on a pool table, and «Американка» (free pyramid) on a Russian one, with
// their racks, fouls, ball in hand and the end of the game; and practice, one player shooting without fouls.
namespace billiards {

enum class GameType : std::uint8_t { EightBall, NineBall, Americanka };
constexpr int GameTypeCount = 3;

constexpr TableKind TableFor(GameType type) { return type == GameType::Americanka ? TableKind::Pyramid : TableKind::Pool; }

// In eight-ball: no group yet, the solids (1–7) or the stripes (9–15).
enum class Group : std::uint8_t { Open, Solids, Stripes };

constexpr bool InGroup(int ball, Group group)
{
    return group == Group::Solids ? ball >= 1 && ball <= 7 : group == Group::Stripes ? ball >= 9 && ball <= 15 : false;
}

enum class InHand : std::uint8_t { None, Kitchen, Anywhere };

enum class Foul : std::uint8_t { None, Scratch, Miss, WrongBall, NoRail, EightEarly, EightWrongPocket, EightOnFoul };

// What a stroke did, from the events of its simulation.
struct ShotRecord {
    int struck = 0;
    int firstContact = -1;         // the first ball the struck ball touched
    bool railAfterContact = false; // a ball met a cushion after that
    int calledPocket = -1;         // where the eight is to go, in eight-ball
    std::vector<std::pair<int, int>> pocketed; // ball and pocket, in order

    bool Pocketed(int ball) const;
    int PocketOf(int ball) const;
};

ShotRecord Record(int struck, const std::vector<Event>& events, int calledPocket);

// What the rules made of a stroke.
struct Verdict {
    Foul foul = Foul::None;
    bool keepsTurn = false;
    int winner = -1;           // the game is over
    std::vector<int> respot;   // balls taken out of the pockets and set on the foot spot
    Group assigned = Group::Open; // eight-ball: the group the shooter now has
    int scored = 0;            // Американка: the balls the shooter scored
    int forfeited = -1;        // Американка: a ball given back for a foul
};

struct Match {
    GameType type = GameType::EightBall;
    bool practice = false;     // one player, no fouls
    int toPlay = 0;
    std::array<Group, 2> groups{};
    std::array<std::vector<int>, 2> taken; // Американка: each player's balls
    bool breakShot = true;
    InHand inHand = InHand::Kitchen;
    int winner = -1;

    static constexpr int Target = 8; // Американка: balls to win

    int Score(int player) const { return static_cast<int>(taken[static_cast<std::size_t>(player)].size()); }
    bool Over() const { return winner >= 0; }
    // The balls the cue may strike: the cue ball, or in Американка (after the break) any ball.
    bool MayStrike(const Simulation& sim, int ball) const;
    // The balls that may be hit first, by the player to move.
    std::vector<int> Targets(const Simulation& sim, int player) const;
    // Eight-ball: the player has cleared their group and plays the eight.
    bool OnEight(const Simulation& sim, int player) const;
    int BallsLeft(const Simulation& sim, Group group) const;
    // The lowest ball on the table, in nine-ball.
    int Lowest(const Simulation& sim) const;
};

// Racks the balls for a new game; the cue ball waits in the kitchen.
void Rack(GameType type, Simulation& sim, std::mt19937_64& random);
// Applies the rules to the stroke just played, `sim` holding the balls where they came to rest.
Verdict Judge(Match& match, Simulation& sim, const ShotRecord& shot);
// Takes a ball out of its pocket and sets it on the foot spot, or the nearest free point behind it on the long line.
void Respot(Simulation& sim, int ball);

// The match and the balls as text, for the settings file.
std::string Serialize(const Match& match, const Simulation& sim);
bool Deserialize(std::string_view text, Match& match, Simulation& sim);

} // namespace billiards
