/**
 * @file   ApplicationNodeImplementation.h
 * @author Sebastian Maisch <sebastian.maisch@uni-ulm.de>
 * @date   2016.11.30
 *
 * @brief  Declaration of the application node implementation common for master and slave nodes.
 */

#pragma once

#include "core/ApplicationNodeInternal.h"
#include "core/ApplicationNodeBase.h"
#include "core/camera/ArcballCamera.h"

#include "app/roomgame/AutomatonUpdater.h"
#include "app/roomgame/DragAndZoomCamera.h"
#include "app/roomgame/GameMesh.h"
#include "app/roomgame/ShadowMap.h"
#include "app/roomgame/UpdateManager.h"
#include "app/roomgame/OuterInfluence.h"
#include "app/roomgame/GPUBuffer.h"
#include "app/roomgame/GPUCellularAutomaton.h"
#include "app/roomgame/RoomSegmentMeshPool.h"

namespace roomgame
{
    class MeshInstanceBuilder;
    class InteractiveGrid;
    class RoomSegmentMeshPool;
    class RoomInteractionManager;
}
using roomgame::MeshInstanceBuilder;
using roomgame::InteractiveGrid;
using roomgame::RoomSegmentMeshPool;
using roomgame::RoomInteractionManager;

namespace viscom {

    class MeshRenderable;

	/* Roomgame controller class for all nodes
	 * Uses RoomSegementMeshPool to create meshes and map them to build states
	 * Triggers rendering of all meshes in pool
	 * Creates and renders static meshes
	*/
    class ApplicationNodeImplementation : public ApplicationNodeBase {


    public:

        explicit ApplicationNodeImplementation(ApplicationNodeInternal* appNode);
        ApplicationNodeImplementation(const ApplicationNodeImplementation&) = delete;
        ApplicationNodeImplementation(ApplicationNodeImplementation&&) = delete;
        ApplicationNodeImplementation& operator=(const ApplicationNodeImplementation&) = delete;
        ApplicationNodeImplementation& operator=(ApplicationNodeImplementation&&) = delete;
        virtual ~ApplicationNodeImplementation() override;

        virtual void InitOpenGL() override;
        virtual void UpdateFrame(double currentTime, double elapsedTime) override;
        virtual void ClearBuffer(FrameBuffer& fbo) override;
        virtual void DrawFrame(FrameBuffer& fbo) override;
        virtual void Draw2D(FrameBuffer& fbo) override;
        virtual void PostDraw() override;
        virtual void CleanUp() override;

        virtual bool KeyboardCallback(int key, int scancode, int action, int mods) override;


    private:
        void DrawScene(glm::vec3 viewPos, glm::mat4 lightspace, glm::mat4 viewProj, LightInfo *lightInfo);
        void RenderOuterInfluence(glm::vec3 viewPos, glm::mat4 viewProj, LightInfo* lightInfo);
		GLenum last_glerror_; // helps output an error only once
        //unsigned int textureColorbuffer;
        std::vector<FrameBuffer> offscreenBuffers;
        const FrameBuffer* currentOffscreenBuffer;
        std::unique_ptr<FullscreenQuad> fullScreenQuad;
        bool debugModels = true;

    protected:

		// ROOMGAME DATA
		// =============

        /*  */
        LightInfo* lightInfo;

        /* Handles all automaton related grid updates */
        roomgame::AutomatonUpdater automatonUpdater_;

        std::shared_ptr<MeshInstanceBuilder> meshInstanceBuilder_;
        std::shared_ptr<RoomInteractionManager> roomInteractionManager_;
        std::shared_ptr<InteractiveGrid> interactiveGrid_;
        /* Copy of part of grid state for use by each node's GPU (same as used by automaton) */
        GPUBuffer::Tex current_grid_state_texture_, last_grid_state_texture_;

        /* Update Manager */
		roomgame::UpdateManager updateManager_;

        /*  */
        std::vector<glm::mat4> outerInfPositions_;

        /*  */
        std::shared_ptr<viscom::GPUProgram> instanceShader_;
        std::shared_ptr<viscom::GPUProgram> terrainShader_;
        std::shared_ptr<roomgame::SourceLightManager> sourceLightManager_;

