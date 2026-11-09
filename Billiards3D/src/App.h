#pragma once

#include "Ai.h"
#include "Camera.h"
#include "Gl.h"
#include "Physics.h"
#include "Prediction.h"
#include "Renderer.h"
#include "Rules.h"
#include "Settings.h"
#include "Sound.h"
#include "Strings.h"
#include "TableView.h"
#include "Text.h"
#include "UiPainter.h"
#include "Win32.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

// The game's window: it runs the matches, the computer players and the interface, and draws them.
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int Run();

private:
    enum Mode { VsComputer, TwoPlayers, PracticeMode, Demo };
    enum AimLines { AimFull, AimShort, AimNone };

    // Between strokes the game is ready for one; a computer thinks, then shows its aim; the cue strikes; the balls
    // roll; or the game is over.
    enum class Stage { Ready, Thinking, Presenting, Stroke, Rolling, Over };

    // What the mouse did since the last frame.
    struct Pointer {
        float x = -1, y = -1;
        bool leftDown = false, rightDown = false;
        bool leftPressed = false, leftReleased = false, rightPressed = false, rightReleased = false;
        float wheel = 0;
        bool consumed = false; // taken by the interface
    };

    enum class Gesture { None, Strike, PlaceBall, TurnCamera };

    struct Toast {
        std::string text;
        float age = 0;
    };

    struct Snapshot {
        billiards::Match match;
        billiards::Simulation sim;
        std::size_t history = 0;
        int shots = 0;
    };

    struct HistoryLine {
        std::string text;
        int kind = 0; // 0 a stroke, 1 a foul, 2 the end of the game
    };

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateMainWindow();
    void ShowLoading();
    void BuildFonts();
    void Frame();
    void Update(float seconds);
    bool NeedsAnimation() const;
    render::Scene BuildScene() const;

    // The game.
    billiards::GameType Type() const { return static_cast<billiards::GameType>(m_game); }
    const billiards::Simulation& Shown() const { return m_stage == Stage::Rolling ? m_live : m_sim; }
    bool IsHuman(int player) const;
    bool HumanToShoot() const;
    bool Aiming() const { return m_stage == Stage::Ready && HumanToShoot() && m_match.MayStrike(m_sim, m_struck); }
    bool OnEight() const { return m_match.OnEight(m_sim, m_match.toPlay); }
    void SetupTable(billiards::TableKind kind);
    bool LoadMatch(const std::string& text);
    void NewGame();
    void StartTurn();
    void DefaultAim();
    billiards::Shot MakeShot() const;
    double SpeedFor(float power) const;
    float PowerFor(double speed) const;
    // Starts the cue's stroke: from where it is drawn back, or first drawing it back.
    void Shoot(const billiards::Shot& shot, bool backswing);
    void Strike();
    void FinishShot();
    void Undo();
    void StopComputer();
    void UpdateComputer(float seconds);
    void UpdateStroke(float seconds);
    void UpdateRolling(float seconds);
    void UpdatePrediction();
    void TurnAim(float radians);
    void FollowAim(bool instant = false);
    void SetGame(int game);
    void SetMode(int mode);
    void SetLevel(int level);
    void SetTheme(int theme);
    void SetAimLines(int lines);
    void SetCamera(int camera);
    void SetSpin(float side, float vertical);
    void SetPower(float power);
    float PanAt(math::Vec3 point) const;
    void Save();
    void ToggleFullScreen();
    void ToggleLanguage();
    void LogShot(int shooter, const billiards::ShotRecord& record, const billiards::Verdict& verdict);
    void AddHistory(std::string text, int kind);

    // Input.
    void HandleTablePointer();
    void HandleKey(WPARAM key);
    bool PointerRay(math::Vec3& origin, math::Vec3& direction) const;
    std::optional<billiards::V2> PointerOnPlane(float height) const;
    int BallUnderPointer() const;
    int PocketUnderPointer() const;
    void PlaceCueBall(billiards::V2 wanted);

    // The interface.
    float Scale() const { return static_cast<float>(m_dpi) / 96.0f; }
    ui::Rect PanelRect() const;
    ui::Rect FreeArea() const;
    std::vector<render::FrostedPanel> FrostedPanels() const;
    std::vector<std::string> Wrap(std::string_view text, float width) const;
    ui::Rect HelpCard() const;
    ui::Rect BannerCard() const;
    void DrawInterface();
    void DrawPanel();
    void DrawStatus(ui::Rect area);
    void DrawPlayers(ui::Rect area);
    void DrawStroke(ui::Rect area);
    void DrawHistory(ui::Rect area);
    void DrawBanner();
    void DrawHelp();
    void DrawToasts();
    void DrawBallIcon(float x, float y, float radius, int ball);
    bool Button(int id, ui::Rect rect, std::string_view label, bool enabled = true, bool accent = false);
    // A label with a value stepped by two arrows; returns -1 or 1 when one is clicked, else 0.
    int Selector(int id, ui::Rect rect, std::string_view label, std::string_view value);
    void ShowToast(std::string text);
    void ShowBanner(std::string title, std::string detail, float seconds);
    std::string_view T(Str id) const { return Tr(id, m_language); }
    std::string Name(int player) const;
    std::string StatusTitle() const;
    std::string StatusDetail() const;
    std::string WinnerTitle() const;
    std::string BallName(int ball) const;
    std::string_view FoulText(billiards::Foul foul) const;
    math::Vec3 Accent() const;

    HINSTANCE m_instance;
    HWND m_window = nullptr;
    std::unique_ptr<gl::Context> m_context;
    std::unique_ptr<render::Renderer> m_renderer;
    std::unique_ptr<ui::Painter> m_painter;
    std::array<std::unique_ptr<text::Font>, 3> m_fonts;
    SoundBank m_sound;
    Settings m_settings;
    Language m_language = Language::English;
    int m_width = 0, m_height = 0;
    UINT m_dpi = 96;
    UINT m_fontDpi = 0;
    bool m_running = true;
    bool m_dirty = true; // something to draw since the last frame
    bool m_fullScreen = false;
    WINDOWPLACEMENT m_placement{sizeof(WINDOWPLACEMENT)};
    std::chrono::steady_clock::time_point m_lastFrame;
    float m_time = 0;

    // The game.
    billiards::TableSpec m_table; // the simulations point at it
    billiards::Match m_match;
    billiards::Simulation m_sim;  // the balls at rest between strokes
    billiards::Simulation m_live; // the stroke being played
    std::vector<billiards::Event> m_events;
    std::size_t m_heard = 0; // events already sounded
    billiards::Shot m_shot;  // the stroke being played or about to be
    Stage m_stage = Stage::Ready;
    int m_mode = VsComputer;
    int m_level = 3;
    int m_theme = 0;
    int m_game = 0;
    int m_aimLines = AimFull;
    int m_breaker = 0; // who breaks the next game
    std::mt19937_64 m_random;
    std::vector<Snapshot> m_undo;
    std::vector<HistoryLine> m_history;
    int m_shots = 0;
    float m_rollClock = 0;
    float m_overClock = 0;
    TableView m_view;

    // The aim.
    int m_struck = 0;
    billiards::V2 m_aim{1, 0};
    float m_power = 0.4f; // 0 to 1
    float m_side = 0, m_vertical = 0;
    int m_called = -1;
    bool m_calledFixed = false;
    billiards::Prediction m_prediction;
    bool m_predictionDirty = true;
    float m_pull = 0; // the cue drawn back from the ball, metres
    float m_charge = 0; // the power while the mouse draws the cue back

    // The stroke.
    float m_strokeClock = 0;
    float m_strokeFrom = 0, m_strokeTo = 0;
    float m_strokeBack = 0, m_strokeHold = 0, m_strokeForward = 0.08f;
    float m_follow = 10; // seconds since the cue met the ball
    billiards::V2 m_strikeSpot; // where the struck ball was

    // The computer.
    billiards::Worker<billiards::Shot> m_computer;
    float m_thinkClock = 0;
    std::optional<billiards::Shot> m_computerShot;
    float m_presentClock = 0;
    billiards::V2 m_presentSpot, m_presentAim;
    float m_presentPower = 0, m_presentSide = 0, m_presentVertical = 0;

    // The view and the input.
    TableCamera m_camera;
    Pointer m_pointer;
    Gesture m_gesture = Gesture::None;
    float m_pressX = 0, m_pressY = 0;
    float m_dragX = 0, m_dragY = 0;
    float m_aimX = -1, m_aimY = -1; // the pointer when it last aimed
    float m_turnX = -1, m_turnY = -1; // the pointer when the turn began
    bool m_pointerAims = false;       // it has moved since, and the aim follows it
    billiards::V2 m_grabOffset;
    int m_hoverBall = -1;
    int m_hoverPocket = -1;
    bool m_cursorHand = false;
    bool m_trackingLeave = false;

    // The interface.
    int m_activeWidget = 0;
    bool m_panelVisible = true;
    float m_panelSlide = 1;
    bool m_helpVisible = false;
    float m_historyScroll = 0;
    bool m_followHistory = true;
    float m_bannerFade = 0;
    float m_bannerTime = 0; // left to show; below zero it stays
    std::string m_bannerTitle, m_bannerDetail;
    std::vector<Toast> m_toasts;
};
