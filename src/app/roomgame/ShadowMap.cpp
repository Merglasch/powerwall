#include "ShadowMap.h"

ShadowMap::ShadowMap(unsigned int w, unsigned int h) :
	FrameBuffer(w, h, { { viscom::FrameBufferTextureDescriptor(GL_DEPTH_COMPONENT32F) },{} }),
	light_matrix_(1)
{

}

glm::mat4& ShadowMap::getLightMatrix() {
	return light_matrix_;
}

void ShadowMap::setLightMatrix(glm::mat4& light_matrix) {
	light_matrix_ = light_matrix;
}