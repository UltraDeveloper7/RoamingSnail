#pragma once

#include "../src/precompiled.h"
#include <string>
#include <glm/vec3.hpp>

struct Config final
{
	Config() = delete;

	// ============================================================
	// Window
	// ============================================================
	inline static constexpr int width = 1920;
	inline static constexpr int height = 1080;
	inline static constexpr const char* const window_name = "Roaming Snail";

	// ============================================================
	// Camera
	// ============================================================
	inline static constexpr float fov = 64.0f * 3.14159265359f / 180.0f;
	inline static constexpr float near_clip = 0.05f;
	inline static constexpr float far_clip = 2000.0f;

	inline static constexpr float camera_movement_speed = 7.5f;
	inline static constexpr float camera_rotation_speed = 0.0016f;

	// Χαμηλή και κοντινή αρχική κάμερα, ώστε να φαίνεται το snail και το terrain relief.
	inline static constexpr glm::vec3 camera_start_position = { 0.0f, 3.2f, 14.0f };
	inline static constexpr glm::vec3 camera_start_target = { 0.0f, 1.15f, 0.0f };

	inline static constexpr float camera_start_pitch = 0.0f;
	inline static constexpr float camera_start_yaw = -1.57079632679f;

	inline static constexpr float camera_min_height = 0.35f;

	// Compatibility aliases, για παλιό κώδικα που ίσως ακόμα τα καλεί.
	inline static constexpr float movement_speed = camera_movement_speed;
	inline static constexpr float rotation_speed = camera_rotation_speed;

	// ============================================================
	// Terrain geometry
	// ============================================================
	inline static constexpr int terrain_resolution = 320;
	inline static constexpr float terrain_size = 65.0f;

	// Ύψος λόφων/κοιλάδων.
	inline static constexpr float terrain_height_scale = 0.55f;

	// Κατεβάζει όλο το procedural terrain προς το χαμηλό επίπεδο του HDR.
	inline static constexpr float terrain_vertical_offset = -1.15f;

	inline static constexpr float terrain_uv_scale = 0.10f;
	inline static constexpr float terrain_bound = terrain_size * 0.5f - 2.0f;

	// ============================================================
	// Terrain rendering / HDR blend
	inline static constexpr glm::vec3 fog_color = { 0.58f, 0.63f, 0.64f };
	inline static constexpr float fog_start = 28.0f;
	inline static constexpr float fog_end = 78.0f;

	// ============================================================
	// Snail movement
	// ============================================================
	inline static constexpr float snail_move_speed = 2.0f;
	inline static constexpr float snail_turn_speed = 2.4f;

	inline static constexpr float snail_body_height_offset = 0.30f;
	inline static constexpr float snail_shell_radius = 0.38f;

	inline static constexpr float snail_world_bound = terrain_bound;

	// ============================================================
	// Shell rolling physics
	// ============================================================
	inline static constexpr float shell_acceleration = 7.5f;
	inline static constexpr float shell_max_speed = 9.0f;
	inline static constexpr float shell_turn_strength = 2.2f;
	inline static constexpr float shell_slope_strength = 8.5f;
	inline static constexpr float shell_max_climb_slope = 35.0f * 3.14159265359f / 180.0f;

	inline static constexpr float shell_flat_friction = 0.982f;
	inline static constexpr float shell_slope_friction = 0.994f;
	inline static constexpr float shell_stop_speed = 0.015f;

	// ============================================================
	// Snail drawing / retract animation
	// ============================================================
	inline static constexpr glm::vec3 slug_body_normal_offset = { 0.0f, 0.10f, 0.0f };
	inline static constexpr glm::vec3 slug_body_retracted_offset = { 0.0f, 0.32f, 0.25f };
	inline static constexpr float slug_body_draw_scale = 0.72f;

	inline static constexpr glm::vec3 shell_draw_offset = { 0.0f, 0.38f, 0.16f };
	inline static constexpr float shell_draw_scale = 0.82f;
	inline static constexpr float shell_retract_pulse = 0.10f;

	inline static constexpr float retract_speed = 2.6f;

	// ============================================================
	// Vegetation
	// ============================================================
	inline static constexpr int vegetation_count = 180;

	inline static constexpr float vegetation_min_scale = 0.50f;
	inline static constexpr float vegetation_max_scale = 0.95f;

	inline static constexpr float vegetation_collision_radius = 0.70f;
	inline static constexpr float vegetation_shell_slowdown = 0.55f;

