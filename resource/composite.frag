#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;

layout(set = 1, binding = 0) uniform sampler2D atmopshere;

void main() 
{
    outColor = texture(atmopshere, fragUV);
}