#pragma once
#include "../precompiled.h"
#include "../core/Object.hpp"

class Ball final : public Object
{
public:
	explicit Ball(int number);

	int GetNumber() const { return number_; }

	void Shot(glm::vec3 power, glm::vec2 spin);
	void Roll(float dt);
	void CollideWith(const std::shared_ptr<Ball>& ball);
	void BounceOffBound(glm::vec3 surface_normal, float bound_x, float bound_z);
	void BounceOffHole(glm::vec2 surface_normal, float hole_radius);
	void HandleGravity(float min_position);
	void TakeFromHole();
	void PlaceAt(const glm::vec3& p);
	void SetDrawn(bool drawn);
	void SetSpin(glm::vec2 spin) { spin_ = spin; }

	[[nodiscard]] bool IsInHole(const std::vector<glm::vec3>& holes, float hole_radius);
	[[nodiscard]] bool IsInMotion() const { return glm::length(velocity_) > 0.003f; }
	[[nodiscard]] bool IsDrawn() const { return is_drawn_; }
	[[nodiscard]] const glm::vec3& GetHole() const { return hole_; }

	inline static constexpr float radius_{ 0.0286f };

private:
	int number_;

	bool is_in_hole_{false};
	bool is_drawn_{true};
	glm::vec3 velocity_{ 0.0f };
	glm::vec3 hole_{};
	glm::vec2 spin_{ 0.0f, 0.0f }; // Added spin member variable

	glm::vec3 last_dir_{ 1.0f, 0.0f, 0.0f }; // last horizontal direction of travel (used when speed ~ 0 for draw)
};