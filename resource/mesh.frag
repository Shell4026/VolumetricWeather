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
	float atmosphereRadius;
	float groundRadius;
} ubo;
layout(set = 1, binding = 1) uniform sampler2D tex;
layout(set = 1, binding = 2) uniform sampler2DShadow shadowMap;
layout(set = 1, binding = 3) uniform sampler2D transmittanceLUT;

vec2 GetTransmittanceUV(vec3 samplePos, vec3 toSunDir)
{
	const vec3 PLANET_CENTER = vec3(0.0, -ubo.groundRadius, 0.0);

	const float sampleLength = length(samplePos - PLANET_CENTER);
	const float height = sampleLength - ubo.groundRadius;
	const vec3 up = (samplePos - PLANET_CENTER) / sampleLength;
	const float mu = dot(up, toSunDir);

	const float ar2 = ubo.atmosphereRadius * ubo.atmosphereRadius;
	const float gr2 = ubo.groundRadius * ubo.groundRadius;
	const float r = height + ubo.groundRadius;
	const float r2 = r * r;
	// 지평선에서 대기까지의 거리 H
	const float H = sqrt(ar2 - gr2);
	// 지평선 거리 rho
	const float rho = sqrt(r2 - gr2);

	const float dMin = ubo.atmosphereRadius - r; // 머리 위의 대기까지의 거리
	const float dMax = rho + H; // 지평선 너머의 대기까지의 거리
	float d = -r * mu + sqrt(r2 * (mu * mu - 1) + ar2); // 샘플에서 ray방향으로의 대기권까지의 거리
	d = max(d, 0.0);

	float u = (d - dMin) / (dMax - dMin); // 0(머리위) ~ 1(지평선)
	float v = rho / H; // 그냥 높이 비율 대신 제곱근 비율로 바뀐거
	v = 1.0 - v;
	return vec2(u, v);
}

vec3 TransmittanceUsingLUT(vec3 samplePos, vec3 dir)
{
	const vec2 size = vec2(textureSize(transmittanceLUT, 0));
	vec2 uv = GetTransmittanceUV(samplePos, dir);
	uv = (vec2(0.5) + uv * (size - vec2(1.0))) / size;
	return texture(transmittanceLUT, uv).rgb;
}

void main() 
{
	const vec3 normal = normalize(worldNormal);
	
	vec3 diffuse = texture(tex, uvs).rgb * ubo.sun.w;
	float diff = max(0.0, dot(normal, -ubo.sun.xyz));
	const vec3 up = vec3(0.0, 1.0, 0.0);
	if (dot(up, -ubo.sun.xyz) < 0.0)
	{
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	
	vec3 lightProjCoord = lightProjPos.xyz / lightProjPos.w;
	vec2 lightProjUV = (lightProjCoord.xy + 1.0) * 0.5;
	lightProjUV.y = 1.0 - lightProjUV.y;
	
	const float bias = max(
		0.005 * (1.0 - dot(normal, -ubo.sun.xyz)),
		0.0005
	); // 수직일수록 bias를 크게
	
	float shadow = texture(shadowMap, vec3(lightProjUV, lightProjCoord.z - bias));
	const vec3 sunTransmittance = TransmittanceUsingLUT(worldPos, -ubo.sun.xyz);
	
	outColor = vec4((shadow * diff) * diffuse * sunTransmittance, 1.0);
}
