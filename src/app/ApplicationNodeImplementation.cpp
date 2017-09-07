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
        meshpool_(GRID_COLS_ * GRID_ROWS_),
        render_mode_(NORMAL),
        clock_{ 0.0 },
        updateManager_(),
        current_grid_state_texture_(roomgame::GRID_STATE_TEXTURE),
        last_grid_state_texture_(roomgame::GRID_STATE_TEXTURE),
        camera_(glm::vec3(0,0,4), (viscom::CameraHelper&)(*GetCamera()))
    {
        outerInfluence_ = std::make_shared<roomgame::OuterInfluence>();
    }

    ApplicationNodeImplementation::~ApplicationNodeImplementation() = default;



    void ApplicationNodeImplementation::InitOpenGL() {

        /* Init mesh pool (mesh and shader resources need to be loaded on all nodes) */

        instanceShader_ = GetApplication()->GetGPUProgramManager().GetResource("renderMeshInstance",
            std::initializer_list<std::string>{ "renderMeshInstance.vert", "renderMeshInstance.frag" });

        meshpool_.loadShader(GetApplication()->GetGPUProgramManager(),instanceShader_);

        meshpool_.addMesh({ GridCell::INSIDE_ROOM },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomFloor.obj"));
        meshpool_.addMesh({ GridCell::CORNER },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomCorner.obj"));
        meshpool_.addMesh({ GridCell::WALL },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/RoomWall.obj"));
        meshpool_.addMesh({ GridCell::INVALID|GridCell::TEMPORARY, GridCell::TEMPORARY, 
            GridCell::INFECTED, GridCell::SOURCE, GridCell::SOURCE|GridCell::INFECTED },
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/InnerInfluence.obj"));

        meshpool_.updateUniformEveryFrame("t_sec", [this](GLint uloc) {
            glUniform1f(uloc, (float)clock_.t_in_sec);
        });

        meshpool_.updateUniformEveryFrame("automatonTimeDelta", [&](GLint uloc) {
            GLfloat time_delta = automaton_transition_time_delta_;
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


        /* Init outer influence */
        std::shared_ptr<viscom::GPUProgram> outerInfShader = GetApplication()->GetGPUProgramManager().GetResource("stuff",
            std::initializer_list<std::string>{ "applyTextureAndShadow.vert", "OuterInfl.frag" });

        SynchronizedGameMesh* outerInfluenceMeshComp = new SynchronizedGameMesh(
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/latticeplane.obj"),
            outerInfShader);
        outerInfluence_->MeshComponent = outerInfluenceMeshComp;
        glm::mat4 movMat = glm::mat4(1);
        movMat = glm::scale(movMat, glm::vec3(0.1, 0.1, 0.1));
        movMat = glm::translate(movMat, glm::vec3(0, 0, 2));
        movMat = glm::translate(movMat, glm::vec3(30, 0, 0));
        outerInfluence_->MeshComponent->model_matrix_ = movMat;
        outerInfluence_->MeshComponent->scale = 0.1f;

        /* Load other meshes */

        //backgroundMesh_ = new ShadowReceivingMesh(
        //    GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/textured_4vertexplane/textured_4vertexplane.obj"),
        //    GetApplication()->GetGPUProgramManager().GetResource("applyTextureAndShadow",
        //		std::initializer_list<std::string>{ "applyTextureAndShadow.vert", "applyTextureAndShadow.frag" }));
        terrainShader_ = GetApplication()->GetGPUProgramManager().GetResource("underwater",
            std::initializer_list<std::string>{ "underwater.vert", "underwater.frag" });
        waterMesh_ = new PostProcessingMesh(
            GetApplication()->GetMeshManager().GetResource("/models/roomgame_models/newModels/desertWithDetail.obj"),
            terrainShader_);
        
        waterMesh_->scale = glm::vec3(0.5f,0.5f,0.5f);

        /* Allocate offscreen framebuffer for shadow map */

        shadowMap_ = new ShadowMap(1024, 1024);
        shadowMap_->setLightMatrix(glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0), glm::vec3(0, 1, 0)));

        /* Init update manager */
        updateManager_.AddUpdateable(outerInfluence_);

        /*Light setup*/
        lightInfo = new LightInfo();
        glm::vec3 direction = glm::vec3(-1,-1,-4);
        glm::vec3 ambient = glm::vec3(0.1f, 0.1f, 0.1f);
        glm::vec3 diffuse = glm::vec3(0.4f, 0.4f, 0.4f);
        glm::vec3 specular = glm::vec3(0.7f, 0.7f, 0.7f);
        lightInfo->sun = new DirLight(ambient,diffuse,specular,direction);

        ambient = glm::vec3(0.01f, 0.01f, 0.01f);
        diffuse = glm::vec3(0.9f, 0.1f, 0.1f);
        specular = glm::vec3(1.0f, .1f, .1f);
        lightInfo->outerInfLights = new PointLight(ambient, diffuse, specular,1.0f,20.0f,30.0f );
        diffuse = glm::vec3(0.1f, 0.1f, 0.9f);
        specular = glm::vec3(.1f, .1f, 1.0f);
        lightInfo->sourceLights = new PointLight(ambient, diffuse, specular, 1.0f, 0.8f, 1.5f);
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
        lightInfo->infLightPos.push_back(glm::vec3(0));
//        lightInfo->sourceLightsPos.push_back(glm::vec3(-.5f, -.5f, 0.2f));
//        lightInfo->sourceLightsPos.push_back(glm::vec3(.5f, .5f, 0.2f));
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
        for (int i = 0; i < min(5,outerInfPositions_.size()); i++) {
            glm::mat4 tmp = outerInfPositions_[i];
            lightInfo->infLightPos[i] = glm::vec3(tmp[3][0], tmp[3][1], tmp[3][2]);
        }
        glm::vec3 viewPos = GetCamera()->GetPosition();
        //TODO Is the engine matrix really needed here?
        glm::mat4 lightspace = shadowMap_->getLightMatrix();
        shadowMap_->DrawToFBO([&]() {
            meshpool_.renderAllMeshesExcept(lightspace, GridCell::OUTER_INFLUENCE, 1, (render_mode_ == RenderMode::DBG) ? 1 : 0,lightInfo,viewPos);
        });
        updateSourcePos(outerInfluence_->MeshComponent->sourcePositions_);

        fbo.DrawToFBO([&]() {
            //backgroundMesh_->render(viewProj, lightspace, shadowMap_->get(), (render_mode_ == RenderMode::DBG) ? 1 : 0);
            waterMesh_->render(viewProj, lightspace, shadowMap_->get(), (render_mode_ == RenderMode::DBG) ? 1 : 0,lightInfo,viewPos);
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            meshpool_.renderAllMeshes(viewProj, 0, (render_mode_ == RenderMode::DBG) ? 1 : 0, lightInfo, viewPos);
            glm::mat4 influPos = outerInfluence_->MeshComponent->model_matrix_;
            
            if (outerInfPositions_.size() > 40) {
                outerInfPositions_.resize(40);
            }
            for (int i = 0; i < outerInfPositions_.size(); i++) {
                if (i % 5 == 0) {
                    outerInfluence_->MeshComponent->scale -= 0.01f;
                }
                outerInfluence_->MeshComponent->model_matrix_ = outerInfPositions_[i];
                outerInfluence_->MeshComponent->render(viewProj,1,nullptr,glm::mat4(1),false,lightInfo,viewPos, (render_mode_ == RenderMode::DBG) ? 1 : 0);
            }
            outerInfluence_->MeshComponent->scale = 0.1f;
            for (int i = 0; i < outerInfluence_->MeshComponent->influencePositions_.size(); i++) {
                outerInfluence_->MeshComponent->model_matrix_ = outerInfluence_->MeshComponent->influencePositions_[i];
                outerInfPositions_.insert(outerInfPositions_.begin(),outerInfluence_->MeshComponent->influencePositions_[i]);
                outerInfluence_->MeshComponent->render(viewProj, 1, nullptr, glm::mat4(1), false, lightInfo, viewPos, (render_mode_ == RenderMode::DBG) ? 1 : 0);
            }
            outerInfluence_->MeshComponent->model_matrix_ = influPos;
            glDisable(GL_BLEND);
        });
    }
    std::string outerInfString = "outerInfLights";
    std::string sourceString = "sourceLights";

    void ApplicationNodeImplementation::updateSourcePos(std::vector<glm::vec3> sourcePositions) {
        glUseProgram(instanceShader_->getProgramId());
        uploadSourcePos(instanceShader_, sourcePositions);
        glUseProgram(terrainShader_->getProgramId());
        uploadSourcePos(terrainShader_, sourcePositions);
    }

    void ApplicationNodeImplementation::uploadSourcePos(std::shared_ptr<viscom::GPUProgram> shad, std::vector<glm::vec3> sourcePositions) {
        GLint lightNum = (GLint)min(sourcePositions.size(), 10);
        glUniform1i(shad->getUniformLocation((std::string)("numSourceLights")), lightNum);
        for (int i = 0; i < lightNum; i++) {
            std::string number = "" + std::to_string(i);
            std::string loc = sourceString + "[" + number + "].position";
            GLint uniLoc = shad->getUniformLocation(loc);
            glUniform3fv(uniLoc, 1, glm::value_ptr(sourcePositions[i]));
        }
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
        //delete backgroundMesh_;
        delete waterMesh_;
    }

    bool ApplicationNodeImplementation::KeyboardCallback(int key, int scancode, int action, int mods)
    {
        return false;
    }

}
