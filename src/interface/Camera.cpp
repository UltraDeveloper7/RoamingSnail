#include "../precompiled.h"
#include "Camera.hpp"

namespace
{
	glm::vec3 DirectionFromAngles(float yaw, float pitch)
	{
		const float cp = cosf(pitch);
		const float sp = sinf(pitch);
		const float cy = cosf(yaw);
		const float sy = sinf(yaw);

		return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
	}
}

void Camera::Init()
{
	UpdateProjectionMatrix(Config::width, Config::height);

	const glm::vec3 dir = DirectionFromAngles(yaw_, pitch_);
	view_matrix_ = glm::lookAt(position_, position_ + dir, glm::vec3(0.0f, 1.0f, 0.0f));

	cursor_initialized_ = false;
}

void Camera::Update(float frame_time)
{
	GLFWwindow* window = glfwGetCurrentContext();

	if (!window)
	{
		return;
	}

	const glm::vec3 dir = DirectionFromAngles(yaw_, pitch_);

	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
	{
		Move(window, dir, Config::movement_speed * frame_time);
		Rotate(window, Config::rotation_speed * frame_time);
	}
	else
	{
		cursor_initialized_ = false;
	}

	view_matrix_ = glm::lookAt(position_, position_ + dir, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::UpdateProjectionMatrix(int width, int height)
{
	const float aspect_ratio =
		width > 0 && height > 0
		? static_cast<float>(width) / static_cast<float>(height)
		: 1.0f;

	projection_matrix_ = glm::perspective(
		Config::fov,
		aspect_ratio,
		Config::near_clip,
		Config::far_clip
	);
}

void Camera::Move(GLFWwindow* window, const glm::vec3& direction, float factor)
{
	constexpr glm::vec3 up{ 0.0f, 1.0f, 0.0f };
	const glm::vec3 right = glm::normalize(glm::cross(direction, up));

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		position_ += direction * factor;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		position_ -= direction * factor;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		position_ -= right * factor;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		position_ += right * factor;
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
	{
		position_ += up * factor;
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
	{
		position_ -= up * factor;
	}
}

void Camera::Rotate(GLFWwindow* window, float factor)
{
	double current_cursor_x = 0.0;
	double current_cursor_y = 0.0;
	glfwGetCursorPos(window, &current_cursor_x, &current_cursor_y);

	if (!cursor_initialized_)
	{
		prior_cursor_ = {
			static_cast<float>(current_cursor_x),
			static_cast<float>(current_cursor_y)
		};

		cursor_initialized_ = true;
		return;
	}

	const glm::vec2 current_cursor{
		static_cast<float>(current_cursor_x),
		static_cast<float>(current_cursor_y)
	};

	const glm::vec2 delta = current_cursor - prior_cursor_;
	prior_cursor_ = current_cursor;

	pitch_ -= delta.y * factor;
	yaw_ += delta.x * factor;

	pitch_ = glm::clamp(pitch_, -1.5f, 1.5f);
	yaw_ = glm::mod(yaw_, glm::two_pi<float>());
}