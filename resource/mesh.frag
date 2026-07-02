#version 450

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO
{
	vec4 color;
} ubo;

void main() 
{
    outColor = ubo.color;
}