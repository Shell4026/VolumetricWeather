#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uvs;
layout(location = 1) in vec3 worldPos;
layout(location = 2) in vec3 worldNormal;

layout(set = 0, binding = 0) uniform Camera
{
	vec3 pos;
	mat4 view;
	mat4 proj;
} camera;
layout(set = 1, binding = 0) uniform UBO
{
	vec4 sun; // dir, illuminance
} ubo;
layout(set = 1, binding = 1) uniform sampler2D tex;

void main() 
{
	const vec3 normal = normalize(worldNormal);
	//const vec3 viewDir = normalize(worldPos - camera.pos);
	
	vec3 diffuse = texture(tex, uvs).rgb;
	float diff = max(0.0, dot(normal, -ubo.sun.xyz));
	
    outColor = vec4(diff * diffuse, 1.0);
}