#include "SourceLightManager.h"


namespace roomgame
{
    SourceLightManager::SourceLightManager()
    {
    }


    SourceLightManager::~SourceLightManager()
    {
    }

    void SourceLightManager::DeleteClosestSourcePos(glm::vec3 curedSourcePos)
    {
        float dist = 1000.0f;
        int counter = -1;
        for (int i = 0; i<sourcePositions_.size();i++)
        {
            float calculatedDistance = glm::distance(sourcePositions_[i], curedSourcePos);
            if (calculatedDistance < dist)
            {
                dist = calculatedDistance;
                counter = i;
            }
        }
        if (counter != -1)
        {
            sourcePositions_.erase(sourcePositions_.begin() + counter);
        }
    }


    void SourceLightManager::updateSourcePos() {
        glUseProgram(instanceShader_->getProgramId());
        uploadSourcePos(instanceShader_, sourcePositions_);
        glUseProgram(terrainShader_->getProgramId());
        uploadSourcePos(terrainShader_, sourcePositions_);
    }

    void SourceLightManager::uploadSourcePos(std::shared_ptr<viscom::GPUProgram> shad, std::vector<glm::vec3> sourcePositions) {
        GLint lightNum = (GLint)min(static_cast<int>(sourcePositions.size()), MAX_VISIBLE_SOURCE_LIGHTS);
        glUniform1i(shad->getUniformLocation((std::string)("numSourceLights")), lightNum);
        for (int i = 0; i < lightNum; i++) {
            std::string number = "" + std::to_string(i);
            std::string loc = sourceString + "[" + number + "].position";
            GLint uniLoc = shad->getUniformLocation(loc);
            glUniform3fv(uniLoc, 1, glm::value_ptr(sourcePositions[i]));
        }
    }
}

