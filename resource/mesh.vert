#version 430

layout(location = 0) in vec3 verts;
layout(location = 1) in vec2 uvs;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec2 fragUVs;
layout(location = 1) out vec3 worldPos;
layout(location = 2) out vec3 worldNormal;
layout(location = 3) out vec4 lightProjPos;

layout(set = 0, binding = 0) uniform Camera
{
	vec3 pos;
	mat4 view;
	mat4 proj;
	mat4 invView;
	mat4 invProj;
} camera;
layout(set = 1, binding = 0) uniform UBO
{
	vec4 sun; // dir, illuminance
	mat4 sunViewProj;
} ubo;
layout(push_constant) uniform PushConstants
{
    mat4 model;
} pc;

void main() 
{
	worldPos = (pc.model * vec4(verts, 1.0)).xyz;
    gl_Position = camera.proj * camera.view * vec4(worldPos, 1.0);
	fragUVs = uvs;
	worldNormal = (transpose(inverse(pc.model)) * vec4(normal, 0.0)).xyz;
	
	lightProjPos = ubo.sunViewProj * vec4(worldPos, 1.0);
}