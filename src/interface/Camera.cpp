#include "../precompiled.h"
#include "Camera.hpp"

void Camera::Init()
{
	UpdateProjectionMatrix(Config::width, Config::height);

	position_ = Config::camera_start_position;

	const glm::vec3 startDirection = Config::camera_start_target - Config::camera_start_position;
	SetAnglesFromDirection(startDirection);

	GLFWwindow* window = glfwGetCurrentContext();

	if (window)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		cursor_locked_ = true;
	}

	const glm::vec3 dir = DirectionFromAngles();
	view_matrix_ = glm::lookAt(position_, position_ + dir, glm::vec3(0.0f, 1.0f, 0.0f));

	cursor_initialized_ = false;
}

glm::vec3 Camera::DirectionFromAngles() const
{
	const float cp = cosf(pitch_);
	const float sp = sinf(pitch_);
	const float cy = cosf(yaw_);
	const float sy = sinf(yaw_);

	return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

void Camera::Update(float frame_time)
{
	GLFWwindow* window = glfwGetCurrentContext();

	if (!window)
	{
		return;
	}

	const bool l_is_pressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;

	if (l_is_pressed && !l_was_pressed_)
	{
		cursor_locked_ = !cursor_locked_;

		glfwSetInputMode(
			window,
			GLFW_CURSOR,
			cursor_locked_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
		);

		cursor_initialized_ = false;
	}

	l_was_pressed_ = l_is_pressed;

	const glm::vec3 dir = DirectionFromAngles();

	if (cursor_locked_)
	{
		Move(window, dir, Config::camera_movement_speed * frame_time);
		Rotate(window, Config::camera_rotation_speed);
	}
	else
	{
		cursor_initialized_ = false;
	}

	view_matrix_ = glm::lookAt(position_, position_ + DirectionFromAngles(), glm::vec3(0.0f, 1.0f, 0.0f));
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

void Camera::SetAnglesFromDirection(const glm::vec3& direction)
{
	const glm::vec3 dir = glm::normalize(direction);

	pitch_ = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
	yaw_ = std::atan2(dir.z, dir.x);
}


void Camera::Move(GLFWwindow* window, const glm::vec3& direction, float factor)
{
	constexpr glm::vec3 up{ 0.0f, 1.0f, 0.0f };

	glm::vec3 horizontalForward = glm::vec3(direction.x, 0.0f, direction.z);

	if (glm::length(horizontalForward) < 0.0001f)
	{
		horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
	}
	else
	{
		horizontalForward = glm::normalize(horizontalForward);
	}

	const glm::vec3 right = glm::normalize(glm::cross(horizontalForward, up));

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		position_ += horizontalForward * factor;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		position_ -= horizontalForward * factor;
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

	// Keep camera above terrain enough for now.
	position_.y = glm::max(position_.y, Config::camera_min_height);
}

void Camera::Rotate(GLFWwindow* window, float sensitivity)
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

	yaw_ += delta.x * sensitivity;
	pitch_ -= delta.y * sensitivity;

	pitch_ = glm::clamp(pitch_, -1.45f, 1.25f);
	yaw_ = glm::mod(yaw_, glm::two_pi<float>());
}