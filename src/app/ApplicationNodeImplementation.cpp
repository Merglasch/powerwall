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
#include <glm/detail/type_mat.hpp>
#include <glm/detail/_vectorize.hpp>
#include "../../extern/fwcore/extern/assimp/code/ColladaHelper.h"
#include "roomgame/MeshInstanceBuilder.h"
#include "roomgame/InteractiveGrid.h"
#include "roomgame/RoomSegmentMeshPool.h"
#include "roomgame/RoomInteractionManager.h"
#include "roomgame\InnerInfluence.h"

namespace viscom {

    ApplicationNodeImplementation::ApplicationNodeImplementation(ApplicationNodeInternal* appNode) :
        ApplicationNodeBase{ appNode },
        meshpool_(GRID_COLS_ * GRID_ROWS_),
        render_mode_(NORMAL),
        clock_{ 0.0 },
        updateManager_(),
        automatonUpdater_(),
        camera_(glm::vec3(0,0,4), (viscom::CameraHelper&)(*GetCamera()))
    {
        sourceLightManager_ = std::make_shared<roomgame::SourceLightManager>();
        outerInfluence_ = std::make_shared<roomgame::OuterInfluence>(sourceLightManager_);
        meshInstanceBuilder_ = std::make_shared<MeshInstanceBuilder>(&meshpool_);
        interactiveGrid_ = std::make_shared<InteractiveGrid>(GRID_COLS_, GRID_ROWS_, GRID_HEIGHT_);
        meshInstanceBuilder_->interactiveGrid_ = interactiveGrid_;
        meshInstanceBuilder_->automatonUpdater_ = &automatonUpdater_;
        roomInteractionManager_ = std::make_shared<RoomInteractionManager>();
        roomInteractionManager_->interactiveGrid_ = interactiveGrid_;
        roomInteractionManager_->meshInstanceBuilder_ = meshInstanceBuilder_;
        roomInteractionManager_->automatonUpdater_ = &automatonUpdater_;
        interactiveGrid_->roomInteractionManager_ = roomInteractionManager_;
        automatonUpdater_.meshInstanceBuilder_ = meshInstanceBuilder_;
        automatonUpdater_.interactiveGrid_ = interactiveGrid_;
    }

    ApplicationNodeImplementation::~ApplicationNodeImplementation() = default;



