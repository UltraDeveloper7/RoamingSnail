#include "../precompiled.h"
#include "Snail.hpp"

Snail::~Snail()
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

void Snail::Init()
{
    position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    yaw_ = 0.0f;
    animation_time_ = 0.0f;
    creep_amount_ = 0.0f;
    is_moving_ = false;

    CreateBoxMesh();
}

void Snail::CreateBoxMesh()
{
    vertices_ = {
        // front
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        // back
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,

        // left
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f, 0.0f,

        // right
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f, 0.0f,

         // top
         -0.5f,  0.5f, -0.5f,  0.0f,  1.0f, 0.0f,
          0.5f,  0.5f, -0.5f,  0.0f,  1.0f, 0.0f,
          0.5f,  0.5f,  0.5f,  0.0f,  1.0f, 0.0f,
         -0.5f,  0.5f,  0.5f,  0.0f,  1.0f, 0.0f,

         // bottom
         -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,
          0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,
          0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,
         -0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f
    };

    indices_ = {
        0, 1, 2, 2, 3, 0,
        4, 6, 5, 6, 4, 7,
        8, 9, 10, 10, 11, 8,
        12, 14, 13, 14, 12, 15,
        16, 17, 18, 18, 19, 16,
        20, 22, 21, 22, 20, 23
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices_.size() * sizeof(float)),
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
        6 * sizeof(float),
        reinterpret_cast<void*>(0)
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Snail::Update(float dt, GLFWwindow* window, const Terrain& terrain)
{
    if (!window)
    {
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        yaw_ += turn_speed_ * dt;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        yaw_ -= turn_speed_ * dt;
    }

    forward_ = glm::normalize(glm::vec3(std::sin(yaw_), 0.0f, -std::cos(yaw_)));

    glm::vec3 move_dir{ 0.0f };

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        move_dir += forward_;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        move_dir -= forward_;
    }

    is_moving_ = glm::length(move_dir) > 0.001f;

    if (is_moving_)
    {
        move_dir = glm::normalize(move_dir);
        position_ += move_dir * move_speed_ * dt;
        animation_time_ += dt * 8.0f;
    }

    creep_amount_ = is_moving_ ? std::sin(animation_time_) : 0.0f;

    const float terrain_height = terrain.GetHeightAt(position_.x, position_.z);
    position_.y = terrain_height + 0.20f;

    UpdateOrientationFromTerrain(terrain);
}

void Snail::UpdateOrientationFromTerrain(const Terrain& terrain)
{
    up_ = terrain.GetNormalAt(position_.x, position_.z);

    glm::vec3 projected_forward = forward_ - glm::dot(forward_, up_) * up_;

    if (glm::length(projected_forward) < 0.001f)
    {
        projected_forward = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), up_);
    }

    projected_forward = glm::normalize(projected_forward);
    right_ = glm::normalize(glm::cross(projected_forward, up_));

    orientation_ = glm::mat4(1.0f);
    orientation_[0] = glm::vec4(right_, 0.0f);
    orientation_[1] = glm::vec4(up_, 0.0f);
    orientation_[2] = glm::vec4(-projected_forward, 0.0f);
    orientation_[3] = glm::vec4(position_, 1.0f);
}

glm::mat4 Snail::BuildModelMatrix(const glm::vec3& localOffset, const glm::vec3& localScale) const
{
    glm::vec3 worldOffset =
        right_ * localOffset.x +
        up_ * localOffset.y -
        forward_ * localOffset.z;

    glm::mat4 model = orientation_;
    model[3] = glm::vec4(position_ + worldOffset, 1.0f);

    model = glm::scale(model, localScale);

    return model;
}

void Snail::Draw(const std::shared_ptr<Shader>& shader) const
{
    if (vao_ == 0)
    {
        return;
    }

    glBindVertexArray(vao_);

    const float stretch = 1.0f + 0.08f * creep_amount_;
    const float squash = 1.0f - 0.05f * creep_amount_;

    // Main body: long and low
    {
        glm::mat4 bodyModel = BuildModelMatrix(
            glm::vec3(0.0f, 0.05f, 0.0f),
            glm::vec3(0.38f * squash, 0.18f, 0.80f * stretch)
        );

        shader->Bind();
        shader->SetMat4(bodyModel, "uModel");
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
        shader->Unbind();
    }

    // Head: slightly in front
    {
        glm::mat4 headModel = BuildModelMatrix(
            glm::vec3(0.0f, 0.14f + 0.03f * creep_amount_, -0.62f),
            glm::vec3(0.28f, 0.22f, 0.24f)
        );

        shader->Bind();
        shader->SetMat4(headModel, "uModel");
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
        shader->Unbind();
    }

    // Shell: larger block on back
    {
        glm::mat4 shellModel = BuildModelMatrix(
            glm::vec3(0.0f, 0.34f, 0.25f),
            glm::vec3(0.42f, 0.42f, 0.42f)
        );

        shader->Bind();
        shader->SetMat4(shellModel, "uModel");
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
        shader->Unbind();
    }

    glBindVertexArray(0);
}