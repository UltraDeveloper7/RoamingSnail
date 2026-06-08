#pragma once

#include "../precompiled.h"
#include "../core/Shader.hpp"

class Terrain
{
public:
    Terrain() = default;
    ~Terrain();

    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    void Generate(int resolution, float size);
    void Draw(const std::shared_ptr<Shader>& shader) const;

private:
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
    };

private:
    std::vector<Vertex> vertices_;
    std::vector<unsigned int> indices_;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    glm::mat4 model_{ 1.0f };
};