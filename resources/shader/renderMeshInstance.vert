#version 330 core

#define M_PI 3.1415926535897932384626433832795
#define M_HALF_PI 1.5707963267948966192313216916397
#define M_THREE_OVER_TWO_PI 4.7123889803846898576939650749192

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

/* Build state and health for the whole grid (bilinear interpolation enabled) */
uniform sampler2D curr_grid_state;
uniform sampler2D last_grid_state;

// Vertex attribs
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;

// Instance attribs
layout(location = 3) in vec3 translation;
layout(location = 4) in float scale;
layout(location = 5) in uint buildState;
layout(location = 6) in uint health;

uniform mat4 subMeshLocalMatrix;
uniform mat3 normalMatrix;
uniform mat4 viewProjectionMatrix;

uniform vec2 gridDimensions;
uniform vec3 gridTranslation;
uniform float gridCellSize;
uniform float automatonTimeDelta;

uniform float t_sec;

out vec3 vPosition;
out vec3 vNormal;
out vec2 vTexCoords;

vec2 rotateZ_step90(float x, float y) {
    // Problem: One mesh can be used with different instance attributes for different build states
    // Very specific solution: Choose rotation for corners and walls
    //TODO Find more generic solution
    if((buildState & INFECTED) == INFECTED) {
        return vec2(x, y);
    }
    if((buildState & (LEFT | TOP | CORNER))==(LEFT|TOP|CORNER) ||
        (buildState & (LEFT | SOURCE))==(LEFT | SOURCE) ||
        (buildState & (LEFT | WALL))==(LEFT | WALL)){
            return vec2(-y, x);
    }
    else if((buildState & (RIGHT | TOP | CORNER))==(RIGHT|TOP|CORNER) ||
        (buildState & (TOP | SOURCE))==(TOP | SOURCE) ||
        (buildState & (TOP | WALL))==(TOP | WALL)){
            return vec2(x, y);
    }
    else if((buildState & (RIGHT | BOTTOM | CORNER))==(RIGHT|BOTTOM|CORNER) ||
        (buildState & (RIGHT | SOURCE))==(RIGHT | SOURCE) ||
        (buildState & (RIGHT | WALL))==(RIGHT | WALL)){
            return vec2(y, -x);
    }
    else {
        return vec2(-x, -y);
    }
}

flat out uint st;
flat out uint hp;
out vec2 cellCoords;
out vec2 causticCoords;

void main() {
    mat4 modelMatrix = mat4(0); // this fixed the glitch
    modelMatrix[3] = vec4(translation, 1);
    modelMatrix[0][0] = scale;
    modelMatrix[1][1] = scale;
    modelMatrix[2][2] = scale;

    st = buildState;
    hp = health;
    cellCoords = translation.xy + vec2(1, 1 + gridCellSize) - gridTranslation.xy;
    cellCoords += (texCoords.yx - 0.5) * gridCellSize;
    cellCoords /= gridDimensions;

    vec3 pos = position;

    if((buildState & INFECTED) > 0U) {
        /*
        const float WATER_WAVE_LENGTH = 5.0;
        const float WATER_WAVE_HEIGHT = 10.0;
        float WATER_WAVE_DIRECTION = cellCoords.x;
        modelMatrix[3][2] += ((1.0 + sin(t_sec * WATER_WAVE_DIRECTION * WATER_WAVE_LENGTH)) / WATER_WAVE_HEIGHT);
        */
        vec4 last = texture(last_grid_state, cellCoords).rgba;
        vec4 curr = texture(curr_grid_state, cellCoords).rgba;
        float fluid = mix(1.0 - last.a, 1.0 - curr.a, automatonTimeDelta);
        pos.y += fluid*3.0f;
    }

    vec4 posV4 = modelMatrix * subMeshLocalMatrix * vec4(rotateZ_step90(pos.x, -pos.z), pos.y, 1);
    vPosition = vec3(posV4);
    vNormal = vec3(rotateZ_step90(normal.x, -normal.z), normal.y); //TODO incorporate sin wave
    vTexCoords = texCoords;
    posV4 = viewProjectionMatrix * posV4;
    vec4 caustpos = viewProjectionMatrix * modelMatrix * subMeshLocalMatrix * vec4(pos.x, -pos.z, pos.y, 1);
    causticCoords = cellCoords + (caustpos - posV4).xy;
    gl_Position = posV4;
}