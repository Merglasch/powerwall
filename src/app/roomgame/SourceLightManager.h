#pragma once
#include <memory>
#include "core/gfx/GPUProgram.h"

namespace roomgame
{
    class SourceLightManager
    {
    private:
        std::string outerInfString = "outerInfLights";
        std::string sourceString = "sourceLights";

    public:
        SourceLightManager();
        ~SourceLightManager();
        
        static const int MAX_VISIBLE_SOURCE_LIGHTS = 15; //When changing this, don't forget to change the values in the underwater.frag and renderMeshInstance.frag too

        void DeleteClosestSourcePos(glm::vec3 curedSourcePos);
        void updateSourcePos();

        void uploadSourcePos(std::shared_ptr<viscom::GPUProgram> shad, std::vector<glm::vec3> sourcePositions);

        std::shared_ptr<viscom::GPUProgram> instanceShader_ = nullptr;
        std::shared_ptr<viscom::GPUProgram> terrainShader_ = nullptr;

        std::vector<glm::vec3> sourcePositions_;
        sgct::SharedVector<glm::vec3> sharedSourceLightPositions_;

        void preSync() { // master
            sharedSourceLightPositions_.setVal(sourcePositions_);
        }
        void encode() { // master
            sgct::SharedData::instance()->writeVector(&sharedSourceLightPositions_);
        }
        void decode() { // slave
            sgct::SharedData::instance()->readVector(&sharedSourceLightPositions_);
        }
        void updateSyncedSlave() {
            sourcePositions_ = sharedSourceLightPositions_.getVal();
        }
        void updateSyncedMaster() {
            //Can maybe stay empty
        }
    };
}
