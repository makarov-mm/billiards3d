#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace math;

namespace {

constexpr float MinPitch = 0.1f, MaxPitch = 1.5703f;
constexpr float OverviewPitch = 0.98f, BehindPitch = 0.3f;
constexpr float FieldOfView = 0.6f;
constexpr float MinAspect = 1.55f; // of the space for the table, below which the view widens
constexpr float Ease = 7.0f;

// The angle nearest to `current` that points the same way as `angle`.
float Unwrap(float angle, float current)
{
    while (angle - current > Pi)
        angle -= 2 * Pi;
    while (angle - current < -Pi)
        angle += 2 * Pi;
    return angle;
}

} // namespace

void TableCamera::SetTable(float width, float reach)
{
    m_width = width;
    m_reach = reach;
    ApplyMode();
    m_pose = m_goal;
}

float TableCamera::OverviewDistance() const
{
    return 2.45f * m_reach;
}

void TableCamera::SetMode(CameraMode mode)
{
    m_mode = mode;
    ApplyMode();
}

void TableCamera::Reset()
{
    ApplyMode();
}

void TableCamera::ApplyMode()
{
    switch (m_mode) {
    case CameraMode::Overview:
        m_goal.target = {0, 0, 0.04f * m_width};
        m_goal.yaw = Unwrap(0, m_goal.yaw);
        m_goal.pitch = OverviewPitch;
        m_goal.distance = OverviewDistance();
        break;
    case CameraMode::Top:
        m_goal.target = {0, 0, 0};
        m_goal.yaw = Unwrap(0, m_goal.yaw);
        m_goal.pitch = MaxPitch;
        m_goal.distance = 2.3f * m_reach;
        break;
    case CameraMode::Behind:
        m_goal.pitch = BehindPitch;
        m_goal.distance = 0.42f * m_reach + 0.5f;
        Follow(m_ball, {-std::sin(m_aimYaw), -std::cos(m_aimYaw)});
        break;
    }
}

void TableCamera::Follow(Vec3 ball, Vec2 aim, bool instant)
{
    m_ball = ball;
    if (Length(aim) < 1e-6f)
        return;
    m_aimYaw = std::atan2(-aim.x, -aim.y);
    if (m_mode != CameraMode::Behind)
        return;
    const float ahead = 0.16f * m_reach;
    const float length = Length(aim);
    m_goal.target = ball + Vec3{aim.x / length * ahead, 0, aim.y / length * ahead};
    m_goal.yaw = Unwrap(m_aimYaw, m_goal.yaw);
    if (m_goal.pitch > 0.75f)
        m_goal.pitch = BehindPitch;
    if (instant)
        m_pose = m_goal;
}

void TableCamera::Lift()
{
    if (m_mode != CameraMode::Behind)
        return;
    m_goal.pitch = std::max(m_goal.pitch, 0.62f);
    m_goal.distance = std::max(m_goal.distance * 1.3f, 0.9f * m_reach);
    m_goal.target = Lerp(m_goal.target, Vec3{0, 0, 0}, 0.35f);
}

void TableCamera::Rotate(float yawRadians, float pitchRadians)
{
    m_goal.yaw += yawRadians;
    m_goal.pitch = std::clamp(m_goal.pitch + pitchRadians, MinPitch, MaxPitch);
    if (m_mode == CameraMode::Top)
        m_mode = CameraMode::Overview;
}

void TableCamera::Zoom(float steps)
{
    m_goal.distance = std::clamp(m_goal.distance * std::pow(0.88f, steps), 0.3f * m_reach, 4.2f * m_reach);
}

void TableCamera::Update(float seconds)
{
    const float ease = 1 - std::exp(-seconds * Ease);
    m_pose.target = Lerp(m_pose.target, m_goal.target, ease);
    m_pose.yaw += (m_goal.yaw - m_pose.yaw) * ease;
    m_pose.pitch += (m_goal.pitch - m_pose.pitch) * ease;
    // The distance eases by ratio, so that zooming feels the same near and far.
    m_pose.distance *= std::pow(m_goal.distance / m_pose.distance, ease);
    m_pose.shift += (m_goal.shift - m_pose.shift) * ease;
    m_pose.space += (m_goal.space - m_pose.space) * ease;
}

bool TableCamera::Moving() const
{
    return Length(m_goal.target - m_pose.target) > 1e-4f || std::abs(m_goal.yaw - m_pose.yaw) > 1e-4f || std::abs(m_goal.pitch - m_pose.pitch) > 1e-4f ||
           std::abs(m_goal.distance / m_pose.distance - 1) > 1e-4f || std::abs(m_goal.shift - m_pose.shift) > 1e-4f ||
           std::abs(m_goal.space - m_pose.space) > 1e-4f;
}

Vec3 TableCamera::Eye() const
{
    const float horizontal = std::cos(m_pose.pitch);
    return m_pose.target + Vec3{std::sin(m_pose.yaw) * horizontal, std::sin(m_pose.pitch), std::cos(m_pose.yaw) * horizontal} * m_pose.distance;
}

Mat4 TableCamera::View() const
{
    // Looking straight down, "up" on the screen points away from the eye's side of the table.
    const float yaw = m_pose.yaw, pitch = m_pose.pitch;
    const Vec3 up = Normalize(Vec3{-std::sin(yaw) * std::sin(pitch), std::cos(pitch), -std::cos(yaw) * std::sin(pitch)});
    return Mat4::LookAt(Eye(), m_pose.target, up);
}

Mat4 TableCamera::Projection(float aspect) const
{
    const float widen = std::max(1.0f, MinAspect / (aspect * m_pose.space));
    Mat4 projection = Mat4::Perspective(2 * std::atan(std::tan(FieldOfView / 2) * widen), aspect, 0.01f * m_reach, 16.0f * m_reach);
    projection(0, 2) = -2 * m_pose.shift; // shift in normalized coordinates, which span 2
    return projection;
}

void TableCamera::Ray(float x, float y, float width, float height, Vec3& origin, Vec3& direction) const
{
    const Mat4 inverse = (Projection(width / std::max(height, 1.0f)) * View()).Inverse();
    const float nx = x / width * 2 - 1, ny = 1 - y / height * 2;
    const Vec3 nearPoint = inverse.TransformPoint({nx, ny, -1});
    const Vec3 farPoint = inverse.TransformPoint({nx, ny, 1});
    origin = nearPoint;
    direction = Normalize(farPoint - nearPoint);
}

bool TableCamera::ToScreen(Vec3 point, float width, float height, Vec2& screen) const
{
    const Vec4 clip = Projection(width / std::max(height, 1.0f)) * View() * Vec4{point.x, point.y, point.z, 1};
    if (clip.w <= 1e-6f)
        return false;
    screen = {(clip.x / clip.w + 1) / 2 * width, (1 - clip.y / clip.w) / 2 * height};
    return true;
}
