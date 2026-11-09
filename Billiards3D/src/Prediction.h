#pragma once

#include "Physics.h"

#include <vector>

// Where a stroke would send the balls, for the lines of the aim: the struck ball's path to the first ball it
// meets, and how both go on from there. Found by playing the stroke out on a copy of the table.
namespace billiards {

struct Prediction {
    std::vector<V2> path;   // the struck ball, up to the first ball it meets, where it stops or where it drops
    int hit = -1;           // the first ball it meets
    V2 contact;             // the struck ball's middle as it meets it: the ghost ball
    std::vector<V2> after;  // the struck ball after that, as far as `reachAfter`
    std::vector<V2> object; // the ball met, as far as `reachObject` or its first cushion
    int objectPocket = -1;  // the pocket the ball met drops into, before it touches anything else
    int struckPocket = -1;  // the pocket the struck ball drops into before it meets a ball
};

Prediction Predict(const Simulation& position, int struck, V2 direction, double speed, double side, double vertical, double reachAfter,
                   double reachObject);

// The pocket a ball rolling from `from` along `direction` heads for: the one whose mouth lies nearest the line.
int PocketAlong(const TableSpec& table, V2 from, V2 direction);

} // namespace billiards
