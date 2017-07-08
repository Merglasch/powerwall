/**
 * @file   ApplicationNodeImplementation.cpp
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.11.30
 *
 * @brief  Implementation of the application node class.
 */

#include "ApplicationNodeImplementation.h"
#include "Vertices.h"
#include <imgui.h>
#include "core/gfx/mesh/MeshRenderable.h"
#include "core/imgui/imgui_impl_glfw_gl3.h"
#include <iostream>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace viscom {

	ApplicationNodeImplementation::ApplicationNodeImplementation(ApplicationNodeInternal* appNode) :
		ApplicationNodeBase{ appNode },
		GRID_COLS_(64), GRID_ROWS_(64), GRID_HEIGHT_NDC_(2.0f),
		meshpool_(GRID_COLS_ * GRID_ROWS_),
		render_mode_(NORMAL),
		clock_{ 0.0 },
		camera_matrix_(1.0f),
		updateManager_()
    {
		outerInfluence_ = std::make_shared<roomgame::OuterInfluence>();
    }

    ApplicationNodeImplementation::~ApplicationNodeImplementation() = default;

    void ApplicationNodeImplementation::InitOpenGL()
    {
		/* Load resources on all nodes */
		meshpool_.loadShader(GetApplication()->GetGPUProgramManager());
		meshpool_.addMesh({ GridCell::BuildState::INSIDE_ROOM },
                            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/floor.obj"));
		meshpool_.addMesh({ GridCell::BuildState::LEFT_LOWER_CORNER,
							GridCell::BuildState::LEFT_UPPER_CORNER,
							GridCell::BuildState::RIGHT_LOWER_CORNER,
							GridCell::BuildState::RIGHT_UPPER_CORNER,
							GridCell::BuildState::INVALID },
                            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/corner.obj"));
		meshpool_.addMesh({ GridCell::BuildState::WALL_BOTTOM,
							GridCell::BuildState::WALL_TOP,
							GridCell::BuildState::WALL_RIGHT,
							GridCell::BuildState::WALL_LEFT },
                            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/wall.obj"));
		meshpool_.addMesh({ GridCell::BuildState::INSIDE_ROOM_INFECTED },
			GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/latticeplane.obj"));
//TODO		meshpool_.addMesh({ GridCell::BuildState::OUTER_INFLUENCE },
//			GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/thingy.obj"));

		SynchronizedGameMesh* outerInfluenceMeshComp = new SynchronizedGameMesh(GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/thingy/thingy.obj"), GetApplication()->GetGPUProgramManager().GetResource("applyTextureAndShadow",
			std::initializer_list<std::string>{ "applyTextureAndShadow.vert", "applyTextureAndShadow.frag" }));
		outerInfluence_->meshComponent = outerInfluenceMeshComp;
		outerInfluence_->meshComponent->transform(glm::scale(glm::mat4(1), glm::vec3(0.1, 0.1, 0.1)));


		meshpool_.updateUniformEveryFrame("t_sec", [this](GLint uloc) {
			glUniform1f(uloc, (float)clock_.t_in_sec);
		});

		
		backgroundMesh_ = new ShadowReceivingMesh(
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/textured_4vertexplane/textured_4vertexplane.obj"),
            GetApplication()->GetGPUProgramManager().GetResource("applyTextureAndShadow",
				std::initializer_list<std::string>{ "applyTextureAndShadow.vert", "applyTextureAndShadow.frag" }));
		backgroundMesh_->transform(glm::scale(glm::translate(glm::mat4(1), 
			glm::vec3(
				0,
				-(GRID_HEIGHT_NDC_/GRID_ROWS_), /* position background mesh exactly under grid */
				-0.001f/*TODO better remove the z bias and use thicker meshes*/)), 
			glm::vec3(1.0f)));

		shadowMap_ = new ShadowMap(1024, 1024);
		shadowMap_->setLightMatrix(glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0), glm::vec3(0, 1, 0)));
        GetApplication()->GetEngine()->setNearAndFarClippingPlanes(0.1f, 100.0f);

		/*Set Up the camera*/
		GetCamera()->SetPosition(glm::vec3(0, 0, 0));

		updateManager_.AddUpdateable(outerInfluence_);
    }


    void ApplicationNodeImplementation::UpdateFrame(double currentTime, double elapsedTime)
    {
		deltaTime = min(currentTime - oldTime,0.25);
		clock_.t_in_sec = currentTime;
		oldTime = currentTime;
    }

    void ApplicationNodeImplementation::ClearBuffer(FrameBuffer& fbo)
    {
        fbo.DrawToFBO([]() {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        });

		shadowMap_->DrawToFBO([&]() {
			glClearDepth(1.0f);
			glClear(GL_DEPTH_BUFFER_BIT);
		});
    }

    void ApplicationNodeImplementation::DrawFrame(FrameBuffer& fbo)
    {
		//TODO Before the first draw there is already a framebuffer error (but seems to work anyway so far)
		//GLenum e;
		//e = glGetError(); printf("%x\n", e);

		glm::mat4 viewProj = GetCamera()->GetViewPerspectiveMatrix();

        //TODO Is the engine matrix really needed here?
		glm::mat4 lightspace = shadowMap_->getLightMatrix();

		shadowMap_->DrawToFBO([&]() {
			meshpool_.renderAllMeshesExcept(lightspace, GridCell::BuildState::OUTER_INFLUENCE, 1);
		});
		

        fbo.DrawToFBO([&]() {
			backgroundMesh_->render(viewProj, lightspace, shadowMap_->get(), (render_mode_ == RenderMode::DBG) ? 1 : 0);
			glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			meshpool_.renderAllMeshes(viewProj, 0, (render_mode_ == RenderMode::DBG) ? 1 : 0);
			outerInfluence_->meshComponent->render(viewProj);
			glDisable(GL_BLEND);
        });
    }

	void ApplicationNodeImplementation::PostDraw() {
		GLenum e;
		while ((e = glGetError()) != GL_NO_ERROR) {
			if (e == last_glerror_) return;
			last_glerror_ = e;
			printf("Something went wrong during the last frame (GL error %x).\n", e);
		}
	}


    void ApplicationNodeImplementation::CleanUp()
    {
		meshpool_.cleanup();
		delete shadowMap_;
		delete backgroundMesh_;
    }

    bool ApplicationNodeImplementation::KeyboardCallback(int key, int scancode, int action, int mods)
    {
        return false;
    }
}
