#include "../precompiled.h"
#include "Terrain.hpp"

Terrain::~Terrain()
{
    if (ebo_ != 0)
    {
        glDeleteBuffers(1, &ebo_);
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

void Terrain::Generate(int resolution, float size)
{
    vertices_.clear();
    indices_.clear();

    const int vertexCount = resolution + 1;
    const float halfSize = size * 0.5f;
    const float step = size / static_cast<float>(resolution);

    vertices_.reserve(vertexCount * vertexCount);

    for (int z = 0; z < vertexCount; ++z)
    {
        for (int x = 0; x < vertexCount; ++x)
        {
            const float worldX = -halfSize + static_cast<float>(x) * step;
            const float worldZ = -halfSize + static_cast<float>(z) * step;

            Vertex vertex{};
            vertex.position = glm::vec3(worldX, 0.0f, worldZ);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            vertices_.push_back(vertex);
        }
    }

    for (int z = 0; z < resolution; ++z)
    {
        for (int x = 0; x < resolution; ++x)
        {
            const unsigned int topLeft = z * vertexCount + x;
            const unsigned int topRight = topLeft + 1;
            const unsigned int bottomLeft = (z + 1) * vertexCount + x;
            const unsigned int bottomRight = bottomLeft + 1;

            indices_.push_back(topLeft);
            indices_.push_back(bottomLeft);
            indices_.push_back(topRight);

            indices_.push_back(topRight);
            indices_.push_back(bottomLeft);
            indices_.push_back(bottomRight);
        }
    }

    if (vao_ == 0)
    {
        glGenVertexArrays(1, &vao_);
    }

    if (vbo_ == 0)
    {
        glGenBuffers(1, &vbo_);
    }

    if (ebo_ == 0)
    {
        glGenBuffers(1, &ebo_);
    }

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
        vertices_.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices_.size() * sizeof(unsigned int)),
        indices_.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position))
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, normal))
    );
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Terrain::Draw(const std::shared_ptr<Shader>& shader) const
{
    if (vao_ == 0 || indices_.empty())
    {
        return;
    }

    shader->Bind();
    shader->SetMat4(model_, "uModel");

    glBindVertexArray(vao_);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(indices_.size()),
        GL_UNSIGNED_INT,
        nullptr
    );
    glBindVertexArray(0);

    shader->Unbind();
}