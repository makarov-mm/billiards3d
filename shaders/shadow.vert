// The depth of the table, the balls and the cue as the key light sees them. The balls are drawn together.

layout(location = 0) in vec3 aPosition;

struct BallData {
    mat4 model;
    vec4 params;
    vec4 extra;
};

layout(std430, binding = 0) readonly buffer Balls {
    BallData uBalls[];
};

uniform mat4 uModel;
uniform int uInstanced;
uniform int uFirst;

void main()
{
    mat4 model = uInstanced != 0 ? uBalls[uFirst + gl_InstanceID].model : uModel;
    gl_Position = uLightViewProjection * model * vec4(aPosition, 1.0);
}
