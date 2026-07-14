#version 430

layout (location = 0) in vec3 verts;
layout (location = 1) in vec2 uvs;
layout (location = 2) in vec3 normal;

layout(set = 1, binding = 0) uniform Camera
{
	vec3 pos;
	mat4 viewProj;
} camera;

layout(push_constant) uniform PushConstants
{
    mat4 model;
} pc;

void main() 
{
    gl_Position = camera.viewProj * pc.model * vec4(verts, 1.0);
}