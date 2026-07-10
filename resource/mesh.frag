#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uvs;

layout(set = 1, binding = 0) uniform UBO
{
	vec4 color;
} ubo;
layout(set = 1, binding = 1) uniform sampler2D tex;

void main() 
{
    outColor = ubo.color * texture(tex, uvs);
}