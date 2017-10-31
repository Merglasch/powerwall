/**
 * @file   MasterNode.cpp
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.11.25
 *
 * @brief  Implementation of the master application node.
 */

#include "MasterNode.h"
#include <imgui.h>
#include "core/imgui/imgui_impl_glfw_gl3.h"
#include <iostream>
#include "roomgame/MeshInstanceBuilder.h"
#include "roomgame/InteractiveGrid.h"
#include "roomgame/RoomSegmentMeshPool.h"
#include "roomgame/RoomInteractionManager.h"
#include "roomgame\InnerInfluence.h"


namespace viscom {

    MasterNode::MasterNode(ApplicationNodeInternal* appNode) :
        ApplicationNodeImplementation{ appNode },
        interaction_mode_(InteractionMode::GRID)
    {
        //meshInstanceBuilder_ = std::make_shared<MeshInstanceBuilder>(&meshpool_);
        //interactiveGrid_ = std::make_shared<InteractiveGrid>(GRID_COLS_, GRID_ROWS_, GRID_HEIGHT_);
        //meshInstanceBuilder_->interactiveGrid_ = interactiveGrid_;
        //meshInstanceBuilder_->automatonUpdater_ = &automatonUpdater_;
        //roomInteractionManager_ = std::make_shared<RoomInteractionManager>();
        //roomInteractionManager_->interactiveGrid_ = interactiveGrid_;
        //roomInteractionManager_->meshInstanceBuilder_ = meshInstanceBuilder_;
        //roomInteractionManager_->automatonUpdater_ = &automatonUpdater_;
        //interactiveGrid_->roomInteractionManager_ = roomInteractionManager_;
        //automatonUpdater_.meshInstanceBuilder_ = meshInstanceBuilder_;
        //automatonUpdater_.interactiveGrid_ = interactiveGrid_;
        cellular_automaton_ = std::make_shared<roomgame::InnerInfluence>(&automatonUpdater_,interactiveGrid_,1.0);
    }

    MasterNode::~MasterNode() = default;


    void MasterNode::InitOpenGL() {
        ApplicationNodeImplementation::InitOpenGL();
        
        interactiveGrid_->loadShader(GetApplication()->GetGPUProgramManager()); // for viewing build states...
        interactiveGrid_->uploadVertexData(); // ...for debug purposes

        cellular_automaton_->init(GetApplication()->GetGPUProgramManager());
        outerInfluence_->Grid = interactiveGrid_;
        glm::vec3 gridPos = interactiveGrid_->grid_center_;
        glm::vec2 pos = GetCirclePos(glm::vec2(gridPos.y, gridPos.z), range, viewAngle);
        GetCamera()->SetPosition(glm::vec3(0, pos.x, pos.y));
        glm::quat lookDir = glm::toQuat(glm::lookAt(GetCamera()->GetPosition(), gridPos, glm::vec3(0, 1, 0)));
        GetCamera()->SetOrientation(lookDir);

    }

    /* Sync step 1: Master sets values of shared objects to the values of corresponding non-shared objects */
    void MasterNode::PreSync() {
        ApplicationNodeImplementation::PreSync();
        sourceLightManager_->preSync();
        outerInfluence_->MeshComponent->preSync();
        meshpool_.preSync();
        synchronized_grid_translation_.setVal(grid_translation_);
        automatonUpdater_.preSync();
        gameLostShared.setVal(gameLost_);
        currentScoreShared.setVal(currentScore);
        highestScoreThisSessionShared.setVal(highestScoreThisSession);
    }

    /* Sync step 2: Master sends shared objects to the central SharedData singleton
     * (Order in decoding on slaves must be the same as in encoding)
    */
    void MasterNode::EncodeData() {
        ApplicationNodeImplementation::EncodeData();
        sourceLightManager_->encode();
        outerInfluence_->MeshComponent->encode();
        meshpool_.encode();
        sgct::SharedData::instance()->writeObj<glm::vec3>(&synchronized_grid_translation_);
        automatonUpdater_.encode();
        sgct::SharedData::instance()->writeBool(&gameLostShared);
        sgct::SharedData::instance()->writeInt32(&currentScoreShared);
        sgct::SharedData::instance()->writeInt32(&highestScoreThisSessionShared);

    }

