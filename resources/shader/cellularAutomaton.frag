#version 330 core

/* Texture holding minimal version of grid state
(build state and health as 32 bit unsigned integers) */
uniform usampler2D inputGrid;

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

#define UINT_MAXVAL 0xFFFFFFFFU //4294967295U

/* Max/min health (should match GridCell::MAX/MIN_HEALTH) */
#define MAX_HEALTH 100U
#define MIN_HEALTH 0U

/* Pixel position aka. cell position in input grid */
in vec2 pixel;

/* Pixel size aka. cell size (for stepping to neighbors) */
uniform vec2 pxsize;

/* Output state, i.e. accumulated effect on the current cell */
out uvec4 outputCell; // written to another texture

/* Function for lookup current neighborhood from the input grid */
void lookupNeighborhood8();
uvec2 neighborhood[8]; // neighborhood states are returned here
#define N 0 // indices of neighbors for each direction
#define NE 1
#define E 2
#define SE 3
#define S 4
#define SW 5
#define W 6
#define NW 7

/* Functions for analyzing neighborhood configurations */
int countNeighborsWithState(uint st);
ivec4 countNeighborsWithStateDirected(uint st);
int countNeighborsInRangeWithState(uint state, ivec2 start, ivec2 end);
ivec4 countNeighborsInRangeWithStateDirected(uint state, ivec2 start, ivec2 end);
bool isStateInNeighborhood(uint state);

/* Function for lookup state in given grid at given cell */
uvec2 lookup(usampler2D grid, vec2 cell) {
    return texture(grid,cell).rg;
}

/* Function for saving the result to the other texture */
void setOutput(uint buildState, int signedHealth) {
    // Clamp health and convert to unsigned
    uint healthPoints = uint(clamp(signedHealth, int(MIN_HEALTH), int(MAX_HEALTH)));
    // Infectedness is intended for filtered texture access in rendering later
    uint infectedness = ((buildState & INFECTED) > 0U) ? UINT_MAXVAL : 0U;
    // Store normalized health in A channel
    uint hpUNORM = uint(float(healthPoints) / float(MAX_HEALTH) * float(UINT_MAXVAL));
    outputCell = uvec4(buildState, healthPoints, infectedness, hpUNORM);
}

/* Simulation parameters */
/* adapted from http://www2.econ.iastate.edu/tesfatsi/SandPileModel.pdf */
// threshold where the fluid begins to flow
uniform int CRITICAL_VALUE; //TODO test whether uint works with unary minus
// amount of fluid a lower cell receives from collapsing neighbor
uniform uint FLOW_SPEED;
//TODO Think about avalanches. Will it work with a parallel setup?

const uint INFECTABLE = INSIDE_ROOM | WALL | CORNER;




/******************** MAIN *************************/
void main() {

    // BACKGROUND STORY:
    // a room got attacked and is now infected
    // through a source in a wall fluid comes in
    // the fluid slowly damages the room
    // the player tries to repair...

    uvec2 cell = lookup(inputGrid, pixel);

    uint bstate = cell[0];
    uint health = cell[1];
    uint fluid = MAX_HEALTH - health;

    // CASE 1A: cell is SOURCE (& WALL)
    if((bstate & SOURCE) > 0U) {
        // constantly decrease health, while player repairs
        setOutput(bstate | INFECTED, int(health) - int(FLOW_SPEED));
        return;
    }

    // CASE 1B: cell is REPAIRING
    if((bstate & REPAIRING) > 0U) {
        // pass on the already increased health, remove REPAIRING state and remove INFECTED state if fully repaired
        uint newState = bstate & ~REPAIRING;
        if(health >= MAX_HEALTH) {
            newState = newState & ~INFECTED;
        }
        setOutput(newState, int(health));
        return;
    }

    // looking at 4 directions and 2 neighbors for each => all 8 neighbors
    ivec2[4] FLOW_DIRECTION;
    FLOW_DIRECTION[0] = ivec2(1,-1);
    FLOW_DIRECTION[1] = ivec2(1,0);
    FLOW_DIRECTION[2] = ivec2(1,1);
    FLOW_DIRECTION[3] = ivec2(0,1);
    
    // accumulating new health here
    int result = int(health);

    for(int i = 0; i < 4; i++) {

        uvec2 left_nbor = lookup(inputGrid, pixel - FLOW_DIRECTION[i] * pxsize);
        uvec2 right_nbor = lookup(inputGrid, pixel + FLOW_DIRECTION[i] * pxsize);

        uint left_bstate = left_nbor[0];
        uint left_health = left_nbor[1];
        uint left_fluid = MAX_HEALTH - left_health;

        uint right_bstate = right_nbor[0];
        uint right_health = right_nbor[1];
        uint right_fluid = MAX_HEALTH - right_health;

        // fluid gradient: positive=>incoming, negative=>outgoing flow
        int left_gradient = int(left_fluid) - int(fluid);
        int right_gradient = int(right_fluid) - int(fluid);

        // prevent flow to/from non-infected cells
        if((left_bstate & INFECTED) == 0U) left_gradient = 0;
        if((right_bstate & INFECTED) == 0U) right_gradient = 0;

        // CASE 2: cell is INFECTED
        if((bstate & INFECTED) > 0U) {
            // compute flow assuming that fluid flows "down hill"
            if(left_gradient+CRITICAL_VALUE > CRITICAL_VALUE) // incoming from left
                result -= int(FLOW_SPEED);
            else if(left_gradient-CRITICAL_VALUE < -CRITICAL_VALUE) // outgoing to left
                result += int(FLOW_SPEED);
            if(right_gradient+CRITICAL_VALUE > CRITICAL_VALUE) // incoming from right
                result -= int(FLOW_SPEED);
            else if(right_gradient-CRITICAL_VALUE < -CRITICAL_VALUE) // outgoing to right
                result += int(FLOW_SPEED);
        }

        // CASE 3: cell is INFECTABLE
        else if((bstate & INFECTABLE) > 0U) {
            // decide whether this cell gets infected
            if(left_gradient > CRITICAL_VALUE)
                result -= int(FLOW_SPEED);
            if(right_gradient > CRITICAL_VALUE)
                result -= int(FLOW_SPEED);
        }

    }
    
    // was there some flow that changed health of current cell?
    if(result != int(health)) {
        setOutput(bstate | INFECTED, result);
    } else {
        //CASE 4: cell is EMPTY or some room segment that can't get infected
        setOutput(bstate, int(health));
    }

}



