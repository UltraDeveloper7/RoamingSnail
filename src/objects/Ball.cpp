#include "../precompiled.h"
#include "Ball.hpp"
#include "../core/Loader.hpp"

Ball::Ball(const int number) : Object(Config::ball_path), spin_(0.0f, 0.0f), number_(number)
{
    const auto path = std::filesystem::current_path() / ("assets/textures/ball" + std::to_string(number) + ".jpg");
    materials_[0]->diffuse_texture = Loader::LoadTexture(path.string());
}

void Ball::Shot(const glm::vec3 power, const glm::vec2 spin)
{
    if (!IsInMotion()) {
        velocity_ = power;
        spin_ = spin;

        // remember direction of travel (horizontal component)
        glm::vec3 horiz = glm::vec3(velocity_.x, 0.0f, velocity_.z);
        if (glm::length(horiz) > Config::min_change)
            last_dir_ = glm::normalize(horiz);
    }
}

void Ball::Roll(const float dt)
{
    // Integrate position
    Translate(velocity_ * dt);

    // Update last_dir_ from horizontal velocity if meaningful
    glm::vec3 horiz_v = { velocity_.x, 0.0f, velocity_.z };
    const float speed = glm::length(horiz_v);
    if (speed > Config::min_change) {
        last_dir_ = glm::normalize(horiz_v);
    }

    // Spin-driven accelerations
    const glm::vec3 up(0, 1, 0);
    const glm::vec3 forward = last_dir_;
    const glm::vec3 right = glm::normalize(glm::cross(up, forward)); // right-handed

    // +Y topspin (follow), -Y backspin (draw). +X right english, -X left.
    const float a_long = Config::spin_longitudinal_accel * spin_.y;
    const float a_side = Config::spin_lateral_accel * spin_.x;

    velocity_ += forward * (a_long * dt);
    velocity_ += right * (a_side * dt);

    // If we are essentially stopped but still have strong backspin,
    // give a small impulse to start the draw motion.
    if (glm::length(horiz_v) < 0.02f && spin_.y < -0.15f) {
        const float draw_kick = 0.18f * (-spin_.y); // tweakable
        velocity_ += -forward * draw_kick;
    }

    // Visual rolling
    const glm::vec3 rot_axis = (speed > Config::min_change)
        ? glm::cross(up, glm::normalize(horiz_v))
        : glm::vec3(0.0f);
    const float rot_angle = glm::length(horiz_v) * dt / radius_;
    Rotate(rot_axis, rot_angle);

    // Softer, frame-rate independent damping (per-second style)
    velocity_ *= std::pow(Config::linear_damping, dt * 60.0f);
    spin_ *= std::pow(Config::angular_damping, dt * 60.0f);

    if (!IsInMotion())
        velocity_ = glm::vec3(0.0f);
}

void Ball::CollideWith(const std::shared_ptr<Ball>& ball)
{
    // Separation / basis
    glm::vec3 n = translation_ - ball->translation_;
    float n_len = glm::length(n);
    if (n_len > radius_ * 2.0f) return;

    glm::vec3 un = (n_len > 0.0f) ? (n / n_len) : glm::vec3(1, 0, 0);

    // Separate so they don't overlap
    glm::vec3 mtv = un * (radius_ * 2.0f - n_len);
    translation_ += 0.5f * mtv;
    ball->translation_ -= 0.5f * mtv;

    // Tangent along cloth
    glm::vec3 ut = glm::vec3(-un.z, 0.0f, un.x);

    // Decompose velocities (equal masses)
    float v1n = glm::dot(un, velocity_);
    float v1t = glm::dot(ut, velocity_);
    float v2n = glm::dot(un, ball->velocity_);
    float v2t = glm::dot(ut, ball->velocity_);

    // Normal exchange with restitution
    const float e = Config::ball_restitution;     // 0.95 default
    float v1n_after = e * v2n;
    float v2n_after = e * v1n;

    // Small tangential exchange (cloth slip at contact)
    const float tf = 0.5f * Config::table_friction; // e.g. 0.06 if table_friction=0.12
    float dv_t = v1t - v2t;
    float v1t_after = v1t - tf * dv_t;
    float v2t_after = v2t + tf * dv_t;

    // Recompose
    velocity_ = un * v1n_after + ut * v1t_after;
    ball->velocity_ = un * v2n_after + ut * v2t_after;

    // ---------- SPIN HANDLING ----------
    // Keep MOST of cue's longitudinal spin (this is what gives draw/follow).
    // Reduce a little due to impact losses; do NOT swap spins.
    spin_.y *= 0.97f;  // slight loss only
    ball->spin_.y *= 1.00f;  // object ball keeps its own (usually ~0)

    // Transfer a bit of SIDE spin based on tangential slip at contact (feels natural).
    float side_transfer = Config::spin_transfer_coef * dv_t; // 0.4 * dv_t by default
    spin_.x -= side_transfer;
    ball->spin_.x += side_transfer;

    // Clamp to a sane range
    spin_.x = glm::clamp(spin_.x, -1.5f, 1.5f);
    spin_.y = glm::clamp(spin_.y, -1.5f, 1.5f);
    ball->spin_.x = glm::clamp(ball->spin_.x, -1.5f, 1.5f);
    ball->spin_.y = glm::clamp(ball->spin_.y, -1.5f, 1.5f);
}


