#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;

layout(set = 1, binding = 0) uniform sampler2D atmopshere;
layout(set = 1, binding = 1) uniform sampler2D opaque;

vec3 ACESFilm(vec3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() 
{
	const float exposure = 1.f;
	vec3 sky = texture(atmopshere, fragUV).xyz;
	vec4 opaque = texture(opaque, fragUV);
	
	vec3 hdrColor = sky * (1.0 - opaque.a) + opaque.xyz;
	hdrColor *= exposure;
	
    outColor = vec4(ACESFilm(hdrColor), 1.0);
}