#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;

uniform mat4 subMeshLocalMatrix;
uniform mat3 normalMatrix;
uniform mat4 viewProjectionMatrix;
uniform mat4 lightSpaceMatrix;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoords;
out vec4 vPosLightSpace;
out vec2 fragCoords;

void main() {
	vec4 posV4 = subMeshLocalMatrix * vec4(position.x, -position.z, position.y, 1);
	vPosition = vec3(posV4);
	vNormal = normalize(normalMatrix * normal);
	vTexCoords = texCoords;
	vPosLightSpace = lightSpaceMatrix * posV4;

	fragCoords = position.xy;

	gl_Position = viewProjectionMatrix * posV4;
}

