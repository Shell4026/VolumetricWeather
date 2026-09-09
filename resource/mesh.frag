#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uvs;
layout(location = 1) in vec3 worldPos;
layout(location = 2) in vec3 worldNormal;
layout(location = 3) in vec3 viewPos;
layout(location = 4) in vec4 lightProjPos[4];
layout(set = 0, binding = 0) uniform Camera
{
	vec3 pos;
	mat4 view;
	mat4 proj;
} camera;
layout(set = 1, binding = 0) uniform UBO
{
	vec4 sun; // dir, illuminance
	
	float atmosphereRadius;
	float groundRadius;
	uint cascade;
	
	vec4 sliceLength;
	mat4 sunViewProj[4];
} ubo;
layout(set = 1, binding = 1) uniform sampler2D tex;
layout(set = 1, binding = 2) uniform sampler2DArrayShadow shadowMap;
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

float PCF(vec2 uv, uint cascade, float z, float bias)
{
	float shadow = 0.0;

	vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);

	const int radius = 2;

	int samples = 0;

	for (int x = -radius; x <= radius; ++x)
	{
		for (int y = -radius; y <= radius; ++y)
		{
			vec2 offset = vec2(x, y) * texelSize;

			shadow += texture(shadowMap, vec4(uv + offset, float(cascade), z - bias));

			++samples;
		}
	}

	return shadow / float(samples);
}

float SampleCascadeShadow(uint cascade, vec3 normal)
{
	const vec3 projCoord = lightProjPos[cascade].xyz / lightProjPos[cascade].w;

	vec2 uv = projCoord.xy * 0.5 + 0.5;
	uv.y = 1.0 - uv.y;

	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || projCoord.z < 0.0 || projCoord.z > 1.0)
	{
		return 1.0;
	}

	float NdotL = max(dot(normal, -ubo.sun.xyz), 0.0);

	float bias = max(0.005 * (1.0 - NdotL), 0.0005);

	return PCF(uv, cascade, projCoord.z, bias);
}

void main() 
{
	const vec3 normal = normalize(worldNormal);
	const vec3 diffuse = texture(tex, uvs).rgb * ubo.sun.w;
	const float diff = max(0.0, dot(normal, -ubo.sun.xyz));
	const vec3 up = vec3(0.0, 1.0, 0.0);
	if (dot(up, -ubo.sun.xyz) < 0.0)
	{
		outColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}
	
	const float viewDepth = -viewPos.z;
	
	float shadow = 1.f;
	if (viewDepth < ubo.sliceLength[ubo.cascade - 1])
	{
		uint cascade = 0;
		for (uint i = 0; i < ubo.cascade; ++i)
		{
			if (viewDepth > ubo.sliceLength[i])
				cascade = i + 1;	
		}
		shadow = SampleCascadeShadow(cascade, normal);
		if (cascade + 1 < ubo.cascade)
		{
			const float splitEnd = ubo.sliceLength[cascade];
			const float splitStart = (cascade == 0) ? 0.0 : ubo.sliceLength[cascade - 1];
			const float cascadeLength = splitEnd - splitStart;
			const float blendWidth = cascadeLength * 0.1;
			const float blendStart = splitEnd - blendWidth;
			if (viewDepth > blendStart)
			{
				const float blend = smoothstep(blendStart, splitEnd, viewDepth);
				const float nextShadow = SampleCascadeShadow(cascade + 1, normal);
				shadow = mix(shadow, nextShadow, blend);
			}
		}
	}
	const vec3 sunTransmittance = TransmittanceUsingLUT(worldPos, -ubo.sun.xyz);
	outColor = vec4((shadow * diff) * diffuse * sunTransmittance, 1.0);
}
