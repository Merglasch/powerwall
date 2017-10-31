#version 330 core


// Speed of water shader
#define speed 0.6f

// the amount of shearing (shifting of a single column or row)
// 1.0 = entire screen height offset (to both sides, meaning it's 2.0 in total)
#define xDistMag 0.005
#define yDistMag 0.005

// cycle multiplier for a given screen height
// 2*PI = you see a complete sine wave from top..bottom
#define xSineCycles 6.28 
#define ySineCycles 6.28 

uniform sampler2D causticTex;

/*Directional Lights Parts*/

struct DirLight{
	vec3 direction;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

uniform DirLight dirLight;
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);

/*Influence and Source Lights*/
struct PointLight{
    vec3 position;  
};
struct PointLightProps{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
	
    float constant;
    float linear;
    float quadratic;

};
uniform PointLightProps outerInfLightProps;
uniform PointLightProps sourceLightProps;

#define NR_OUT_INF_LIGHTS 5  
uniform PointLight outerInfLights[NR_OUT_INF_LIGHTS];
#define MAX_NR_SOURCE_LIGHTS 15  
uniform PointLight sourceLights[MAX_NR_SOURCE_LIGHTS];
uniform int numSourceLights;
vec3 CalcPointLight(PointLightProps lightProps, PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir); 




/*Material attributes*/
struct Material {
    float alpha;
    vec3 ambient;
    float bumpMultiplier;
    sampler2D bumpTex;
    vec3 diffuse;
    sampler2D diffuseTex;
    float refraction;
    vec3 specular;
    float specularExponent;
};

uniform Material material;


uniform sampler2DShadow shadowMap;
uniform float time;

uniform int isDebugMode;

uniform vec3 viewPos;

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoords;
in vec4 vPosLightSpace;

out vec4 color;

const float DEPTH_BIAS = 0.01;

// Calc visibility of this fragment in lightspace using PCF, bias and Stratified Poisson Sampling

vec2 poissonDisk[4] = vec2[](
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
);

float random(vec3 seed, int i){
	vec4 seed4 = vec4(seed,i);
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

float visibility(vec3 thisFragment) {
	float v = 0.0;
	float dynBias = max(0.002 * (1.0 - dot(vNormal, -normalize(dirLight.direction))), 0.00090);
	for (int i=0;i<4;i++){
		int index = int(16.0*random(floor(vPosition.xyz*1000.0), i))%16;
		vec3 frPos = vec3(thisFragment.x,thisFragment.y,thisFragment.z);
		frPos.xy += poissonDisk[index]/700.0;
		frPos.z -= dynBias;
		v += texture(shadowMap, frPos);
	}
	return v/4.0;
}

void main() {
	if(isDebugMode == 1) {
		color = vec4(1);
		return;
	}

	vec3 norm = normalize(vNormal);
	vec3 viewDir = normalize(viewPos-vPosition);
	vec3 result = CalcDirLight(dirLight, norm, viewDir);
	for(int i = 0; i < NR_OUT_INF_LIGHTS; i++){
		result += CalcPointLight(outerInfLightProps,outerInfLights[i], norm, vPosition, viewDir);
	}
	for(int i = 0; i < numSourceLights; i++){
		result += CalcPointLight(sourceLightProps,sourceLights[i], norm, vPosition, viewDir); 
	}
    color = vec4(result,1);

    // caustic effect movement

    vec2 waterTexCoords = vPosition.xy/18;
	//waterTexCoords /= 2; // Use this to increase/decrease caustic texture size
	
    // the value for the sine has 2 inputs:
    // 1. the time, so that it animates.
    // 2. the y-row, so that ALL scanlines do not distort equally.
	float localtime = speed * time;
    float xAngle = localtime + waterTexCoords.y * ySineCycles;
    float yAngle = localtime + waterTexCoords.x * xSineCycles;

    vec2 distortOffset = 
        vec2(sin(xAngle), sin(yAngle)) * // amount of shearing
        vec2(xDistMag,yDistMag); // magnitude adjustment

	waterTexCoords += distortOffset; 	

    // distance calculation for fading
    float diste = 0.1 * exp((distance(vPosition,viewPos))*0.4); 

    // add caustics for the underwater effect
    color += texture2D(causticTex,waterTexCoords*10)*abs(distortOffset.x)*5 *(4/diste);

    // add underwater fog
    color += diste * vec4(-0.09f,-0.04f,-0.04f,1.0f) + vec4(0.0f,0.20f,0.20f,0.0f); 

}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir){
	vec3 lightDir = normalize(-light.direction);
	//diffuse
	float diff = max(dot(normal, lightDir),0.0);
	//specular
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.specularExponent);
	//combine
	vec3 ambient = light.ambient * material.diffuse;
	vec3 diffuse = light.diffuse * diff * material.diffuse;
	vec3 specular = light.specular * spec * material.specular;
	return ambient + (diffuse + specular) * visibility(vPosLightSpace.xyz / vPosLightSpace.w * 0.5 + 0.5);
}

vec3 CalcPointLight(PointLightProps lightProps, PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse 
    float diff = max(dot(normal, lightDir), 0.0);
    // specular 
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.specularExponent);
    // attenuation
    float distance    = length(light.position - fragPos);
    float attenuation = 1.0 / (lightProps.constant + lightProps.linear * distance + 
  			     lightProps.quadratic * (distance * distance));    
	//combine
	vec3 ambient = lightProps.ambient * material.diffuse;
	vec3 diffuse = lightProps.diffuse * diff * material.diffuse;
	vec3 specular = lightProps.specular * spec * material.specular;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return ambient + (diffuse + specular) * visibility(vPosLightSpace.xyz / vPosLightSpace.w * 0.5 + 0.5);
} 