    void ApplicationNodeImplementation::InitOpenGL() {

        screenfilling_quad_.init(GetApplication()->GetGPUProgramManager());

        /* Init mesh pool (mesh and shader resources need to be loaded on all nodes) */

        caustics = std::move(GetTextureManager().GetResource("/textures/caustics.png"));

        instanceShader_ = GetApplication()->GetGPUProgramManager().GetResource("renderMeshInstance",
            std::initializer_list<std::string>{ "renderMeshInstance.vert", "renderMeshInstance.frag" });

        sourceLightManager_->instanceShader_ = instanceShader_;
        meshpool_.loadShader(GetApplication()->GetGPUProgramManager(),instanceShader_);

        meshpool_.addMesh({ GridCell::INSIDE_ROOM },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomFloor.obj"));
        meshpool_.addMesh({ GridCell::CORNER },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomCorner.obj"));
        meshpool_.addMesh({ GridCell::WALL },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomWall.obj"));
        meshpool_.addMesh({ GridCell::INVALID | GridCell::TEMPORARY, GridCell::TEMPORARY,
            GridCell::INFECTED, GridCell::SOURCE | GridCell::INFECTED, GridCell::REPAIRING | GridCell::INFECTED },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/InnerInfluence.obj"));
        meshpool_.addMesh({ GridCell::SOURCE},
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomWallBrokenWater.obj"));

        meshpool_.updateUniformEveryFrame("t_sec", [this](GLint uloc) {
            glUniform1f(uloc, (float)clock_.t_in_sec);
        });

        meshpool_.updateUniformEveryFrame("automatonTimeDelta", [&](GLint uloc) {
            GLfloat time_delta = automatonUpdater_.automaton_transition_time_delta_;
            glUniform1f(uloc, time_delta);
        });

        meshpool_.updateUniformEveryFrame("gridDimensions", [&](GLint uloc) {
            glUniform2f(uloc, GRID_WIDTH_, GRID_HEIGHT_);
        });

        meshpool_.updateUniformEveryFrame("gridTranslation", [&](GLint uloc) {
            glm::vec3 translation = grid_translation_;
            glUniform3f(uloc, translation.x, translation.y, translation.z);
        });


        meshpool_.updateUniformEveryFrame("gridCellSize", [&](GLint uloc) {
            glUniform1f(uloc, GRID_CELL_SIZE_);
        });

        current_grid_state_texture_.id = GPUBuffer::alloc_texture2D(GRID_COLS_, GRID_ROWS_,
            roomgame::FILTERABLE_GRID_STATE_TEXTURE.sized_format, 
            roomgame::FILTERABLE_GRID_STATE_TEXTURE.format, 
            roomgame::FILTERABLE_GRID_STATE_TEXTURE.datatype);
        last_grid_state_texture_.id = GPUBuffer::alloc_texture2D(GRID_COLS_, GRID_ROWS_,
            roomgame::FILTERABLE_GRID_STATE_TEXTURE.sized_format,
            roomgame::FILTERABLE_GRID_STATE_TEXTURE.format,
            roomgame::FILTERABLE_GRID_STATE_TEXTURE.datatype);

        automatonUpdater_.currGridStateTexID = current_grid_state_texture_.id;
        automatonUpdater_.lastGridStateTexID = last_grid_state_texture_.id;

        meshpool_.updateUniformEveryFrame("curr_grid_state", [&](GLint uloc) {
            GLuint texture_unit = GL_TEXTURE0 + 0;
            glActiveTexture(texture_unit);
            glBindTexture(GL_TEXTURE_2D, current_grid_state_texture_.id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float border[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
            glUniform1i(uloc, 0);
        });

        meshpool_.updateUniformEveryFrame("last_grid_state", [&](GLint uloc) {
            GLuint texture_unit = GL_TEXTURE0 + 1;
            glActiveTexture(texture_unit);
            glBindTexture(GL_TEXTURE_2D, last_grid_state_texture_.id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float border[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
            glUniform1i(uloc, 1);
        });

        meshpool_.updateUniformEveryFrame("causticTex", [&](GLint uloc) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, caustics->getTextureId());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glUniform1i(uloc, 2);
        });


        /* Init outer influence */
        std::shared_ptr<viscom::GPUProgram> outerInfShader = GetApplication()->GetGPUProgramManager().GetResource("stuff",
            std::initializer_list<std::string>{ "applyTextureAndShadow.vert", "OuterInfl.frag" });

        SynchronizedGameMesh* outerInfluenceMeshComp = new SynchronizedGameMesh(
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/floor.obj"),
            outerInfShader);
        outerInfluence_->MeshComponent = outerInfluenceMeshComp;
        glm::mat4 movMat = glm::mat4(1);
        movMat = glm::scale(movMat, glm::vec3(0.1, 0.1, 0.1));
        movMat = glm::translate(movMat, glm::vec3(0, 0, 2));
        movMat = glm::translate(movMat, glm::vec3(30, 0, 0));
        outerInfluence_->MeshComponent->model_matrix_ = movMat;
        outerInfluence_->MeshComponent->scale = 0.1f;

        /* Load other meshes */

        terrainShader_ = GetApplication()->GetGPUProgramManager().GetResource("underwater",
            std::initializer_list<std::string>{ "underwater.vert", "underwater.frag" });
        sourceLightManager_->terrainShader_ = terrainShader_;
        std::string desertVersion = "";
#ifdef _DEBUG
        desertVersion = "/models/roomgame_models/newModels/desert.obj";
#else
        desertVersion = "/models/roomgame_models/newModels/desertWithDetail.obj";
#endif
        waterMesh_ = new PostProcessingMesh(
            GetApplication()->GetMeshManager().GetResource(desertVersion),
            terrainShader_);
        

        waterMesh_->scale = glm::vec3(0.5f,0.5f,0.5f);

        /* Init update manager */
        updateManager_.AddUpdateable(outerInfluence_);

        /*Light setup*/
        lightInfo = new LightInfo();
        glm::vec3 direction = glm::vec3(-1,-1,-4);
        glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
        glm::vec3 diffuse = glm::vec3(0.4f, 0.4f, 0.4f);
        glm::vec3 specular = glm::vec3(0.7f, 0.7f, 0.7f);
        lightInfo->sun = new DirLight(ambient,diffuse,specular,direction);
        /* Allocate offscreen framebuffer for shadow map */
        sm_ = new GPUBuffer::Tex{ 0, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
        sm_fbo_ = new GPUBuffer(2048, 2048, { sm_ });
        sm_lightmatrix_ = glm::ortho(/*left*/-10.0f, /*right*/10.0f, /*bot*/-10.0f, /*top*/10.0f, 0.1f, 100.0f) *
            glm::lookAt(-direction, glm::vec3(0, 0, -4), glm::vec3(0, 1, 0));

        ambient = glm::vec3(0.01f, 0.01f, 0.01f);
        diffuse = glm::vec3(0.9f, 0.1f, 0.1f);
        specular = glm::vec3(1.0f, .1f, .1f);
        lightInfo->outerInfLights = new PointLight(ambient, diffuse, specular,1.0f,0.8f,1.5f );
        diffuse = glm::vec3(0.1f, 0.1f, 0.9f);
        specular = glm::vec3(.1f, .1f, 1.0f);
        lightInfo->sourceLights = new PointLight(ambient, diffuse, specular, 1.0f, 0.8f, 1.5f);
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));

        // framebuffer configuration
        // -------------------------
        FrameBufferTextureDescriptor texDesc(GL_RGBA);
        RenderBufferDescriptor bufDesc(GL_DEPTH24_STENCIL8);
        std::vector<RenderBufferDescriptor> renVec;
        std::vector<FrameBufferTextureDescriptor> texVec;
        texVec.push_back(texDesc);
        renVec.push_back(bufDesc);
        FrameBufferDescriptor desc{texVec,renVec};
        offscreenBuffers = CreateOffscreenBuffers(desc);
        currentOffscreenBuffer = GetApplication()->SelectOffscreenBuffer(offscreenBuffers);
        fullScreenQuad = CreateFullscreenQuad("postProcessing.frag");



    }

    void ApplicationNodeImplementation::Draw2D(FrameBuffer &fbo) {
#if VISCOM_CLIENTGUI 
        glm::vec2 screenRes = GetConfig().virtualScreenSize_;
        ImVec2 popupSize = ImVec2(900, 100);
        ImVec2 scoreWindowSize = ImVec2(400, 100);
        fbo.DrawToFBO([&]() {
            ImGui::SetNextWindowPos(ImVec2(10, scoreWindowSize.y - 100), ImGuiSetCond_Always);
            ImGui::SetNextWindowSize(scoreWindowSize, ImGuiSetCond_Always);
            ImGui::Begin("Score", false, scoreWindowSize, -1.0f, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs);
            ImGui::Text("Current Score: %i", currentScore);
            ImGui::Text("Highest Score this Session: %i", highestScoreThisSession);
            ImGui::End();

            if (gameLost_)
            {
                ImGui::SetNextWindowPos(ImVec2(screenRes.x / 2 - popupSize.x / 2, screenRes.y / 2 - popupSize.y / 2), ImGuiSetCond_Always);
                ImGui::Begin("Game Lost", false, popupSize, -1.0f, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs);
                ImGui::Text("You can restart by pressing the 'Reset Playground' button on the master");
                ImGui::End();
            }
        });
#endif
    }


    void ApplicationNodeImplementation::UpdateFrame(double currentTime, double elapsedTime)
    {
//        camera_.UpdateCamera(elapsedTime, this);
        clock_.set(currentTime);
        waterMesh_->setTime(currentTime);
    }

    void ApplicationNodeImplementation::ClearBuffer(FrameBuffer& fbo)
    {
        fbo.DrawToFBO([]() {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        });
    }

    void ApplicationNodeImplementation::DrawFrame(FrameBuffer& fbo)
    {
        glm::mat4 viewProj = GetCamera()->GetViewPerspectiveMatrix();

        for (int i = 0; i < min(5,outerInfPositions_.size()); i++) {
            glm::mat4 tmp = outerInfPositions_[i];
            lightInfo->infLightPos[i] = glm::vec3(tmp[3][0], tmp[3][1], tmp[3][2]);
        }

        sourceLightManager_->updateSourcePos();

        glm::vec3 viewPos = GetCamera()->GetPosition();

        sm_lightmatrix_ = glm::ortho(/*left*/-10.0f, /*right*/10.0f, /*bot*/-10.0f, /*top*/10.0f, 0.1f, 20.0f) *
            glm::lookAt(-lightInfo->sun->direction, glm::vec3(0, 0, -4), glm::vec3(0, 1, 0));

        // render to shadow map
        glBindFramebuffer(GL_FRAMEBUFFER, sm_fbo_->id());
        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, 2048, 2048);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        DrawScene(viewPos, sm_lightmatrix_, sm_lightmatrix_, lightInfo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        currentOffscreenBuffer->DrawToFBO([&]()
        {
            glEnable(GL_DEPTH_TEST); // enable depth testing (is disabled for rendering screen-space quad)

                                     // make sure we clear the framebuffer's content
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawScene(viewPos, sm_lightmatrix_, viewProj, lightInfo);
        });



        fbo.DrawToFBO([&]() {
            glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
                                      // clear all relevant buffers
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(fullScreenQuad->GetGPUProgram()->getProgramId());
            GLuint imageTex = currentOffscreenBuffer->GetTextures().at(0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, imageTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glUniform1i(fullScreenQuad->GetGPUProgram()->getUniformLocation("screenTexture"), 0);

            fullScreenQuad->Draw();

        });
    }

    void ApplicationNodeImplementation::DrawScene(glm::vec3 viewPos, glm::mat4 lightspace, glm::mat4 viewProj, LightInfo *lightInfo)
    {
        waterMesh_->render(viewProj, lightspace, sm_->id, caustics->getTextureId(), (render_mode_ == RenderMode::DBG) ? 1 : 0, lightInfo, viewPos);
        
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        meshpool_.renderAllMeshes(viewProj, 0, (render_mode_ == RenderMode::DBG) ? 1 : 0, lightInfo, viewPos);
        RenderOuterInfluence(viewPos, viewProj, lightInfo);
        glDisable(GL_BLEND);
    }

    void ApplicationNodeImplementation::RenderOuterInfluence(glm::vec3 viewPos, glm::mat4 viewProj, LightInfo* lightInfo)
    {
        const auto influPos = outerInfluence_->MeshComponent->model_matrix_;
        int outerInfInstances = 250;
        float scaleStep = 0.004f;
#ifdef _DEBUG
        outerInfInstances = 20;
        scaleStep = 0.01f;
#endif
        if (outerInfPositions_.size() > outerInfInstances) {
            outerInfPositions_.resize(outerInfInstances);
        }
        for (auto i = 0; i < outerInfPositions_.size(); i++) {
            if (i % 5 == 0) {
                outerInfluence_->MeshComponent->scale -= scaleStep;
            }
            outerInfluence_->MeshComponent->model_matrix_ = outerInfPositions_[i];
            outerInfluence_->MeshComponent->render(viewProj, 1, nullptr, glm::mat4(1), false, lightInfo, viewPos, (render_mode_ == RenderMode::DBG) ? 1 : 0);
        }
        outerInfluence_->MeshComponent->scale = 0.2f;
        for (auto i = 0; i < outerInfluence_->MeshComponent->influencePositions_.size(); i++) {
            outerInfluence_->MeshComponent->model_matrix_ = outerInfluence_->MeshComponent->influencePositions_[i];
            outerInfPositions_.insert(outerInfPositions_.begin(), outerInfluence_->MeshComponent->influencePositions_[i]);
            outerInfluence_->MeshComponent->render(viewProj, 1, nullptr, glm::mat4(1), false, lightInfo, viewPos, (render_mode_ == RenderMode::DBG) ? 1 : 0);
        }
        outerInfluence_->MeshComponent->model_matrix_ = influPos;
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
        delete sm_;
        delete sm_fbo_;
        delete waterMesh_;
        delete lightInfo->sun;
        delete lightInfo->outerInfLights;
        delete lightInfo->sourceLights;
    }

    bool ApplicationNodeImplementation::KeyboardCallback(int key, int scancode, int action, int mods)
    {
        return false;
    }

}
