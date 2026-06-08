#pragma once

#include "../precompiled.h"
#include "../core/Shader.hpp"
#include "../world/Terrain.hpp"

class Snail
{
public:
    Snail() = default;
    ~Snail();

    Snail(const Snail&) = delete;
    Snail& operator=(const Snail&) = delete;

    void Init();
    void Update(float dt, GLFWwindow* window, const Terrain& terrain);
    void Draw(const std::shared_ptr<Shader>& shader) const;

    glm::vec3 GetPosition() const { return position_; }

private:
    enum class Mode
    {
        Normal,
        Retracting,
        Shell,
        Unretracting
    };

private:
    void CreateBoxMesh();
    void HandleInput(float dt, GLFWwindow* window);
    void UpdateRetractAnimation(float dt);

    void UpdateNormalMovement(float dt, GLFWwindow* window);
    void UpdateShellPhysics(float dt, GLFWwindow* window, const Terrain& terrain);

    void UpdateOrientationFromTerrain(const Terrain& terrain);
    void ClampToTerrainBounds();
    float GetTerrainSlopeAngle(const Terrain& terrain) const;

    void UpdateShellSpin(float dt, const glm::vec3& terrainNormal);
    float GetSurfaceFriction(const Terrain& terrain) const;

    glm::mat4 BuildModelMatrix(const glm::vec3& localOffset, const glm::vec3& localScale) const;

    bool IsBodyVisible() const;
    bool IsShellOnly() const;

private:
    glm::vec3 position_{ 0.0f, 0.0f, 0.0f };
    glm::vec3 forward_{ 0.0f, 0.0f, -1.0f };
    glm::vec3 up_{ 0.0f, 1.0f, 0.0f };
    glm::vec3 right_{ 1.0f, 0.0f, 0.0f };

    float move_speed_ = 2.0f;
    float turn_speed_ = 2.5f;
    float yaw_ = 0.0f;

    float animation_time_ = 0.0f;
    float creep_amount_ = 0.0f;
    bool is_moving_ = false;

    Mode mode_ = Mode::Normal;

    float retract_progress_ = 0.0f;
    float retract_speed_ = 2.25f;
    bool r_was_pressed_ = false;

    glm::vec3 shell_velocity_{ 0.0f };
    glm::vec3 shell_angular_velocity_{ 0.0f };

    float shell_radius_ = 0.42f;
    float shell_acceleration_ = 7.0f;
    float shell_max_speed_ = 8.0f;
    float shell_friction_ = 0.985f;
    float shell_slope_strength_ = 8.5f;
    float shell_turn_strength_ = 2.6f;
    float max_shell_climb_slope_ = glm::radians(32.0f);

    glm::quat shell_rotation_{ 1.0f, 0.0f, 0.0f, 0.0f };

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    std::vector<float> vertices_;
    std::vector<unsigned int> indices_;

    glm::mat4 orientation_{ 1.0f };
};