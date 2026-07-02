#version 430

layout (location = 0) in vec3 verts;

void main() 
{
    gl_Position = vec4(verts, 1.0);
}