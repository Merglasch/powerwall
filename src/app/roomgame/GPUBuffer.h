#ifndef GPUBUFFER_H_
#define GPUBUFFER_H_

#include <stdexcept>
#include <initializer_list>
#include <GL/glew.h>

/*
* A gpu buffer representation.
* Can be used as
* a) Vertex buffer
* b) Framebuffer
* c) Uniform buffer
*    ...
*/
class GPUBuffer {
    bool is_fbo_;
    GLuint opengl_id_;
public:
    struct Tex {
        GLuint id = 0;
        GLenum attachmentType = 0;
        GLint sized_format = 0;
        GLenum format = 0;
        GLenum datatype = 0;
    };
    // Create multipurpose buffer
    GPUBuffer();
    GPUBuffer(GLsizeiptr bytes, const GLvoid* pointer);
    // Create framebuffer with one or more 2D textures
    GPUBuffer(GLsizei w, GLsizei h, std::initializer_list<Tex*> tex_attachments);
    ~GPUBuffer();
    GLuint id() { return opengl_id_; }
    void bind();
    void bind_to(GLenum targetpoint);
    // Bind and init gpu memory
    void bind_data(GLsizeiptr bytes, const GLvoid* pointer);
    // Create empty 2D texture without mipmap
    static GLuint alloc_texture2D(GLsizei w, GLsizei h, GLint sized_format, GLenum format, GLenum type);
    static GLuint alloc_immutable_format_texture2D(GLsizei w, GLsizei h, GLint sized_format, GLenum format, GLenum type);

};

#endif