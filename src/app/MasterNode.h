/**
 * @file   MasterNode.h
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.11.25
 *
 * @brief  Declaration of the ApplicationNodeImplementation for the master node.
 */

#pragma once


#include <mutex>
#include <vector>

#include "glm\gtx\quaternion.hpp"

#include "../app/ApplicationNodeImplementation.h"
#include "core\camera\ArcballCamera.h"
#ifdef WITH_TUIO
#include "core/TuioInputWrapper.h"
#endif
namespace roomgame
{
    class InnerInfluence;
}
using roomgame::InnerInfluence;

namespace viscom {

    enum inputType { INPUT_ADD, INPUT_UPDATE, INPUT_REMOVE };

    struct input {
        inputType type;
        float x, y;
        int id;
    };

    /* Roomgame controller class for master node
     * Holds the grid with all the build states
     * Uses cellular automaton to compute dynamic influence
     * Can modify the view by using camera
     * Handles all user input
    */
    class MasterNode final : public ApplicationNodeImplementation
    {


        /* Controls inner influence, i.e. room infection */
        std::shared_ptr<InnerInfluence> cellular_automaton_;

        /* Interaction modes: GRID: build rooms, CAMERA: move camera,
        AUTOMATON: define init state for testing automaton rules */
        enum InteractionMode { GRID, CAMERA, AUTOMATON } interaction_mode_;

    public:
        class TransitionMsg
        {
        public:
            TransitionMsg(int clientId, int transitionNr)
                : clientId(clientId),
                  transitionNr(transitionNr)
            {
            }

            int clientId;
            int transitionNr;
        };

        explicit MasterNode(ApplicationNodeInternal* appNode);
        virtual ~MasterNode() override;

        /* This SGCT stage is called after the OpenGL context has been created and is only called once.
        This callback must be set before the Engine is initiated to have any effect.
        During the other stages the callbacks can be set end re-set anytime. */
        void InitOpenGL() override;

        /* Master node sends shared data here.
        This SGCT stage is called before the data is synchronized across the cluster.
        Set the shared variables here on the master and the slaves will receive them during the sync stage.
        The actual SGCT sync stage distributes the shared data from the master to the slaves.
        The slaves wait for the data to be received before the rendering takes place. 
        The slaves also send messages to the server if there are any (console printouts, warnings and errors). 
        There are two callbacks that can be set here, one for the master and one for the slaves. 
        The master encodes and serializes the data and the slaves decode and de-serialize the data.*/
        void PreSync() override;

        void EncodeData() override;
        void UpdateSyncedInfo() override;

        void UpdateFrame(double, double) override;

        /* This SGCT stage draws the scene to the current back buffer (left, right or both). 
        This stage can be called several times per frame if multiple viewports and/or if stereoscopic rendering is active.*/
        void DrawFrame(FrameBuffer& fbo) override;
        bool DataTransferCallback(void * receivedData, int receivedLength, int packageID, int clientID) override;
        void Draw2D(FrameBuffer& fbo) override;

        /* This SGCT stage is called after the rendering is finalized. */
        void PostDraw() override;

        void CleanUp() override;

        /* Switch input modes by keyboard on master
        * [C] key down: camera control mode
        * [V] key hit: tilt camera 45 degrees
        * [S] key hit: start automaton and switch between outer influence and room placement
        * [D] key down: debug render mode
        */
        bool KeyboardCallback(int key, int scancode, int action, int mods) override;

        /* Mouse/touch controls camera and room-/outer influence placement
        * When in "place outer influence"-mode, click to place outer influence
        * When in camera mode, click and drag to move camera
        * When in grid mode, click and drag to build room
        */
        bool MouseButtonCallback(int button, int action) override;

        /* MousePosCallback constantly updates interaction targets with cursor position
        * Workaround for missing cursor position in MouseButtonCallback:
        * Interaction targets can use their last cursor position
        */
        bool MousePosCallback(double x, double y) override;

        /* Mouse scroll events are used to zoom, when in camera mode */
        bool MouseScrollCallback(double xoffset, double yoffset) override;

        /* Currently not used */
        bool CharCallback(unsigned int character, int mods) override;

        /* Please comment */
        bool AddTuioCursor(TUIO::TuioCursor *tcur) override;
        bool UpdateTuioCursor(TUIO::TuioCursor *tcur) override;
        bool RemoveTuioCursor(TUIO::TuioCursor *tcur) override;

        /* Please comment */
        bool handleInputBuffer();

        /* Please comment */
        glm::vec2 GetCirclePos(glm::vec2 center, float radius, int angle);
        int viewAngle = 125;
        float range = 10;
    private:
        /* Please comment */
        std::mutex mtx;
        std::vector<input> inputBuffer;
        
        int counter = 0;
        void reset();
        void resetPlaygroundValues();
        bool isGameLost();
        std::list<TransitionMsg> slaveTransitionNumbers_;
    };
}
