#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uvs;

layout(set = 1, binding = 0) uniform UBO
{
	vec4 color;
} ubo;

void main() 
{
    outColor = ubo.color * vec4(uvs.x, 0.f, uvs.y, 1.0);
}