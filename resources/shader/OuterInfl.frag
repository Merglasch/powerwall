#version 330 core

uniform float time;

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoords;
in vec4 gl_FragCoord;

layout(location = 0) out vec4 color;


void main() {
//    float mult = mod(int(time),10) * .1;
//	color.rgba = vec4(1.*(1-mult),1.*(mult),0.,1.);
	color.rgba = vec4(1.,0.1,0.1,1.);
}
