#pragma once

#include "MathTypes.h"

enum class CameraMode { Overview, Behind, Top };
constexpr int CameraModeCount = 3;

// A camera circling a point over the table. Its moves are eased: dragging, zooming and the views set where it
// heads. From behind the ball it looks along the aim; over the table it takes it in whole, at an angle or from
// straight above.
class TableCamera {
public:
    // The table's width between the cushions and how far the rails reach from its middle along its length.
    void SetTable(float width, float reach);
    void SetMode(CameraMode mode);
    CameraMode Mode() const { return m_mode; }
    // In the view from behind: the ball struck and the direction of the aim, looked along.
    void Follow(math::Vec3 ball, math::Vec2 aim, bool instant = false);
    // While the balls roll, the view from behind rises to see more of the table.
    void Lift();
    void Reset();
    void Rotate(float yawRadians, float pitchRadians);
    void Zoom(float steps);
    // The share of the window's width left for the table beside the panel: the view centers on it, and widens
    // when it is narrow, so the whole table stays in sight.
    void SetSpace(float share)
    {
        m_goal.shift = (share - 1) / 2;
        m_goal.space = share;
    }

    void Update(float seconds);
    bool Moving() const;

    math::Vec3 Eye() const;
    math::Mat4 View() const;
    math::Mat4 Projection(float aspect) const;
    // The ray through a point of the window, in pixels from the top left.
    void Ray(float x, float y, float width, float height, math::Vec3& origin, math::Vec3& direction) const;
    // A point of the scene in pixels from the window's top left; false behind the camera.
    bool ToScreen(math::Vec3 point, float width, float height, math::Vec2& screen) const;

private:
    struct Pose {
        math::Vec3 target;
        float yaw = 0, pitch = 1, distance = 4;
        float shift = 0, space = 1;
    };

    void ApplyMode();
    float OverviewDistance() const;

    CameraMode m_mode = CameraMode::Overview;
    float m_width = 1.27f, m_reach = 1.45f;
    Pose m_pose, m_goal;
    math::Vec3 m_ball;
    float m_aimYaw = 0;
};
