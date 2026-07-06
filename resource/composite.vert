#version 430

vec2 verts[4] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
	vec2(1.0, 1.0)
);
vec2 uvs[4] = vec2[]
(
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0)
);

layout(location = 0) out vec2 fragUV;

void main() 
{
    gl_Position = vec4(verts[gl_VertexIndex], 0.0, 1.0);
	fragUV = uvs[gl_VertexIndex];
}