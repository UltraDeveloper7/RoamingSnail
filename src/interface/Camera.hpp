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

	void SetPosition(const glm::vec3& position) { position_ = position; }

private:
	glm::vec3 DirectionFromAngles() const;
	void SetAnglesFromDirection(const glm::vec3& direction);
	void Move(GLFWwindow* window, const glm::vec3& direction, float factor);
	void Rotate(GLFWwindow* window, float sensitivity);

private:
	glm::mat4 projection_matrix_{ 1.0f };
	glm::mat4 view_matrix_{ 1.0f };

	glm::vec2 prior_cursor_{ 0.0f };
	glm::vec3 position_{ Config::camera_start_position };

	float pitch_{ Config::camera_start_pitch };
	float yaw_{ Config::camera_start_yaw };

	bool cursor_initialized_ = false;
	bool cursor_locked_ = true;
	bool l_was_pressed_ = false;
};