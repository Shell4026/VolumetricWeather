#version 430

layout (location = 0) in vec3 verts;
layout (location = 1) in vec2 uvs;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec2 fragUVs;

layout(set = 0, binding = 0) uniform Camera
{
	vec3 pos;
	mat4 view;
	mat4 proj;
} camera;

layout(push_constant) uniform PushConstants
{
    mat4 model;
} pc;

void main() 
{
	vec4 worldPos = pc.model * vec4(verts, 1.0);
    gl_Position = camera.proj * camera.view * worldPos;
	fragUVs = uvs;
}