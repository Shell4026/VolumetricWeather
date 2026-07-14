#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uvs;
layout(location = 1) in vec3 worldPos;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec4 lightProjPos;

layout(set = 0, binding = 0) uniform Camera
{
	vec3 pos;
	mat4 view;
	mat4 proj;
} camera;
layout(set = 1, binding = 0) uniform UBO
{
	vec4 sun; // dir, illuminance
	mat4 sunViewProj;
} ubo;
layout(set = 1, binding = 1) uniform sampler2D tex;
layout(set = 1, binding = 2) uniform sampler2DShadow shadowMap;
void main() 
{
	const vec3 normal = normalize(worldNormal);
	
	vec3 diffuse = texture(tex, uvs).rgb;
	float diff = max(0.0, dot(normal, -ubo.sun.xyz));
	
	vec3 lightProjCoord = lightProjPos.xyz / lightProjPos.w;
	vec2 lightProjUV = (lightProjCoord.xy + 1.0) * 0.5;
	lightProjUV.y = 1.0 - lightProjUV.y;
	
	const float bias = max(
		0.005 * (1.0 - dot(normal, -ubo.sun.xyz)),
		0.0005
	); // 수직일수록 bias를 크게
	
	float shadow = texture(shadowMap, vec3(lightProjUV, lightProjCoord.z - bias));
	
    outColor = vec4(shadow * diff * diffuse, 1.0);
}