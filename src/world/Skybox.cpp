#include "../precompiled.h"
#include "Skybox.hpp"

#include <stb_image.h>

Skybox::~Skybox()
{
    if (hdr_texture_ != 0)
    {
        glDeleteTextures(1, &hdr_texture_);
    }

    if (vbo_ != 0)
    {
        glDeleteBuffers(1, &vbo_);
    }

    if (vao_ != 0)
    {
        glDeleteVertexArrays(1, &vao_);
    }
}

void Skybox::Init(const std::string& hdrPath)
{
    CreateCube();
    LoadHDRTexture(hdrPath);
}

void Skybox::CreateCube()
{
    const float vertices[] = {
        -1.0f,  1.0f, -1.0f,   -1.0f, -1.0f, -1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,    1.0f,  1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,   -1.0f, -1.0f, -1.0f,   -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,    1.0f, -1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f,  1.0f, -1.0f,    1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,    1.0f, -1.0f,  1.0f,   -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,    1.0f,  1.0f, -1.0f,    1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   -1.0f,  1.0f,  1.0f,   -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   -1.0f, -1.0f,  1.0f,    1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        reinterpret_cast<void*>(0)
    );
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Skybox::LoadHDRTexture(const std::string& hdrPath)
{
    stbi_set_flip_vertically_on_load(false);

    int width = 0;
    int height = 0;
    int channels = 0;

    float* data = stbi_loadf(hdrPath.c_str(), &width, &height, &channels, 0);

    if (!data)
    {
        std::cerr << "Failed to load HDR skybox: " << hdrPath << std::endl;
        hdr_texture_ = 0;
        return;
    }

    GLenum format = GL_RGB;
    if (channels == 4)
    {
        format = GL_RGBA;
    }

    glGenTextures(1, &hdr_texture_);
    glBindTexture(GL_TEXTURE_2D, hdr_texture_);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB16F,
        width,
        height,
        0,
        format,
        GL_FLOAT,
        data
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    std::cout << "Loaded HDR skybox: " << hdrPath
        << " (" << width << "x" << height << ", channels=" << channels << ")"
        << std::endl;
}

void Skybox::Draw(const std::shared_ptr<Shader>& shader, const glm::mat4& view, const glm::mat4& projection) const
{
    if (vao_ == 0 || hdr_texture_ == 0)
    {
        return;
    }

    GLint oldDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    shader->Bind();
    shader->SetMat4(view, "uView");
    shader->SetMat4(projection, "uProjection");
    shader->SetInt(0, "uHDR");
    shader->SetFloat(exposure_, "uExposure");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_texture_);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    shader->Unbind();

    glDepthMask(GL_TRUE);
    glDepthFunc(oldDepthFunc);
}