	inline static constexpr float vegetation_eat_boost_duration = 4.0f;
	inline static constexpr float vegetation_eat_speed_multiplier = 1.65f;


	// ============================================================
	// Assets
	// ============================================================
	inline static constexpr const char* const terrain_texture_path = "assets/textures/terrain/grass.jpg";

	inline static constexpr const char* const slug_model_path = "snail/slug_body.obj";
	inline static constexpr const char* const shell_model_path = "shell/shell.obj";
	inline static constexpr const char* const vegetation_model_path = "vegetation/dry_bush_clump.obj";

	inline static constexpr const char* const hdr_path = "rolling_hills_4k.hdr";

	// ============================================================
	// Shader paths
	// ============================================================
	inline static constexpr const char* const terrain_vertex_path = "terrain.vertexshader";
	inline static constexpr const char* const terrain_fragment_path = "terrain.fragmentshader";

	// Compatibility με Object::GetShader / παλιό material pipeline.
	inline static constexpr const char* const vertex_path = "shader.vertexshader";
	inline static constexpr const char* const fragment_path = "shader.fragmentshader";

	inline static constexpr const char* const background_vertex_path = "background.vertexshader";
	inline static constexpr const char* const background_fragment_path = "background.fragmentshader";

	inline static constexpr const char* const cubemap_vertex_path = "cubemap.vertexshader";
	inline static constexpr const char* const cubemap_fragment_path = "cubemap.fragmentshader";

	inline static constexpr const char* const irradiance_fragment_path = "irradiance.fragmentshader";
	inline static constexpr const char* const prefilter_fragment_path = "prefilter.fragmentshader";

	inline static constexpr const char* const brdf_vertex_path = "brdf.vertexshader";
	inline static constexpr const char* const brdf_fragment_path = "brdf.fragmentshader";

	inline static constexpr const char* const depth_vertex_path = "Depth.vertexshader";
	inline static constexpr const char* const depth_fragment_path = "Depth.fragmentshader";

	inline static constexpr const char* const text_vertex_path = "text.vertexshader";
	inline static constexpr const char* const text_fragment_path = "text.fragmentshader";

	inline static constexpr const char* const sprite_vertex_path = "Spriteshader.vertexshader";
	inline static constexpr const char* const sprite_fragment_path = "Spriteshader.fragmentshader";

	inline static constexpr const char* const post_vertex_path = "post.vertexshader";
	inline static constexpr const char* const blur_fragment_path = "blur.fragmentshader";
	inline static constexpr const char* const screen_fragment_path = "screen.fragmentshader";

	inline static constexpr const char* const line_vertex_path = "line.vertexshader";
	inline static constexpr const char* const line_fragment_path = "line.fragmentshader";

	// ============================================================
	// Environment / IBL / PBR
	// ============================================================
	inline static constexpr int cube_map_size = 4096;
	inline static constexpr int irradiance_scale = 128;
	inline static constexpr int prefilter_scale = 1024;
	inline static constexpr int max_mip_levels = 7;

	// ============================================================
	// Lighting / shadows
	// ============================================================
	inline static constexpr glm::vec3 sun_direction = { -0.45f, -0.78f, -0.28f };
	inline static constexpr glm::vec3 sun_color = { 2.10f, 1.78f, 1.25f };
	inline static constexpr glm::vec3 ambient_color = { 0.32f, 0.35f, 0.36f };

	inline static constexpr bool enable_shadows = true;

	inline static constexpr int shadow_width = 4096;
	inline static constexpr int shadow_height = 4096;

	inline static constexpr float shadow_extent = 48.0f;
	inline static constexpr float sun_shadow_distance = 62.0f;
	inline static constexpr float shadow_near_plane = 1.0f;
	inline static constexpr float shadow_far_plane = 145.0f;

	inline static constexpr float shadow_bias_min = 0.0020f;
	inline static constexpr float shadow_bias_max = 0.0100f;

	// 0 = no visible shadow, 1 = full dark shadow
	inline static constexpr float shadow_strength = 0.55f;

	// ============================================================
	// Font / future HUD
	// ============================================================
	inline static constexpr const char* const font_path = "NotoSans-Bold.ttf";
	inline static constexpr unsigned default_font_size = 64;

	// ============================================================
	// General
	// ============================================================
	inline static constexpr float min_change = 0.001f;
};