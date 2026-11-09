// The balls, drawn together: each instance its place, its turn and its number.

layout(location = 0) in vec3 aPosition; // on the unit sphere
layout(location = 1) in vec3 aNormal;

struct BallData {
    mat4 model;  // the radius, the ball's turn and its place
    vec4 params; // x: number, y: 1 for the Russian set, z: opacity, w: highlight
    vec4 extra;  // x: height of the cloth under it
};

layout(std430, binding = 0) readonly buffer Balls {
    BallData uBalls[];
};

uniform int uFirst;

out vec3 vWorld;
out vec3 vNormal;
out vec3 vLocal;
flat out vec4 vParams;
flat out vec4 vExtra;
flat out vec3 vCenter;

void main()
{
    BallData ball = uBalls[uFirst + gl_InstanceID];
    vec4 world = ball.model * vec4(aPosition, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(ball.model) * aNormal;
    vLocal = aPosition;
    vParams = ball.params;
    vExtra = ball.extra;
    vCenter = ball.model[3].xyz;
    gl_Position = uViewProjection * world;
}
