#include "GameMesh.h"

ShadowReceivingMesh::ShadowReceivingMesh(std::shared_ptr<viscom::Mesh> mesh, std::shared_ptr<viscom::GPUProgram> shader) :
	SimpleGameMesh(mesh, shader)
{
	uloc_lightspace_matrix_ = shader->getUniformLocation("lightSpaceMatrix");
	uloc_shadow_map_ = shader->getUniformLocation("shadowMap");
}

void ShadowReceivingMesh::render(glm::mat4& vp, glm::mat4& lightspace, GLuint shadowMap, GLint isDebugMode, LightInfo* lightInfo, glm::vec3& viewPos)  {
	SimpleGameMesh::render(vp,1,[&]() {
        glUniformMatrix4fv(uloc_lightspace_matrix_, 1, GL_FALSE, &lightspace[0][0]);
		// Bind shadow map to texture unit **2** because
		// the MeshBase super-class already uses 0 (diffuse texture) and 1 (bump map)
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadowMap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glUniform1i(uloc_shadow_map_, 2);
	},false,lightInfo,viewPos,isDebugMode);
}

PostProcessingMesh::PostProcessingMesh(std::shared_ptr<viscom::Mesh> mesh, std::shared_ptr<viscom::GPUProgram> shader) :
	SimpleGameMesh(mesh, shader)
{
	time_ = 0;
	uloc_time_ = shader->getUniformLocation("time");
	uloc_lightspace_matrix_ = shader->getUniformLocation("lightSpaceMatrix");
	uloc_shadow_map_ = shader->getUniformLocation("shadowMap");
    uloc_caustics_ = shader->getUniformLocation("causticTex");
}

void PostProcessingMesh::render(glm::mat4& vp, glm::mat4& lightspace, GLuint shadowMap, GLuint caustics, GLint isDebugMode, LightInfo* lightInfo, glm::vec3& viewPos )  {
    SimpleGameMesh::render(vp,1,[&]() {
        glUniformMatrix4fv(uloc_lightspace_matrix_, 1, GL_FALSE, &lightspace[0][0]);
		// Bind shadow map to texture unit **2** because
		// the MeshBase super-class already uses 0 (diffuse texture) and 1 (bump map)
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadowMap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glUniform1i(uloc_shadow_map_, 2);
		glUniform1f(uloc_time_, time_);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, caustics);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glUniform1i(uloc_caustics_, 3);
	},false, lightInfo,viewPos,isDebugMode);
}