#version 430

layout (location = 0) in vec3 verts;
layout (location = 1) in vec2 uvs;
layout (location = 2) in vec3 normal;

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
} pc;

void main() 
{
    gl_Position = pc.mvp * vec4(verts, 1.0);
}