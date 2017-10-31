#pragma once

#include "GPUBuffer.h"
#include <memory>
#include "GridCell.h"

namespace viscom {
    class GPUProgramManager;
    class GPUProgram;
}

namespace roomgame {
    class AutomatonUpdater;
    class InteractiveGrid;
    /* Minimal version of grid state
     * Channel R: build state as UINT bitfield
     * Channel G: health as UINT
     * Channel B: is-infected as UNORM
     * Channel A: health as UNORM
    */
    const unsigned int GRID_STATE_TEXTURE_CHANNELS = 4;
    const GPUBuffer::Tex GRID_STATE_TEXTURE = { 0, 0, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT };
    const GPUBuffer::Tex FILTERABLE_GRID_STATE_TEXTURE = { 0, 0, GL_RGBA32F, GL_RGBA, GL_UNSIGNED_INT };
    using GRID_STATE_ELEMENT = GLuint;
    //TODO reduce bits per channel for transferring less data during sync (could be major bottleneck)

    /* Implementation of a parallelized cellular automaton with interactive grid.
    * Construct with grid and time between transitions.
    * Call init() once to setup GPU state and copy state to texture.
    * Call transition(time) as often as pleased with current time.
    * transition(time) returns immediately if it is not time yet.
    * Else it applies rules in shader (see cellularAutomaton.frag).
    * After each transition:
    *  1. Notifies grid by calling onTransition().
    *  2. Copies results to grid (updating only changed cells).
    * State changes to the grid can also occur on user input.
    *  => When user changed something, grid calls updateCell on automaton.
    */
    class GPUCellularAutomaton {
        double DEFAULT_TRANSITION_TIME = 3.0f;
    protected:
        std::shared_ptr<viscom::GPUProgram> shader_;
    private:
        GPUBuffer* framebuffer_pair_[2]; // two images for "double buffering", i.e...
        GPUBuffer::Tex texture_pair_[2]; // ... reading from one while writing to other
        int current_read_index_;
        roomgame::GRID_STATE_ELEMENT* tmp_client_buffer_; // temporary grid state buffer for GPU-CPU transfer
        size_t sizeof_tmp_client_buffer_;
        GLuint vao_; // holds screenfilling quad
        bool is_initialized_; // true if quad and framebuffers are ready
        GLint pixel_size_uniform_location_;
        GLint texture_uniform_location_;
        glm::vec2 pixel_size_;
        double transition_time_;
        double last_time_;
        double delta_time_;
        void copyFromGridToTexture(int tex_index);
        void copyFromTextureToGrid(int tex_index);
    public:

        AutomatonUpdater* automatonUpdater_;
        std::shared_ptr<InteractiveGrid> interactiveGrid_;
        void updateCell(GridCell* c, GLuint state, GLuint hp);
        bool checkForTransitionTexSwapWithDeltaReset(double time, bool oldVal);
        virtual void init(viscom::GPUProgramManager mgr);
        virtual void transition();
        GPUCellularAutomaton(AutomatonUpdater* automatonGrid_grid,
                             std::shared_ptr<InteractiveGrid> interactiveGrid, double transition_time);
        void cleanup();
        //Setter
        void setTransitionTime(double);

        //Getter
        GLfloat getTimeDeltaNormalized();
        GLuint getLatestTexture();
        GLuint getPreviousTexture();
        bool isInitialized();
        size_t getGridBufferSize(); // "grid buffer" refers to automaton state storage
        size_t getGridBufferElements();
        roomgame::GRID_STATE_ELEMENT* getGridBuffer();
        double getTransitionTime();
        int getCurrentReadIndex() { return current_read_index_; }

        //reset
        void ResetTransitionTime();
    };

}

