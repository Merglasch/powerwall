#include "GPUBuffer.h"
#include <iostream>

GPUBuffer::GPUBuffer() {
    glGenBuffers(1, &opengl_id_);
    is_fbo_ = false;
}

GPUBuffer::GPUBuffer(GLsizeiptr bytes, const GLvoid* pointer) {
    glGenBuffers(1, &opengl_id_);
    glBindBuffer(GL_ARRAY_BUFFER, opengl_id_);
    glBufferData(GL_ARRAY_BUFFER, bytes, pointer, GL_STATIC_DRAW);
    is_fbo_ = false;
}

GPUBuffer::GPUBuffer(GLsizei w, GLsizei h, std::initializer_list<Tex*> tex_attachments) {
    glGenFramebuffers(1, &opengl_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, opengl_id_);
    int num_color_attachments = 0;
    int num_depth_attachments = 0;
    int num_stencil_attachments = 0;
    for (Tex* tex : tex_attachments) {
        GLenum e;
        e = glGetError(); printf("a: %x\n", e);        
        tex->id = alloc_immutable_format_texture2D(w, h, tex->sized_format, tex->format,tex->datatype);
        e = glGetError(); printf("b: %x\n", e);
        GLenum attachment_type = tex->attachmentType;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_type, GL_TEXTURE_2D, tex->id, 0);
        // Count attachments
        GLint max_color_attachments = 8;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
        if ((int)attachment_type >= (int)GL_COLOR_ATTACHMENT0 && (int)attachment_type <= ((int)GL_COLOR_ATTACHMENT0 + max_color_attachments - 1))
            num_color_attachments++;
        else if (attachment_type == GL_DEPTH_ATTACHMENT)
            num_depth_attachments++;
        else if (attachment_type == GL_STENCIL_ATTACHMENT)
            num_stencil_attachments++;
        else if (attachment_type == GL_DEPTH_STENCIL_ATTACHMENT) {
            num_depth_attachments++;
            num_stencil_attachments++;
        }
    }
    // If no color texture, disable color rendering
    if (num_color_attachments == 0) {
        glDrawBuffer(GL_NONE); // OpenGL won't try to write to a color buffer
        glReadBuffer(GL_NONE); // reading from color buffer is also disabled
    }
    else if (num_color_attachments > 1) {
        //TODO Link fragment shader out variables to attachments with glBindFragDataLocation
    }
    // If no depth and stencil texture, attach renderbuffer
    if (num_depth_attachments == 0 && num_stencil_attachments == 0) {
        // Renderbuffer is write-only, and probably faster than textures
        GLuint renderbuffer = 0;
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
    }
    else if (num_depth_attachments == 0) {
        //TODO do something for color+stencil attachments
    }
    else if (num_stencil_attachments == 0) {
        //TODO do something for color+depth attachments
    }
    // Check if framebuffer complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Program exits because I failed to complete framebuffer.");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    is_fbo_ = true;
}

GPUBuffer::~GPUBuffer() {
    if (is_fbo_)
        glDeleteFramebuffers(1, &opengl_id_);
    else
        glDeleteBuffers(1, &opengl_id_);
    opengl_id_ = 0;
}

void GPUBuffer::bind() {
    if (is_fbo_)
        glBindFramebuffer(GL_FRAMEBUFFER, opengl_id_);
    else
        glBindBuffer(GL_ARRAY_BUFFER, opengl_id_);
}

void GPUBuffer::bind_to(GLenum targetpoint) {
    if (is_fbo_)
        glBindFramebuffer(targetpoint, opengl_id_);
    else
        glBindBuffer(targetpoint, opengl_id_);
}

void GPUBuffer::bind_data(GLsizeiptr bytes, const GLvoid* pointer) {
    if (is_fbo_)
        return;
    glBindBuffer(GL_ARRAY_BUFFER, opengl_id_);
    glBufferData(GL_ARRAY_BUFFER, bytes, pointer, GL_STATIC_DRAW);
}

GLuint GPUBuffer::alloc_texture2D(GLsizei w, GLsizei h, GLint sized_format, GLenum format, GLenum type) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // format and type are irrelevant here because they refer to passed pixel data, which is 0 here
    // still they have to be compatible with sized_format, i.e. the format in which pixel data is converted by GL
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, sized_format, w, h, 0, format, type, 0);
    return tex;
}

GLuint GPUBuffer::alloc_immutable_format_texture2D(GLsizei w, GLsizei h, GLint sized_format, GLenum format, GLenum type) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // glTexStorage specifies an immutable-format texture,
    // which means glTexImage cannot be called because it might alter format (glTexSubImage is possible anyway)
    
    glTexStorage2D(GL_TEXTURE_2D, 1, sized_format, w, h);
//    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h, format, type, 0);
    return tex;
}