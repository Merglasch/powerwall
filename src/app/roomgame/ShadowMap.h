#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include "core/gfx/FrameBuffer.h"

class ShadowMap : public viscom::FrameBuffer {
	glm::mat4 light_matrix_;
public:
	ShadowMap(unsigned int w, unsigned int h);
	GLuint get() const {
		return GetTextures()[0];
	}
	glm::mat4& getLightMatrix();
	void setLightMatrix(glm::mat4&);
};

#endif