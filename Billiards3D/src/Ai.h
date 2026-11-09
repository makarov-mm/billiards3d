#pragma once

#include "Physics.h"
#include "Rules.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace billiards {

// A stroke: which ball the cue strikes, where to, how hard, and where on the ball; first putting the ball down
// when it is in hand, and calling a pocket for the eight.
struct Shot {
    int struck = 0;
    V2 direction{1, 0};
    double speed = 1;     // of the struck ball, m/s
    double side = 0;      // right of the middle, as a fraction of the radius
    double vertical = 0;  // above the middle
    int calledPocket = -1;
    bool place = false;
    V2 spot;
};

// The fastest a cue sends a ball on each table.
constexpr double MaxSpeed(TableKind kind) { return kind == TableKind::Pool ? 8.5 : 7.5; }

// Plays a stroke out on a copy of the table: the events and where the balls stop.
Simulation Play(const Simulation& position, const Shot& shot, std::vector<Event>* events);

// The computer player. For each ball it may strike, each ball it may pot and each pocket it finds the line
// through the ghost ball, keeps the clear and easy ones, plays them out with a few speeds and spins to see
// where the balls stop and how easy the next shot is, tests the best against its own unsteadiness, and plays
// safe when nothing is likely to go in. Lower levels look at fewer shots and strike less accurately.
class Ai {
public:
    static Shot Choose(const Simulation& position, const Match& match, int level, std::uint64_t seed, std::stop_token stop);
    // The line to the ghost ball for potting `target` into `pocket` with `struck`, if clear: the direction and how
    // easy it is, 0 to 1.
    static std::optional<std::pair<V2, double>> Line(const Simulation& position, int struck, int target, int pocket);
    // The easiest pot the player could try from this position, 0 to 1.
    static double BestChance(const Simulation& position, const Match& match, int player);
};

// Runs a computation on a thread of its own; the window polls for the result.
template <class Result>
class Worker {
public:
    Worker() = default;
    ~Worker() { Cancel(); }
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    template <class Function>
    void Start(Function function)
    {
        Cancel();
        m_thread = std::jthread([this, function = std::move(function)](std::stop_token stop) {
            Result result = function(stop);
            if (!stop.stop_requested()) {
                std::scoped_lock lock(m_mutex);
                m_result = std::move(result);
            }
        });
    }

    void Cancel()
    {
        if (m_thread.joinable()) {
            m_thread.request_stop();
            m_thread.join();
        }
        std::scoped_lock lock(m_mutex);
        m_result.reset();
    }

    std::optional<Result> Take()
    {
        std::scoped_lock lock(m_mutex);
        return std::exchange(m_result, std::nullopt);
    }

private:
    std::jthread m_thread;
    std::mutex m_mutex;
    std::optional<Result> m_result;
};

} // namespace billiards
