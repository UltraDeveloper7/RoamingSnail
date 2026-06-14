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

float Terrain::GetProceduralHeight(float x, float z) const
{
    float height = 0.0f;

    // Large rolling hills
    height += std::sin(x * 0.16f) * 1.10f;
    height += std::cos(z * 0.14f) * 0.95f;
    height += std::sin((x + z) * 0.09f) * 1.25f;
    height += std::cos((x - z) * 0.075f) * 1.15f;

    // Medium detail
    height += std::sin(x * 0.42f + z * 0.18f) * 0.32f;
    height += std::cos(z * 0.36f - x * 0.10f) * 0.28f;

    // Central shallow valley path
    const float valleyWidth = 0.018f;
    const float valley = std::exp(-(x * x) * valleyWidth) * 1.35f;
    height -= valley;

    // Small ridge variation, subtle
    const float ridge = std::sin((x * 0.20f) + std::cos(z * 0.12f) * 2.0f) * 0.22f;
    height += ridge;

    return height * Config::terrain_height_scale + Config::terrain_vertical_offset;
}

void Terrain::SetTexture(GLuint textureId)
{
    texture_id_ = textureId;
}

void Terrain::SetModelMatrix(const glm::mat4& model)
{
    model_ = model;
}

void Terrain::Generate(int resolution, float size)
{
    resolution_ = resolution;
    size_ = size;
    half_size_ = size * 0.5f;

    vertices_.clear();
    indices_.clear();

    const int vertexCount = resolution + 1;
    const float step = size / static_cast<float>(resolution);

    vertices_.reserve(vertexCount * vertexCount);

    for (int z = 0; z < vertexCount; ++z)
    {
        for (int x = 0; x < vertexCount; ++x)
        {
            const float worldX = -half_size_ + static_cast<float>(x) * step;
            const float worldZ = -half_size_ + static_cast<float>(z) * step;

            Vertex vertex{};
            vertex.position = glm::vec3(worldX, GetProceduralHeight(worldX, worldZ), worldZ);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            const float uvScale = Config::terrain_uv_scale;
            glm::vec2 uv(worldX * uvScale, worldZ * uvScale);

            const int tileX = static_cast<int>(std::floor(worldX));
            const int tileZ = static_cast<int>(std::floor(worldZ));

            vertex.uv = uv;

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

    RecalculateNormals();

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

    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, uv))
    );
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Terrain::RecalculateNormals()
{
    for (Vertex& vertex : vertices_)
    {
        vertex.normal = glm::vec3(0.0f);
    }

    for (size_t i = 0; i + 2 < indices_.size(); i += 3)
    {
        Vertex& v0 = vertices_[indices_[i + 0]];
        Vertex& v1 = vertices_[indices_[i + 1]];
        Vertex& v2 = vertices_[indices_[i + 2]];

        const glm::vec3 edge1 = v1.position - v0.position;
        const glm::vec3 edge2 = v2.position - v0.position;

        const glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        v0.normal += normal;
        v1.normal += normal;
        v2.normal += normal;
    }

    for (Vertex& vertex : vertices_)
    {
        if (glm::length(vertex.normal) > 0.0001f)
        {
            vertex.normal = glm::normalize(vertex.normal);
        }
        else
        {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

float Terrain::GetHeightAt(float x, float z) const
{
    return GetProceduralHeight(x, z);
}

glm::vec3 Terrain::GetNormalAt(float x, float z) const
{
    const float eps = 0.1f;

    const float hL = GetHeightAt(x - eps, z);
    const float hR = GetHeightAt(x + eps, z);
    const float hD = GetHeightAt(x, z - eps);
    const float hU = GetHeightAt(x, z + eps);

    glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));
    return normal;
}

glm::vec2 Terrain::TransformTileUV(glm::vec2 uv, int tileX, int tileZ) const
{
    unsigned int seed =
        static_cast<unsigned int>(tileX * 73856093) ^
        static_cast<unsigned int>(tileZ * 19349663);

    const int rotation = static_cast<int>(seed % 4);
    const bool flipX = ((seed >> 3) & 1u) != 0u;
    const bool flipY = ((seed >> 4) & 1u) != 0u;

    glm::vec2 f = glm::fract(uv);

    if (flipX)
    {
        f.x = 1.0f - f.x;
    }

    if (flipY)
    {
        f.y = 1.0f - f.y;
    }

    glm::vec2 rotated = f;

    if (rotation == 1)
    {
        rotated = glm::vec2(f.y, 1.0f - f.x);
    }
    else if (rotation == 2)
    {
        rotated = glm::vec2(1.0f - f.x, 1.0f - f.y);
    }
    else if (rotation == 3)
    {
        rotated = glm::vec2(1.0f - f.y, f.x);
    }

    return glm::floor(uv) + rotated;
}


void Terrain::Draw(const std::shared_ptr<Shader>& shader) const
{
    if (vao_ == 0 || indices_.empty())
    {
        return;
    }

    shader->Bind();

    shader->SetMat4(model_, "uModel");
    shader->SetMat4(model_, "modelMatrix");

    shader->SetVec3(glm::vec3(1.0f), "uColorTint");

    shader->SetBool(texture_id_ != 0, "uUseTexture");
    shader->SetBool(false, "uUseObjectColor");
    shader->SetBool(false, "uUseVertexColor");
    shader->SetBool(false, "uUseMaterial");

    if (texture_id_ != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        shader->SetInt(0, "uTerrainTexture");
    }

    glBindVertexArray(vao_);

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(indices_.size()),
        GL_UNSIGNED_INT,
        nullptr
    );

    glBindVertexArray(0);

    if (texture_id_ != 0)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    shader->SetBool(false, "uUseTexture");

    shader->Unbind();
}

void Terrain::DrawDepth(const std::shared_ptr<Shader>& shader) const
{
    if (vao_ == 0 || indices_.empty())
    {
        return;
    }

    shader->Bind();

    shader->SetMat4(model_, "modelMatrix");
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