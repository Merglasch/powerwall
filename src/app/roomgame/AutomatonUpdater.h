#pragma once
#include <memory>
#include <functional>
#include "GridCell.h"
#include "MeshInstanceBuilder.h"

namespace roomgame
{
    class InteractiveGrid;
    class GPUCellularAutomaton;
    using GRID_STATE_ELEMENT = GLuint;

    /* Class for uploading/downloading gridstates to the cellular automaton on the gpu.
    * Adds possibility to receive changes
    * a) from user input by overriding buildAt() and
    * b) from cellular automaton by offering updateCell() function.
    */
    class AutomatonUpdater {
        // Delaying mesh instance updates allows to play smoother animations
        struct DelayedUpdate {
            unsigned int wait_count_; // number of transitions to wait
            GridCell* target_; // cell to update
            GLuint to_; // build state to set
            DelayedUpdate* next_; // next list element
            DelayedUpdate(unsigned int wait_count, GridCell* target, GLuint to) :
                wait_count_(wait_count), target_(target), to_(to), next_(0) {}
        };
        DelayedUpdate* delayed_update_list_;

        // Update grid only (called from cellular automaton)
        void updateGridAt(GridCell* c, GLuint state, GLuint hp);
        friend GPUCellularAutomaton; // allow private access


    public:
        GPUCellularAutomaton* automaton_;
        int automatonTransitionNr_ = 0;
        sgct::SharedFloat synchronized_automaton_transition_time_delta_;
        float automaton_transition_time_delta_;
        sgct::SharedBool synchronized_automaton_has_transitioned_;
        bool automaton_has_transitioned_;
        GLuint currGridStateTexID;
        GLuint lastGridStateTexID;
        // use vector although grid state is not dynamic because sgct provides no shared array
        sgct::SharedVector<roomgame::GRID_STATE_ELEMENT> synchronized_grid_state_;
        std::vector<roomgame::GRID_STATE_ELEMENT> grid_state_;

        // Update automaton (called from user input or outer influence through buildAt)
        void updateAutomatonAt(GridCell* c, GLuint state, GLuint hp);

        std::shared_ptr<MeshInstanceBuilder> meshInstanceBuilder_;
        std::shared_ptr<InteractiveGrid> interactiveGrid_;
        static const GLuint SIMULATED_STATE = GridCell::INFECTED;
        AutomatonUpdater();
        ~AutomatonUpdater();
        void setCellularAutomaton(GPUCellularAutomaton*);

        void onTransition();
        void updateMaster(double currentTimeInSec);
        void uploadGridStateToGPU(bool masterNode);
        void populateCircleAtLastMousePosition(int radius);

        void preSync() { // master
            synchronized_automaton_transition_time_delta_.setVal(automaton_transition_time_delta_);
            synchronized_automaton_has_transitioned_.setVal(automaton_has_transitioned_);
            synchronized_grid_state_.setVal(grid_state_);
        }
        void encode() { // master
            sgct::SharedData::instance()->writeFloat(&synchronized_automaton_transition_time_delta_);
            sgct::SharedData::instance()->writeBool(&synchronized_automaton_has_transitioned_);
            sgct::SharedData::instance()->writeVector(&synchronized_grid_state_);
        }
        void decode() { // slave
            sgct::SharedData::instance()->readFloat(&synchronized_automaton_transition_time_delta_);
            sgct::SharedData::instance()->readBool(&synchronized_automaton_has_transitioned_);
            sgct::SharedData::instance()->readVector(&synchronized_grid_state_);
        }
        void updateSyncedSlave() {
            automaton_transition_time_delta_ = synchronized_automaton_transition_time_delta_.getVal();
            bool oldVal = automaton_has_transitioned_;
            automaton_has_transitioned_ = synchronized_automaton_has_transitioned_.getVal();
            if (oldVal != automaton_has_transitioned_) {
                automatonTransitionNr_++;
                uploadGridStateToGPU(false);
            }
        }

    };
}
