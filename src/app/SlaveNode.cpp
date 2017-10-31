/**
 * @file   SlaveNode.cpp
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.11.25
 *
 * @brief  Implementation of the slave application node.
 */

#include "SlaveNode.h"
#include <imgui.h>

namespace viscom {

    SlaveNode::SlaveNode(ApplicationNodeInternal* appNode) :
        SlaveNodeInternal{ appNode }
    {
    }


    void SlaveNode::UpdateFrame(double t1, double t2) {
        ApplicationNodeImplementation::UpdateFrame(t1, t2);
    }

    void SlaveNode::Draw2D(FrameBuffer& fbo)
    {
        //updateManager_.ManageUpdates(deltaTime, false);
        // always do this call last!
        SlaveNodeInternal::Draw2D(fbo);
    }

    void SlaveNode::PostDraw() {
        ApplicationNodeImplementation::PostDraw();
    }

    SlaveNode::~SlaveNode() = default;

    /* Sync step 1: Slave nodes fetch the data shared by master node here 
     * (Order in decoding must be the same as in encoding on master)
    */
    void SlaveNode::DecodeData() {
        SlaveNodeInternal::DecodeData();
        sourceLightManager_->decode();
        outerInfluence_->MeshComponent->decode();
        meshpool_.decode();
        sgct::SharedData::instance()->readObj<glm::vec3>(&synchronized_grid_translation_);
        automatonUpdater_.decode();
        sgct::SharedData::instance()->readBool(&gameLostShared);
        sgct::SharedData::instance()->readInt32(&currentScoreShared);
        sgct::SharedData::instance()->readInt32(&highestScoreThisSessionShared);
    }

    /* Sync step 2: Slaves set their copies of cluster-wide variables to values received from master 
     * These copies are good because in contrast to SGCT shared objects they are not mutex locked on each access
    */
    void SlaveNode::UpdateSyncedInfo() {
        SlaveNodeInternal::UpdateSyncedInfo();
        sourceLightManager_->updateSyncedSlave();
        outerInfluence_->MeshComponent->updateSyncedSlave();
        meshpool_.updateSyncedSlave();
        grid_translation_ = synchronized_grid_translation_.getVal();
        automatonUpdater_.updateSyncedSlave();
        ConfirmCurrentState();
        gameLost_ = gameLostShared.getVal();
        currentScore = currentScoreShared.getVal();
        highestScoreThisSession = highestScoreThisSessionShared.getVal();
    }

    void SlaveNode::ConfirmCurrentState() const
    {
        sgct::Engine::instance()->transferDataToNode(
            &automatonUpdater_.automatonTransitionNr_,
            sizeof(int), 0,
            0);
    }

}
