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

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoords;

out vec4 color;

void main() {
	color = texture(material.diffuseTex, vTexCoords);
}