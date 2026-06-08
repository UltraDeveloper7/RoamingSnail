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

    mode_ = Mode::Normal;
    retract_progress_ = 0.0f;
    r_was_pressed_ = false;

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

    HandleInput(dt, window);
    UpdateRetractAnimation(dt);

    if (mode_ == Mode::Shell)
    {
        UpdateShellPhysics(dt, window, terrain);
    }
    else
    {
        UpdateNormalMovement(dt, window);
    }

    ClampToTerrainBounds();

    const float terrain_height = terrain.GetHeightAt(position_.x, position_.z);

    const float shellHeightOffset = shell_radius_;
    const float bodyHeightOffset = 0.20f;
    const float targetOffset = IsShellOnly() ? shellHeightOffset : bodyHeightOffset;

    position_.y = terrain_height + targetOffset;

    UpdateOrientationFromTerrain(terrain);
}
void Snail::HandleInput(float dt, GLFWwindow* window)
{
    (void)dt;

    const bool r_is_pressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

    if (r_is_pressed && !r_was_pressed_)
    {
        if (mode_ == Mode::Normal)
        {
            mode_ = Mode::Retracting;
            shell_velocity_ = glm::vec3(0.0f);
        }
        else if (mode_ == Mode::Shell)
        {
            mode_ = Mode::Unretracting;
            shell_velocity_ = glm::vec3(0.0f);
        }
    }

    r_was_pressed_ = r_is_pressed;
}

void Snail::UpdateNormalMovement(float dt, GLFWwindow* window)
{
    if (mode_ != Mode::Normal && mode_ != Mode::Retracting && mode_ != Mode::Unretracting)
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

        if (mode_ == Mode::Normal)
        {
            animation_time_ += dt * 8.0f;
        }
    }

    creep_amount_ = mode_ == Mode::Normal && is_moving_
        ? std::sin(animation_time_)
        : 0.0f;
}

void Snail::UpdateShellPhysics(float dt, GLFWwindow* window, const Terrain& terrain)
{
    const glm::vec3 terrain_normal = terrain.GetNormalAt(position_.x, position_.z);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        yaw_ += shell_turn_strength_ * dt;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        yaw_ -= shell_turn_strength_ * dt;
    }

    forward_ = glm::normalize(glm::vec3(std::sin(yaw_), 0.0f, -std::cos(yaw_)));

    glm::vec3 control_force{ 0.0f };

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        control_force += forward_ * shell_acceleration_;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        control_force -= forward_ * shell_acceleration_ * 0.65f;
    }

    const float slope_angle = GetTerrainSlopeAngle(terrain);

    if (slope_angle > max_shell_climb_slope_)
    {
        const glm::vec3 uphill_component =
            terrain_normal * glm::dot(control_force, terrain_normal);

        control_force -= uphill_component;
        control_force *= 0.35f;
    }

    const glm::vec3 gravity = glm::vec3(0.0f, -1.0f, 0.0f) * shell_slope_strength_;
    const glm::vec3 slope_force = gravity - glm::dot(gravity, terrain_normal) * terrain_normal;

    shell_velocity_ += (control_force + slope_force) * dt;

    shell_velocity_.y = 0.0f;

    const float speed = glm::length(shell_velocity_);

    if (speed > shell_max_speed_)
    {
        shell_velocity_ = glm::normalize(shell_velocity_) * shell_max_speed_;
    }

    position_ += shell_velocity_ * dt;

    const float frictionFactor = std::pow(shell_friction_, dt * 60.0f);
    shell_velocity_ *= frictionFactor;

    if (glm::length(shell_velocity_) < 0.015f)
    {
        shell_velocity_ = glm::vec3(0.0f);
    }

    if (glm::length(shell_velocity_) > 0.001f)
    {
        const glm::vec3 velocity_dir = glm::normalize(shell_velocity_);
        shell_rotation_axis_ = glm::normalize(glm::cross(terrain_normal, velocity_dir));

        const float distance = glm::length(shell_velocity_) * dt;
        shell_rotation_angle_ += distance / shell_radius_;

        shell_angular_velocity_ = shell_rotation_axis_ * (glm::length(shell_velocity_) / shell_radius_);
    }
    else
    {
        shell_angular_velocity_ = glm::vec3(0.0f);
    }

    is_moving_ = glm::length(shell_velocity_) > 0.05f;
    creep_amount_ = 0.0f;
}

