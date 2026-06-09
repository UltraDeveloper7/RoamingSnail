#pragma once

#include "../precompiled.h"
#include "../core/Shader.hpp"

class Skybox
{
public:
    Skybox() = default;
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    void Init(const std::string& hdrPath);
    void Draw(const std::shared_ptr<Shader>& shader, const glm::mat4& view, const glm::mat4& projection) const;

private:
    void CreateCube();
    void LoadHDRTexture(const std::string& hdrPath);

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint hdr_texture_ = 0;

    float exposure_ = 1.0f;
};