    /* Sync step 3: Master updates its copies of cluster-wide variables with data it just synced
     * (Slaves update their copies with the data they received)
     * These copies are good because in contrast to SGCT shared objects they are not mutex locked on each access
     * Note, that master does not need this update because its data is always up to date...
     * ... but it is currently done to access the same variables with the same code on master and slaves
    */
    void MasterNode::UpdateSyncedInfo() {
        ApplicationNodeImplementation::UpdateSyncedInfo();
        meshpool_.updateSyncedMaster();
    }

    /* This SGCT stage is called only once before each frame, regardless of the number of viewports */
    void MasterNode::UpdateFrame(double t1, double t2) {
        ApplicationNodeImplementation::UpdateFrame(t1, t2);

#ifdef WITH_TUIO
        if (!inputBuffer.empty()) {
            handleInputBuffer();
        }
#endif
        grid_translation_ = interactiveGrid_->getTranslation();
        automatonUpdater_.updateMaster(clock_.t_in_sec);
        updateManager_.ManageUpdates(min(clock_.deltat(), 0.25));
    }

    /* This SGCT stage draws the scene to the current back buffer (left, right or both).
    This stage can be called several times per frame if multiple viewports and/or if stereoscopic rendering is active.*/
    void MasterNode::DrawFrame(FrameBuffer& fbo) {
        ApplicationNodeImplementation::DrawFrame(fbo);

        glm::mat4 viewProj = GetCamera()->GetViewPerspectiveMatrix();
        outerInfluence_->ViewPersMat = viewProj;

        automatonUpdater_.interactiveGrid_->updateProjection(viewProj);
        fbo.DrawToFBO([&] {
            if (render_mode_ == RenderMode::DBG) interactiveGrid_->onFrame();
        });

    }

    bool MasterNode::DataTransferCallback(void* receivedData, int
        receivedLength, int packageID, int clientID)
    {
        int transNr;
        switch (packageID) {
        case 0:
        {
            transNr = *reinterpret_cast<int*>(receivedData);
            bool newSlave = true;
            for (auto msg = slaveTransitionNumbers_.begin(); msg != slaveTransitionNumbers_.end(); ++msg){
                if(msg->clientId==clientID)
                {
                    msg->transitionNr = transNr;
                    newSlave = false;
                    break;
                }
            }
            if (newSlave)
            {
                slaveTransitionNumbers_.push_back(TransitionMsg(clientID, transNr));
            }
        }
        return true;
        default: return false;
        }
    }

