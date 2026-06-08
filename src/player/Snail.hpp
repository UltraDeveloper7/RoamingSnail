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
    void CreateBoxMesh();
    void UpdateOrientationFromTerrain(const Terrain& terrain);
    glm::mat4 BuildModelMatrix(const glm::vec3& localOffset, const glm::vec3& localScale) const;

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

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    std::vector<float> vertices_;
    std::vector<unsigned int> indices_;

    glm::mat4 orientation_{ 1.0f };
};