		/* Grid parameters (constant on all nodes) */
		const int GRID_COLS_ = 128;
		const int GRID_ROWS_ = 128;
		const float GRID_HEIGHT_ = 10.0f;
		const float GRID_CELL_SIZE_ = GRID_HEIGHT_ / GRID_ROWS_;
		const float GRID_WIDTH_ = GRID_COLS_ * GRID_CELL_SIZE_;

		/* Grid translation (controlled by master node and synced with slaves) */
		sgct::SharedObject<glm::vec3> synchronized_grid_translation_;
		glm::vec3 grid_translation_;

		/* Outer influence object containing AI logic and mesh */
		std::shared_ptr<roomgame::OuterInfluence> outerInfluence_;

		/* Mesh pool manages and renders instanced meshes corresponding to build states of grid cells */
		roomgame::RoomSegmentMeshPool meshpool_; // hold mesh and shader resources and render on all nodes

        /* Shadow map is basically an offscreen framebuffer */
		ShadowMap* shadowMap_; // hold shadow map framebuffer on all nodes

        GPUBuffer::Tex* sm_;
        GPUBuffer* sm_fbo_;
        glm::mat4 sm_lightmatrix_;

        /* Shadow receiving meshes are (non-instanced) meshes with a shader reading from shadow maps */
		ShadowReceivingMesh* backgroundMesh_; // hold static mesh on all nodes

		/* Same as ShadowReceivingMesh but with water features (can also apply arbitrary post-processing) */
		PostProcessingMesh* waterMesh_;

		/* Switch for debug rendering */
		enum RenderMode { NORMAL, DBG } render_mode_; // hold on all nodes but able to change only on master

		/* Clock holding a (hopefully!) synchronized time on all nodes */
		struct Clock {
			double t_in_sec;
			double last_t_in_sec;
			void set(double current_t_in_sec) {
				last_t_in_sec = t_in_sec;
				t_in_sec = current_t_in_sec;
			}
			double deltat() {
				return t_in_sec - last_t_in_sec;
			}
		} clock_;

        /*  */
        viscom::ArcballCamera camera_;

        bool gameLost_ = false;
        sgct::SharedBool gameLostShared;

        float uninfectedCells = 0;
        float infectedCells = 0;
        float sourceCells = 0;

        int currentScore = 0;
        sgct::SharedInt32 currentScoreShared;
        int highestScoreThisSession = 0;
        sgct::SharedInt32 highestScoreThisSessionShared;

		/* Quad for debug-visualizing offscreen textures */
		struct Quad {
			std::shared_ptr<GPUProgram> shader;
			GLint texture_uniform_location;
			GLuint vao;
			void init(GPUProgramManager mgr) {
				glGenVertexArrays(1, &vao);
				glBindVertexArray(vao);
				GLfloat quad[] = {
					// (x, y)      // (u, v)
					-1.0f,  1.0f,  0.0f, 1.0f, // top left
					1.0f, -1.0f,  1.0f, 0.0f, // bottom right
					-1.0f, -1.0f,  0.0f, 0.0f, // bottom left
					-1.0f,  1.0f,  0.0f, 1.0f, // top left
					1.0f,  1.0f,  1.0f, 1.0f, // top right
					1.0f, -1.0f,  1.0f, 0.0f // bottom right
				};
				GLuint vbo;
				glGenBuffers(1, &vbo);
				glBindBuffer(GL_ARRAY_BUFFER, vbo);
				glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
				glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
				glEnableVertexAttribArray(0);
				glEnableVertexAttribArray(1);
				glBindVertexArray(0);
				shader = mgr.GetResource("applyTextureToQuad",
					std::initializer_list<std::string>{ "applyTextureToQuad.vert", "applyTextureToQuad.frag" });
				texture_uniform_location = shader->getUniformLocation("tex");
			}
			void render(GLuint texture) const {
				glUseProgram(shader->getProgramId());
				glDisable(GL_DEPTH_TEST);
				glDisable(GL_CULL_FACE);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texture);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
				float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
				glUniform1i(texture_uniform_location, 0);
				glBindVertexArray(vao);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				glBindVertexArray(0);
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
			}
		} screenfilling_quad_;

    private:
        std::shared_ptr<Texture> caustics;
	};
}