void Snail::ClampToTerrainBounds()
{
    const float bound = 9.5f;

    position_.x = glm::clamp(position_.x, -bound, bound);
    position_.z = glm::clamp(position_.z, -bound, bound);

    if (position_.x <= -bound || position_.x >= bound)
    {
        shell_velocity_.x *= -0.35f;
    }

    if (position_.z <= -bound || position_.z >= bound)
    {
        shell_velocity_.z *= -0.35f;
    }
}

float Snail::GetTerrainSlopeAngle(const Terrain& terrain) const
{
    const glm::vec3 normal = terrain.GetNormalAt(position_.x, position_.z);
    const float d = glm::clamp(glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);

    return std::acos(d);
}

void Snail::UpdateRetractAnimation(float dt)
{
    if (mode_ == Mode::Retracting)
    {
        retract_progress_ += retract_speed_ * dt;

        if (retract_progress_ >= 1.0f)
        {
            retract_progress_ = 1.0f;
            mode_ = Mode::Shell;
        }
    }
    else if (mode_ == Mode::Unretracting)
    {
        retract_progress_ -= retract_speed_ * dt;

        if (retract_progress_ <= 0.0f)
        {
            retract_progress_ = 0.0f;
            mode_ = Mode::Normal;
        }
    }
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

bool Snail::IsBodyVisible() const
{
    return retract_progress_ < 0.98f;
}

bool Snail::IsShellOnly() const
{
    return mode_ == Mode::Shell || mode_ == Mode::Retracting;
}

void Snail::Draw(const std::shared_ptr<Shader>& shader) const
{
    if (vao_ == 0)
    {
        return;
    }

    glBindVertexArray(vao_);

    const float bodyVisibility = 1.0f - retract_progress_;

    const float stretch = 1.0f + 0.08f * creep_amount_;
    const float squash = 1.0f - 0.05f * creep_amount_;

    if (IsBodyVisible())
    {
        // Body retracts backward and shrinks into the shell.
        {
            const glm::vec3 bodyOffset = glm::mix(
                glm::vec3(0.0f, 0.05f, 0.0f),
                glm::vec3(0.0f, 0.22f, 0.25f),
                retract_progress_
            );

            const glm::vec3 bodyScale =
                glm::vec3(0.38f * squash, 0.18f, 0.80f * stretch) *
                glm::vec3(1.0f, bodyVisibility, bodyVisibility);

            glm::mat4 bodyModel = BuildModelMatrix(bodyOffset, bodyScale);

            shader->Bind();
            shader->SetMat4(bodyModel, "uModel");
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
            shader->Unbind();
        }

        // Head retracts faster into the shell.
        {
            const glm::vec3 headOffset = glm::mix(
                glm::vec3(0.0f, 0.14f + 0.03f * creep_amount_, -0.62f),
                glm::vec3(0.0f, 0.28f, 0.18f),
                retract_progress_
            );

            const float headScaleFactor = glm::max(0.05f, bodyVisibility);

            glm::mat4 headModel = BuildModelMatrix(
                headOffset,
                glm::vec3(0.28f, 0.22f, 0.24f) * headScaleFactor
            );

            shader->Bind();
            shader->SetMat4(headModel, "uModel");
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
            shader->Unbind();
        }
    }

    // Shell is always visible. It slightly grows while retracting to make the transition readable.
    {
        const float shellPulse = 1.0f + 0.10f * retract_progress_;

        glm::mat4 shellModel = BuildModelMatrix(
            glm::vec3(0.0f, 0.34f, 0.25f),
            glm::vec3(0.42f, 0.42f, 0.42f) * shellPulse
        );

        if (mode_ == Mode::Shell)
        {
            shellModel = glm::rotate(shellModel, shell_rotation_angle_, shell_rotation_axis_);
        }

        shader->Bind();
        shader->SetMat4(shellModel, "uModel");
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
        shader->Unbind();
    }

    glBindVertexArray(0);
}