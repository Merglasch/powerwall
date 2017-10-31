#version 330 core

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

/* Max health (should match GridCell::MAX_HEALTH) */
#define MAX_HEALTH 100U

flat in vec2 fPosition;
flat in uint fBuildState;
flat in uint fHealthPoints;

out vec4 color;

void main()
{
	/*
	* This fragment shader color-codes buildstate and health of a cell
	*/

	if((fBuildState & INFECTED) > 0U) color = vec4(1,0,0,1);
	else color = vec4(1,1,1,1);
	color = color * (float(fHealthPoints) / MAX_HEALTH);
}