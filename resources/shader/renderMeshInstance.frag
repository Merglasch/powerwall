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

/* Interpolated vertex attributes */
in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoords;

/* Build state and health of this grid cell (not interpolated) */
flat in uint st;
flat in uint hp;

/* Build state bits */
#define EMPTY 0U
#define INSIDE_ROOM 1U
#define CORNER 2U
#define WALL 4U
#define TOP 8U
#define BOTTOM 16U
#define RIGHT 32U
#define LEFT 64U
#define INVALID 128U
#define SOURCE 256U
#define INFECTED 512U
#define OUTER_INFLUENCE 1024U
#define TEMPORARY 2048U
#define REPAIRING 4096U

/* Max health (should match GridCell::MAX_HEALTH) */
#define MAX_HEALTH 100U

/* Build state and health for the whole grid (bilinear interpolation enabled) */
uniform sampler2D curr_grid_state;
uniform sampler2D last_grid_state;

/* Coordinates of this fragment on the grid (in texture space) */
in vec2 cellCoords;

uniform float automatonTimeDelta;

/*Directional Lights Parts*/
uniform vec3 viewPos;

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

/*alternative Shader modes */
uniform int isDepthPass;
uniform int isDebugMode;

/*caustics*/
uniform float t_sec;
uniform sampler2D causticTex;

/*Output*/
out vec4 color;

void main() {
    // In depth pass do not need to do anything because depth is implicitly written
    if(isDepthPass == 1) return;

    // Debug mode draws white wireframe
    if(isDebugMode == 1) {
        color = vec4(1);
        return;
    }

    // Lighting
    // vec3 lightDir = normalize(vPosition - vec3(-10.0f, -10.0f, -10.0f));
    // float NdotL = clamp(dot(lightDir, normalize(vNormal)), 0.0f, 1.0f);
    
    // Normalized health can be a nice weight for colors/ some damage overlay
    float healthNormalized = float(hp) / float(MAX_HEALTH);

    // Lookup cell state of this fragment with bilinear interpolation enabled
    vec4 last = texture(last_grid_state, cellCoords).rgba;
    vec4 curr = texture(curr_grid_state, cellCoords).rgba;

    // When a mesh instance is rendered on a cell, it can be:
    // 1) a room segment (st has WALL, CORNER or INSIDE_ROOM bit set)
    // 2) an infected room segment (st has INFECTED bit set)
    // 3) an infection source, most likely in a wall (st has SOURCE bit set)
    // 4) an invalid room, that is too small or too big (st has INVALID bit set)
    // ...
    if((st & INFECTED) > 0U) {
        // Determining "infectedness" of this fragment 
        // by spatial and temporal interpolation between
        // infected (1.0) and not infected (0.0),
        // which is stored in the blue channel of the grid texture.
        float infectedness = mix(last.b, curr.b, automatonTimeDelta);
        if(infectedness < 0.7) discard;
        float fluid = mix(1.0 - last.a, 1.0 - curr.a, automatonTimeDelta);
        color = vec4(vec3(.0,.0,.5) + fluid * vec3(1.,.8,.9), 1);
        if((st & REPAIRING) > 0U) color += vec4(clamp(curr.a - .5, .0, 1.));
    }
    else if((st & TEMPORARY) > 0U) {
        float tmpAlpha = 0.5f;
        if((st & INVALID) > 0U) {
            color = vec4(1,0,0, tmpAlpha);
        } else {
            color = vec4(material.diffuse, tmpAlpha);
        }
    } else {
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
    }

    float time = t_sec;

    // caustic effect movement

	vec2 waterTexCoords = vPosition.xy / 18;
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
	vec3 ambient = light.ambient * mix(material.diffuse,material.ambient,0.5);
	vec3 diffuse = light.diffuse * diff * material.diffuse;
	vec3 specular = light.specular * spec * material.specular;
	return (ambient + diffuse + specular);
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
	vec3 ambient = lightProps.ambient * material.ambient;
	vec3 diffuse = lightProps.diffuse * diff * material.diffuse;
	vec3 specular = lightProps.specular * spec * material.specular;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
} 