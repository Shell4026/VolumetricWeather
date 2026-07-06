#version 430

layout (location = 0) in vec3 verts;

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
    gl_Position = camera.proj * camera.view * pc.model * vec4(verts, 1.0);
}