void Ball::BounceOffBound(const glm::vec3 surface_normal, const float bound_x, const float bound_z)
{
    // Decompose velocity into normal/tangent
    glm::vec3 n = glm::normalize(surface_normal);
    float vn = glm::dot(velocity_, n);
    glm::vec3 vN = vn * n;
    glm::vec3 vT = velocity_ - vN;

    const float e = Config::cushion_restitution; // normal restitution
    const float mu = Config::cushion_friction;    // tangential loss

    // Bounce with restitution + friction
    glm::vec3 vN2 = -e * vN;
    glm::vec3 vT2 = vT * (1.0f - mu);

    velocity_ = vN2 + vT2;

    // Clamp position back to the rail plane (like you did)
    if (std::abs(n.x) > Config::min_change)
        translation_.x = -n.x * bound_x;
    else if (std::abs(n.z) > Config::min_change)
        translation_.z = -n.z * bound_z;

    // Build right/forward to apply spin effects
    const glm::vec3 up(0, 1, 0);
    glm::vec3 fwd = glm::vec3(velocity_.x, 0.0f, velocity_.z);
    if (glm::length(fwd) < Config::min_change) fwd = last_dir_;
    else                                       fwd = glm::normalize(fwd);
    glm::vec3 right = glm::normalize(glm::cross(up, fwd));

    // Keep old side spin for rail-throw direction
    const float side_before = spin_.x;

    // Spin on rail: flip side, keep some top/back
    spin_.x = -spin_.x * Config::rail_side_flip;
    spin_.y *= Config::rail_longitudinal_keep;

    // Rail throw: a small sideways velocity from english
    velocity_ += right * (Config::rail_throw_impulse * side_before);
}


void Ball::BounceOffHole(const glm::vec2 surface_normal, const float hole_radius)
{
    const float keepY = translation_.y;

    // 2D normal (x,z), and rim tangent
    glm::vec2 n2 = glm::normalize(surface_normal);
    glm::vec2 t2 = glm::vec2(-n2.y, n2.x); // tangent around the rim

    // Decompose horizontal velocity into normal/tangent
    glm::vec2 v2(velocity_.x, velocity_.z);
    float vn = glm::dot(v2, n2);
    glm::vec2 vN = vn * n2;
    glm::vec2 vT = v2 - vN;

    const float e = Config::cushion_restitution; // reuse cushion coeff for rim
    const float mu = Config::cushion_friction;

    glm::vec2 vN2 = -e * vN;
    glm::vec2 vT2 = vT * (1.0f - mu);

    glm::vec2 v2p = vN2 + vT2;
    velocity_.x = v2p.x;
    velocity_.z = v2p.y;

    // Snap ball to rim, preserving height
    glm::vec3 push_dir = glm::vec3(-n2.x, 0.0f, -n2.y); // away from rim center
    translation_ = hole_ + push_dir * (hole_radius - radius_);
    translation_.y = keepY;

    // Spin effects on rim: flip side, keep some top/back, and a small tangent throw
    const float side_before = spin_.x;

    spin_.x = -spin_.x * Config::rail_side_flip;
    spin_.y *= Config::rail_longitudinal_keep;

    glm::vec3 t3(t2.x, 0.0f, t2.y);
    velocity_ += t3 * (Config::rail_throw_impulse * side_before);
}


void Ball::HandleGravity(const float min_position)
{
    if (translation_.y > min_position + radius_ + Config::min_change)
        velocity_.y -= 0.05f;
    else
        velocity_.y = 0.0f;

    translation_.y = glm::clamp(translation_.y, min_position + radius_, radius_);
}

void Ball::TakeFromHole()
{
    is_in_hole_ = false;
    velocity_ = glm::vec3(0.0f);
    translation_ = glm::vec3(0.0f, radius_, 0.0f);
    spin_ = glm::vec2(0.0f);
}

void Ball::PlaceAt(const glm::vec3& p)
{
    is_in_hole_ = false;
    is_drawn_ = true;
    translation_ = p;
    velocity_ = glm::vec3(0.0f);
    spin_ = glm::vec2(0.0f);
}

void Ball::SetDrawn(const bool drawn)
{
    is_drawn_ = drawn;
}

bool Ball::IsInHole(const std::vector<glm::vec3>& holes, const float hole_radius)
{
    for (const auto& hole : holes)
    {
        if (glm::distance(translation_, hole) < hole_radius)
        {
            hole_ = hole;
            is_in_hole_ = true;
        }
    }

    return is_in_hole_;
}