    void MasterNode::Draw2D(FrameBuffer& fbo) {
        counter++;
        if(counter>30)
        {
            gameLost_ = isGameLost();
            //currentScore = static_cast<int>(glm::round(
            //    (uninfectedCells+0.5f) / (uninfectedCells + infectedCells+1.0f) * 100.0f - 50.0f + uninfectedCells * 10.0f - sourceCells * 100.0f));
            currentScore = static_cast<int>(max(uninfectedCells*10.0f-infectedCells*5.0f-sourceCells*20.0f,0));
            if(currentScore > highestScoreThisSession)
            {
                highestScoreThisSession = currentScore;
            }
            counter = 0;
        }

        int maxPatrolTime = outerInfluence_->getMaxPatrolTime();
        int minPatrolTime = outerInfluence_->getMinPatrolTime();
        float outInfluenceSpeed = outerInfluence_->getBaseSpeed();
        float innerInfluenceTransition = (float) cellular_automaton_->getTransitionTime();
        int innerInfluenceFlowSpeed = cellular_automaton_->FLOW_SPEED;
        int innerInfluenceCriticalValue = cellular_automaton_->CRITICAL_VALUE;
        float repairPerClickValue = roomInteractionManager_->healAmount_;
        int currentPatrolTime = outerInfluence_->getCurrentPatrolTime();
        int patrolTime = outerInfluence_->getPatrolTime();
        int masterTransitionNr = automatonUpdater_.automatonTransitionNr_;
        float sunX = lightInfo->sun->direction.x;
        float sunY = lightInfo->sun->direction.y;
        float sunZ = lightInfo->sun->direction.z;

        fbo.DrawToFBO([&]() {
            ImGui::SetNextWindowPos(ImVec2(700, 60), ImGuiSetCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiSetCond_FirstUseEver);
            ImGui::GetIO().FontGlobalScale = 1.5f;
            if (ImGui::Begin("Roomgame Controls")) {
                //ImGui::SetWindowFontScale(2.0f);
                ImGui::Text("Interaction mode: %s", (interaction_mode_ == GRID) ? "GRID" :
                    ((interaction_mode_ == AUTOMATON) ? "AUTOMATON" : "CAMERA"));
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Settings"))
            {
                ImGui::Spacing();
                ImGui::Text("Outer Influence");
                ImGui::Spacing();
                ImGui::Text("Patrol Completion: (%i/ %i)",currentPatrolTime,patrolTime);
                ImGui::Spacing();
                if (ImGui::SliderInt("Max Patrol Time", &maxPatrolTime, minPatrolTime+1, 30, "%.0f%")) {
                    outerInfluence_->setMaxPatrolTime(maxPatrolTime);
                }
                ImGui::Spacing();
                if (ImGui::SliderInt("Min Patrol Time", &minPatrolTime, 2, maxPatrolTime-1, "%.0f%")) {
                    outerInfluence_->setMinPatrolTime(minPatrolTime);
                }

                ImGui::Spacing();
                if (ImGui::SliderFloat("Speed", &outInfluenceSpeed, 0.2f, 1.0f)) {
                    outerInfluence_->setBaseSpeed(outInfluenceSpeed);
                }                

                ImGui::Spacing();
                ImGui::Text("Inner Influence");
                ImGui::Spacing();
                if (ImGui::SliderFloat("Transition time", &innerInfluenceTransition, 0.2f, 10.0f)) {
                    cellular_automaton_->setTransitionTime(glm::clamp(innerInfluenceTransition, 0.2f, 10.0f));
                }
                ImGui::Spacing();
                if (ImGui::SliderInt("Flow Speed", &innerInfluenceFlowSpeed, 1, 40)) {
                    cellular_automaton_->FLOW_SPEED = (glm::clamp(innerInfluenceFlowSpeed, 1, 40));
                }
                ImGui::Spacing();
                if (ImGui::SliderInt("Critical Value", &innerInfluenceCriticalValue, 1, 100)) {
                    cellular_automaton_->CRITICAL_VALUE = (glm::clamp(innerInfluenceCriticalValue, 1, 100));
                }

                ImGui::Spacing();
                ImGui::Text("Repairs");
                ImGui::Spacing();
                if (ImGui::SliderFloat("Heal per click", &repairPerClickValue, 0.1f, 1.1f)) {
                    roomInteractionManager_->healAmount_ = glm::clamp(repairPerClickValue, 0.1f, 1.1f);
                }

                ImGui::Spacing();
                if (ImGui::Button("Reset Values")) {
                    resetPlaygroundValues();
                }
            }

            ImGui::Spacing();
            if (ImGui::Button("Reset Playground")) {
                resetPlaygroundValues();
                reset();
            }

            ImGui::Spacing();
            if(ImGui::CollapsingHeader("Transition Numbers"))
            {
                ImGui::Text("Master: %i", masterTransitionNr);
                for (auto msg = slaveTransitionNumbers_.begin(); msg != slaveTransitionNumbers_.end(); ++msg)
                {
                    ImGui::Text("Slave %i: %i", msg->clientId, msg->transitionNr);
                }
            }

            ImGui::Spacing();
            if(ImGui::CollapsingHeader("Sun Position"))
            {
                if (ImGui::SliderFloat("sunX", &sunX, -10.0f, 10.0f)) {
                    lightInfo->sun->direction.x = sunX;
                }
                ImGui::Spacing();
                if (ImGui::SliderFloat("sunY", &sunY, -10.0f, 10.0f)) {
                    lightInfo->sun->direction.y = sunY;
                }
                ImGui::Spacing();
                if (ImGui::SliderFloat("sunZ", &sunZ, -10.0f, 0.0f)) {
                    lightInfo->sun->direction.z = sunZ;
                }
            }


            ImGui::Spacing();
            gameLost_ ? ImGui::Text("Game Lost: yes"):ImGui::Text("Game Lost: no");

            ImGui::End();
        });

        ApplicationNodeImplementation::Draw2D(fbo);
    }

    void MasterNode::PostDraw() {
        ApplicationNodeImplementation::PostDraw();
    }

    void MasterNode::CleanUp() {
        interactiveGrid_->cleanup();
        cellular_automaton_->cleanup();
        ApplicationNodeImplementation::CleanUp();
    }

    glm::vec2 MasterNode::GetCirclePos(glm::vec2 center, float radius, int angle) {
        float x = center.x + radius*cos(glm::radians<float>(static_cast<float>(angle)));
        float y = center.y + radius*sin(glm::radians<float>(static_cast<float>(angle)));

        return glm::vec2(x, y);
    }

    /* Switch input modes by keyboard on master
     * [C] key down: camera control mode
     * [S] key hit: switch between interacting with automaton state or building rooms
     * [D] key down: debug render mode
    */
    bool MasterNode::KeyboardCallback(int key, int scancode, int action, int mods) {
#ifndef VISCOM_CLIENTGUI
        ImGui_ImplGlfwGL3_KeyCallback(key, scancode, action, mods);
        if (ImGui::GetIO().WantCaptureKeyboard) return true;
#endif
        if (ApplicationNodeImplementation::KeyboardCallback(key, scancode, action, mods))return true;


        // Keys switch input modes
        static InteractionMode mode_before_switch_to_camera = interaction_mode_;
        if (key == GLFW_KEY_C) {
            if (action == GLFW_PRESS) {
                mode_before_switch_to_camera = interaction_mode_;
                interaction_mode_ = InteractionMode::CAMERA;
            }
            else if (action == GLFW_RELEASE) {
                interaction_mode_ = (InteractionMode)mode_before_switch_to_camera;
            }
        }
        else if (interaction_mode_ == InteractionMode::CAMERA) {
            glm::vec3 gridPos = interactiveGrid_->grid_center_;

            int right = key == GLFW_KEY_D ? 1 : 0;
            int left = key == GLFW_KEY_A ? 1 : 0;
            int up = key == GLFW_KEY_W ? 1 : 0;
            int down = key == GLFW_KEY_S ? 1 : 0;
            int xAxis = right - left;
            int yAxis = up - down;
            GetCamera()->SetPosition(GetCamera()->GetPosition() + glm::vec3(xAxis, yAxis, 0)*0.1f);
            int upWards = key == GLFW_KEY_UP ? 1 : 0;
            int downWards = key == GLFW_KEY_DOWN ? 1 : 0;
            int rotate = upWards - downWards;
            if (rotate != 0) {
                viewAngle += 5 * rotate;
                viewAngle = glm::clamp(viewAngle, 90, 170);
                glm::vec2 pos = GetCirclePos(glm::vec2(gridPos.y, gridPos.z), range, viewAngle);
                GetCamera()->SetPosition(glm::vec3(0, pos.x, pos.y));
                std::cout << range << "/" << viewAngle << std::endl;
            }
            glm::quat lookDir = glm::toQuat(glm::lookAt(GetCamera()->GetPosition(), gridPos, glm::vec3(0, 1, 0)));
            GetCamera()->SetOrientation(lookDir);
        }
        else if (key == GLFW_KEY_D) {
            if (action == GLFW_PRESS) {
                render_mode_ = RenderMode::DBG;
            }
            else if (action == GLFW_RELEASE) {
                render_mode_ = RenderMode::NORMAL;
            }
        }
        else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
            if (interaction_mode_ == AUTOMATON) interaction_mode_ = InteractionMode::GRID;
            else interaction_mode_ = InteractionMode::AUTOMATON;
        }
        return true;
    }

