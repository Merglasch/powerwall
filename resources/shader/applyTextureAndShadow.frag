#version 330 core

struct Material {
//    float alpha;
//    vec3 ambient;
//    float bumpMultiplier;
//    sampler2D bumpTex;
//    vec3 diffuse;
    sampler2D diffuseTex;
//    float refraction;
//    vec3 specular;
//    float specularExponent;
};

uniform Material material;

uniform sampler2D shadowMap;
uniform float time;

uniform int isDebugMode;

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoords;
in vec4 vPosLightSpace;

out vec4 color;

const float DEPTH_BIAS = 0.00001;

float visibility(vec3 thisFragment) {
	float v = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	for(int x = -1; x <= 1; x++)
		for(int y = -1; y <= 1; y++)
			if(texture(shadowMap, thisFragment.xy + vec2(x,y) * texelSize).r > thisFragment.z) v += 1.0;
	return v/9.0;
}

void main() {
	if(isDebugMode == 1) {
		color = vec4(1);
		return;
	}
	vec3 thisFragment = vPosLightSpace.xyz / vPosLightSpace.w * 0.5 + 0.5;

	color = texture(material.diffuseTex, vTexCoords);

	// color *= visibility(thisFragment);
	if(texture(shadowMap, thisFragment.xy).r < (thisFragment.z - DEPTH_BIAS))
		color *= 0.5;

}