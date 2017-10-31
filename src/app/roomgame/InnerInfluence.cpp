#include "InnerInfluence.h"
#include "core/resources/GPUProgramManager.h"

namespace roomgame
{
    InnerInfluence::InnerInfluence(AutomatonUpdater* automatonUpdater, std::shared_ptr<InteractiveGrid> interactiveGrid, double transition_time) :
        GPUCellularAutomaton(automatonUpdater, interactiveGrid, transition_time), num_transitions_(0), NUM_DIRECTIONS_(8),
        FLOW_DIRECTION(new glm::ivec2[NUM_DIRECTIONS_]{
            { 1, -1 },
            { 1, 0 },
            { 1, 1 },
            { 0, 1 },
            { -1, 1 },
            { -1, 0 },
            { -1, -1 },
            { 0, -1 }
    }),
        FLOW_SPEED(DEFAULT_FLOW_SPEED),
        CRITICAL_VALUE(DEFAULT_CRITICAL_VALUE)
    {
    }

    InnerInfluence::~InnerInfluence() {
        delete[] FLOW_DIRECTION;
    }

    void InnerInfluence::init(viscom::GPUProgramManager mgr) {
        GPUCellularAutomaton::init(mgr);
        //uloc_FLOW_DIRECTION = shader_->getUniformLocation("FLOW_DIRECTION");
        uloc_FLOW_SPEED = shader_->getUniformLocation("FLOW_SPEED");
        uloc_CRITICAL_VALUE = shader_->getUniformLocation("CRITICAL_VALUE");
    }

    void InnerInfluence::transition() {
        if (GPUCellularAutomaton::isInitialized()) {
            glUseProgram(shader_->getProgramId());
            glUniform1ui(uloc_FLOW_SPEED, FLOW_SPEED);
            glUniform1i(uloc_CRITICAL_VALUE, CRITICAL_VALUE);
            GPUCellularAutomaton::transition();
            num_transitions_++;
        }
    }

    void InnerInfluence::Reset()
    {
        FLOW_SPEED = DEFAULT_FLOW_SPEED;
        CRITICAL_VALUE = DEFAULT_CRITICAL_VALUE;
    }
}
