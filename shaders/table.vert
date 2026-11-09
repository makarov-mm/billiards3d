// The table, the floor and the cue: meshes placed by a matrix.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

uniform mat4 uModel;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUv;
out vec3 vLocal;

void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUv = aUv;
    vLocal = aPosition;
    gl_Position = uViewProjection * world;
}