    /* Mouse/touch control room placement
    * click to place rooms
    */
    bool MasterNode::MouseButtonCallback(int button, int action) {
#ifndef VISCOM_CLIENTGUI
        ImGui_ImplGlfwGL3_MouseButtonCallback(button, action, 0);
        if (ImGui::GetIO().WantCaptureMouse) return true;
#endif
        if(ApplicationNodeImplementation::MouseButtonCallback(button, action))return true;

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (interaction_mode_ == InteractionMode::GRID) {
                if (action == GLFW_PRESS) interactiveGrid_->onTouch(-1);
                else if (action == GLFW_RELEASE) interactiveGrid_->onRelease(-1);
            }
            else if (interaction_mode_ == InteractionMode::AUTOMATON) {
                if (action == GLFW_PRESS) automatonUpdater_.populateCircleAtLastMousePosition(1);
            }
            else if (interaction_mode_ == InteractionMode::CAMERA) {
//                camera_.HandleMouse(button, action, 0, this);
            }
            return true;
        }
        return false;
    }

    /* MousePosCallback constantly updates interaction targets with cursor position
     * Workaround for missing cursor position in MouseButtonCallback:
     * Interaction targets can use their last cursor position
    */
    bool MasterNode::MousePosCallback(double x, double y) {
#ifndef VISCOM_CLIENTGUI
        ImGui_ImplGlfwGL3_MousePositionCallback(x, y);
        if (ImGui::GetIO().WantCaptureMouse) return true;
#endif
        if (ApplicationNodeImplementation::MousePosCallback(x, y))return true;

        viscom::math::Line3<float> ray = GetCamera()->GetPickRay({ x,y });
        interactiveGrid_->onMouseMove(-1, ray[0], ray[1]);
        return true;
    }

    /* Mouse scroll events are used to zoom, when in camera mode */
    bool MasterNode::MouseScrollCallback(double xoffset, double yoffset) {

#ifndef VISCOM_CLIENTGUI
        ImGui_ImplGlfwGL3_ScrollCallback(xoffset, yoffset);
        if (ImGui::GetIO().WantCaptureMouse) return true;
#endif
        if (ApplicationNodeImplementation::MouseScrollCallback(xoffset, yoffset)) return true;

        if (interaction_mode_ == InteractionMode::CAMERA) {
            float change = (float)yoffset*0.1f;
            glm::vec3 camToGrid = GetCamera()->GetPosition() - interactiveGrid_->grid_center_;
            if (glm::length(camToGrid) > 0.5 || yoffset > 0) {
                GetCamera()->SetPosition(GetCamera()->GetPosition() + camToGrid*change);
                range = glm::distance(GetCamera()->GetPosition(), interactiveGrid_->grid_center_);
            }
//            camera_.HandleMouse(0, 0, (float)yoffset, this);
            return true;
        }
        return false;
    }

    bool MasterNode::CharCallback(unsigned int character, int mods) {
#ifndef VISCOM_CLIENTGUI
        ImGui_ImplGlfwGL3_CharCallback(character);
#endif
        return ApplicationNodeImplementation::CharCallback(character, mods);
    }

    bool MasterNode::AddTuioCursor(TUIO::TuioCursor* tcur)
    {
        // grid_.onTouch(-1); //TODO @Tobias: pass the CursorID here,
        // ID is saved in interaction object by RoomInteractionManager::StartNewRoomInteractionAtTouchedCell,
            // this object lives for the duration of one touch gesture
            // and is used to distinguish clicks/touches, to reuse IDs of accidentally interrupted touch gestures, etc.
            // (see RoomInteractionManager::AdjustTemporaryRoomSize + FinalizeTemporaryRoom + GridInteraction::testTemporalAndSpatialProximity)
#ifdef WITH_TUIO
        mtx.lock();
        inputBuffer.push_back(input{ INPUT_ADD,tcur->getX(),tcur->getY(), tcur->getCursorID() });
        mtx.unlock();
#endif
        //std::cout << "add    cur  " << tcur->getCursorID() << " (" << tcur->getSessionID() << "/" << tcur->getTuioSourceID() << ") " << tcur->getX() << " " << tcur->getY() << std::endl;
        return true;
    }

    bool MasterNode::UpdateTuioCursor(TUIO::TuioCursor* tcur)
    {
#ifdef WITH_TUIO
        mtx.lock();
        inputBuffer.push_back(input{ INPUT_UPDATE,tcur->getX(),tcur->getY(), tcur->getCursorID() });
        mtx.unlock();
#endif
        //std::cout << "set    cur  " << tcur->getCursorID() << " (" << tcur->getSessionID() << "/" << tcur->getTuioSourceID() << ") " << tcur->getX() << " " << tcur->getY()
        //    << " " << tcur->getMotionSpeed() << " " << tcur->getMotionAccel() << " " << std::endl;
        return true;
    }

    bool MasterNode::RemoveTuioCursor(TUIO::TuioCursor* tcur)
    {
#ifdef WITH_TUIO
        mtx.lock();
        inputBuffer.push_back(input{ INPUT_REMOVE,tcur->getX(),tcur->getY(), tcur->getCursorID() });
        mtx.unlock();
#endif
        //std::cout << "delete cur  " << tcur->getCursorID() << " (" << tcur->getSessionID() << "/" << tcur->getTuioSourceID() << ")" << std::endl;
        return true;
    }

    bool MasterNode::handleInputBuffer() {
#ifdef WITH_TUIO
        if (mtx.try_lock()) {
            //std::cout << "DEBUG: working on input with size " << inputBuffer.size() << std::endl;
            for (int i = 0; i < inputBuffer.size(); i++) {
                input tmp = inputBuffer.at(i);
                viscom::math::Line3<float> ray = GetCamera()->GetPickRay({ tmp.x,tmp.y });
                if (tmp.type == INPUT_UPDATE) {
                    interactiveGrid_->onMouseMove(tmp.id, ray[0], ray[1]);
                }
                else if (tmp.type == INPUT_ADD) {
                    interactiveGrid_->onMouseMove(tmp.id, ray[0], ray[1]);
                    interactiveGrid_->onTouch(tmp.id);
                }
                else if (tmp.type == INPUT_REMOVE) {
                    interactiveGrid_->onMouseMove(tmp.id, ray[0], ray[1]);
                    interactiveGrid_->onRelease(tmp.id);
                }
            }
            inputBuffer.clear();
            mtx.unlock();
            return true;
        }
        else {
            std::cout << "could not lock" << std::endl;
            return false;
        }
#endif
        return true;
    }

    void MasterNode::reset() {
        interactiveGrid_->forEachCell([&](GridCell *cell) {
            meshInstanceBuilder_->buildAt(cell->getCol(), cell->getRow(), GridCell::EMPTY, MeshInstanceBuilder::BuildMode::Replace);
            roomInteractionManager_->updateHealthPoints(cell, GridCell::MAX_HEALTH);
            roomInteractionManager_->reset();
        });
        sourceLightManager_->sourcePositions_.clear();
        gameLost_ = false;
    }

    void MasterNode::resetPlaygroundValues()
    {
        outerInfluence_->resetValues();
        cellular_automaton_->ResetTransitionTime();
        cellular_automaton_->Reset();
        roomInteractionManager_->ResetHealAmount();
    }

    bool MasterNode::isGameLost()
    {
        uninfectedCells = 0;
        infectedCells = 0;
        sourceCells = 0;
        if (roomInteractionManager_->getFirstRoom()) return false;
        bool lost = true;
        interactiveGrid_->forEachCell(static_cast<std::function<void(GridCell*)>>([&](GridCell *cell) {
            GLuint buildState = cell->getBuildState();
            if(buildState != GridCell::EMPTY)
            {
                if((buildState & (GridCell::INFECTED | GridCell::TEMPORARY | GridCell::INVALID)) == 0)
                {
                    uninfectedCells++;
                    if((buildState & GridCell::CORNER)!=0)
                    {
                        uninfectedCells += 4;
                    }
                    lost = false;
                }
                if((buildState & GridCell::INFECTED)!=0)
                {
                    if((buildState & GridCell::SOURCE)!=0)
                    {
                        sourceCells++;
                    }
                    infectedCells++;
                }
            }
        }));
        return lost;
    }
}