/******************** HELPER FUNCTION IMPLEMENTATIONS *************************/

void lookupNeighborhood8() {
    neighborhood[N] = lookup(inputGrid, vec2(pixel.x, pixel.y+pxsize.y)).rg;
    neighborhood[NE] = lookup(inputGrid, vec2(pixel.x+pxsize.x, pixel.y+pxsize.y)).rg;
    neighborhood[E] = lookup(inputGrid, vec2(pixel.x+pxsize.x, pixel.y)).rg;
    neighborhood[SE] = lookup(inputGrid, vec2(pixel.x+pxsize.x, pixel.y-pxsize.y)).rg;
    neighborhood[S] = lookup(inputGrid, vec2(pixel.x, pixel.y-pxsize.y)).rg;
    neighborhood[SW] = lookup(inputGrid, vec2(pixel.x-pxsize.x, pixel.y-pxsize.y)).rg;
    neighborhood[W] = lookup(inputGrid, vec2(pixel.x-pxsize.x, pixel.y)).rg;
    neighborhood[NW] = lookup(inputGrid, vec2(pixel.x-pxsize.x, pixel.y+pxsize.y)).rg;
}

bool isStateInNeighborhood(uint state) {
    for(int i = 0; i < 8; i++) {
        if((neighborhood[i].r & state) > 0U)
            return true;
    }
    return false;
}

int countNeighborsWithState(uint st) {
    int cnt = 0;
    for(int i = 0; i < 8; i++) {
        if((neighborhood[i].r & st) > 0U) cnt++;
    }
    return cnt;
}

ivec4 countNeighborsWithStateDirected(uint st) {
    // x = number of neighbors in positive x direction
    // y = neighbors in positive y
    // z = neighbors in negative x
    // w = neighbors in negative y
    ivec4 cnt = ivec4(0);
    if((neighborhood[NE].r & st) > 0U) {
        cnt.x++;
        cnt.y++;
    }
    if((neighborhood[SE].r & st) > 0U) {
        cnt.x++;
        cnt.w++;
    }
    if((neighborhood[SW].r & st) > 0U) {
        cnt.z++;
        cnt.w++;
    }
    if((neighborhood[NW].r & st) > 0U) {
        cnt.z++;
        cnt.y++;
    }
    if((neighborhood[N].r & st) > 0U) cnt.y++;
    if((neighborhood[E].r & st) > 0U) cnt.x++;
    if((neighborhood[S].r & st) > 0U) cnt.w++;
    if((neighborhood[W].r & st) > 0U) cnt.z++;
    return cnt;
}

int countNeighborsInRangeWithState(uint state, ivec2 start, ivec2 end) {
    // start/end are distances from current cell in units of cells
    // distance is given in x and y direction
    // negative distance means left/below
    // positive distance means right/above
    int cnt = 0;
    for(int x = start.x; x <= end.x; x += 1) {
        for(int y = start.y; y <= end.y; y += 1) {
            if(x==0 && y==0) continue;
            uvec2 cell = lookup(inputGrid, pixel + vec2(x*pxsize.x, y*pxsize.y));
            if((cell.r & state) > 0U) cnt++;
        }
    }
    return cnt;
}

ivec4 countNeighborsInRangeWithStateDirected(uint state, ivec2 start, ivec2 end) {
    // x = number of neighbors in positive x direction
    // y = neighbors in positive y
    // z = neighbors in negative x
    // w = neighbors in negative y
    ivec4 cnt = ivec4(0);
    for(int x = start.x; x <= end.x; x += 1) {
        for(int y = start.y; y <= end.y; y += 1) {
            if(x==0 && y==0) continue;
            uint neighborState = lookup(inputGrid, pixel + vec2(x*pxsize.x, y*pxsize.y)).r;
            if(neighborState != state) continue;
            if(x > 0) cnt.x++;
            else if(x < 0) cnt.z++;
            if(y > 0) cnt.y++;
            else if(y < 0) cnt.w++;
        }
    }
    return cnt;
}