#version 430

in vec2 pixel;
uniform sampler2D tex;

const float near = 0.1;
const float far = 100.0;

out vec4 color;

float lin(float depth) {
	float z = depth * 2.0 - 1.0; // Back to NDC 
	return (2.0 * near * far) / (far + near - z * (far - near));	
}

void main() {
    float depth = texture(tex, pixel).r;
	color = vec4(depth);
}