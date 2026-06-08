#pragma once

#include "../precompiled.h"

class Camera
{
public:
	Camera() = default;

	void Init();
	void Update(float frame_time);
	void UpdateProjectionMatrix(int width, int height);

	glm::mat4 GetViewMatrix() const { return view_matrix_; }
	glm::mat4 GetProjectionMatrix() const { return projection_matrix_; }
	glm::vec3 GetPosition() const { return position_; }

private:
	void Move(GLFWwindow* window, const glm::vec3& direction, float factor);
	void Rotate(GLFWwindow* window, float factor);

private:
	glm::mat4 projection_matrix_{ 1.0f };
	glm::mat4 view_matrix_{ 1.0f };

	glm::vec2 prior_cursor_{ 0.0f };
	glm::vec3 position_{ 0.0f, 3.0f, 6.0f };

	float pitch_{ -0.35f };
	float yaw_{ -glm::half_pi<float>() };

	bool cursor_initialized_ = false;
};