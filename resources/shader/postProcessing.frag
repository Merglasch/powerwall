#version 330 core
out vec4 FragColor;
  
in vec2 texCoord;

const float offset = 1.0 / 600.0;  

//float kernel5[25] = float[](
//	0.003765, 0.015019, 0.023792, 0.015019, 0.003765,
//	0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
//	0.023792, 0.094907, 0.150342, 0.094907, 0.023792,
//	0.015019, 0.059912, 0.094907, 0.059912, 0.015019,
//	0.003765, 0.015019, 0.023792, 0.015019, 0.003765
//);

float kernel3[9] = float[](
	0.024879, 0.107973, 0.024879,
	0.107973, 0.468592, 0.107973,
	0.024879, 0.107973, 0.024879
);

//float kernel[9] = float[](
//    1.0 / 16, 2.0 / 16, 1.0 / 16,
//    2.0 / 16, 4.0 / 16, 2.0 / 16,
//    1.0 / 16, 2.0 / 16, 1.0 / 16  
//);

vec2 offsets[9] = vec2[](
    vec2(-offset,  offset), // top-left
    vec2( 0.0f,    offset), // top-center
    vec2( offset,  offset), // top-right
    vec2(-offset,  0.0f),   // center-left
    vec2( 0.0f,    0.0f),   // center-center
    vec2( offset,  0.0f),   // center-right
    vec2(-offset, -offset), // bottom-left
	vec2( 0.0f,   -offset), // bottom-center
	vec2( offset, -offset)  // bottom-right    
);

uniform sampler2D screenTexture;

void main()
{ 
	//vec2 offsets5[25];
	//for(int i = 0; i<5;i++)
	//{
	//	for(int j = 0; j<5;j++)
	//	{
	//		offsets5[i*5+j] = vec2(offset*(j-2), offset*(2-i));
	//	}
	//}

	vec3 sampleTex[9];
    for(int i = 0; i < 9; i++)
    {
        sampleTex[i] = vec3(texture(screenTexture, texCoord.st + offsets[i]));
    }
    vec3 col = vec3(0.0);
    for(int i = 0; i < 9; i++)
        col += sampleTex[i] * kernel3[i];
    
    FragColor = vec4(col, 1.0);
}