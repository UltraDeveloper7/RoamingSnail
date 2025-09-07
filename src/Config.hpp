#pragma once
#include "../src/precompiled.h"
#include <string>
#include <glm/vec3.hpp>

struct Config final
{
	Config() = delete;

	// Window
	inline static constexpr int width = 1920;
	inline static constexpr int height = 1080;
	inline static constexpr const char* const window_name = "8-Ball-Pool";

	// Shadow
	inline static constexpr int shadow_width = 2048;
	inline static constexpr int shadow_height = 2048;
	inline static constexpr float shadow_extent = 8.0f;
	inline static constexpr float near_plane = 1.0f;
	inline static constexpr float far_plane = 20.0f;

	// Camera
	inline static constexpr bool bound_camera = true;
	inline static float fov = 1.55f; // ~89 degrees
	inline static constexpr float near_clip = 0.001f;
	inline static constexpr float far_clip = 1000.0f;
	inline static constexpr float movement_speed = 2.0f;
	inline static constexpr float rotation_speed = 0.05f;
	inline static constexpr glm::vec3 camera_min_position = { -1.8f, 0.25f, -1.8f };
	inline static constexpr glm::vec3 camera_max_position = { 1.8f, 2.25f, 1.8f };

	// Shaders
	inline static constexpr const char* const vertex_path = "shader.vertexshader";
	inline static constexpr const char* const fragment_path = "shader.fragmentshader";
	inline static constexpr const char* const depth_vertex_path = "Depth.vertexshader";
	inline static constexpr const char* const depth_fragment_path = "Depth.fragmentshader";
	inline static constexpr const char* const text_vertex_path = "text.vertexshader";
	inline static constexpr const char* const text_fragment_path = "text.fragmentshader";
	inline static constexpr const char* const cubemap_vertex_path = "cubemap.vertexshader";
	inline static constexpr const char* const cubemap_fragment_path = "cubemap.fragmentshader";
	inline static constexpr const char* const brdf_vertex_path = "brdf.vertexshader";
	inline static constexpr const char* const brdf_fragment_path = "brdf.fragmentshader";
	inline static constexpr const char* const background_vertex_path = "background.vertexshader";
	inline static constexpr const char* const background_fragment_path = "background.fragmentshader";
	inline static constexpr const char* const irradiance_fragment_path = "irradiance.fragmentshader";
	inline static constexpr const char* const prefilter_fragment_path = "prefilter.fragmentshader";
	inline static constexpr const char* const CueMap_vertex_path = "CueMap.vertexshader";
	inline static constexpr const char* const CueMap_fragment_path = "CueMap.fragmentshader";
	inline static constexpr const char* const sprite_vertex_path = "Spriteshader.vertexshader";
	inline static constexpr const char* const sprite_fragment_path = "Spriteshader.fragmentshader";
	inline static constexpr const char* const post_vertex_path = "post.vertexshader";
	inline static constexpr const char* const blur_fragment_path = "blur.fragmentshader";
	inline static constexpr const char* const screen_fragment_path = "screen.fragmentshader";
	inline static constexpr const char* line_vertex_path = "line.vertexshader";
	inline static constexpr const char* line_fragment_path = "line.fragmentshader";




	// Model
	inline static constexpr float min_change = 0.001f;
	inline static constexpr const char* const table_path = "table.obj";
	inline static constexpr const char* const cue_path = "cue.obj";
	inline static constexpr const char* const ball_path = "ball.obj";
	inline static constexpr const char* const lightbulb_path = "lamp.obj";
	inline static constexpr const char* const ceiling_path = "ceiling.obj";


	// Lighting
	inline static constexpr int light_count = 3; // Existing lights controlled with LShift
	inline static constexpr int physical_light_count = 10; // New physical lights controlled with numpad
	inline static constexpr int max_shader_lights = 14; // Maximum number of lights in the shader
	inline static constexpr const char* const hdr_path = "billiard_hall_4k.hdr";
	//inline static constexpr const char* const hdr_path = "empty_play_room_4k.hdr";
	//inline static constexpr const char* const hdr_path = "brown_photostudio_4k.hdr"; 
	inline static constexpr int cube_map_size = 4096;
	inline static constexpr int irradiance_scale = 128;
	inline static constexpr int prefilter_scale = 1024;
	inline static constexpr int max_mip_levels = 7;

	// Font
	//inline static constexpr const char* const font_path = "CronusRound-KA6y.ttf";
	//inline static constexpr const char* const font_path = "PassagewayBold-YBgv.otf";
	//inline static constexpr const char* const font_path = "NotoSans-Regular.ttf";
	inline static constexpr const char* const font_path = "NotoSans-Bold.ttf";
	//inline static constexpr const char* const font_path = "NotoSansSymbols2-Regular.ttf";
	inline static constexpr unsigned default_font_size = 64;

	// Physics
	inline static float power_coeff = 10.0f;
	inline static float cue_rot_speed = 0.45f; // radians/sec (~34°/s). Tweak 0.35–0.85
	inline static float velocity_multiplier = 0.985f;
	inline static float spin_damping = 0.95f;          // Damping for spin
	inline static float spin_coefficient = 0.03f;     // Controls spin effect magnitude
	inline static float ball_mass = 0.165f;   // ~165 g for a pool ball

	// Spin → linear coupling
	inline static float spin_longitudinal_accel = 9.5f;   // more authority for draw/follow
	inline static float spin_lateral_accel = 1.7f;   // gentler curve

	// Rails/rim
	inline static float rail_longitudinal_keep = 0.88f;  // keep more top/back off the rail
	inline static float rail_side_flip = 1.0f;
	inline static float rail_throw_impulse = 0.08f;  // a bit subtler than before

	inline static float ball_radius = 0.0286f;  // 28.6 mm radius
	inline static float ball_restitution = 0.95f;    // elasticity in ball-ball collisions
	inline static float cushion_restitution = 0.9f;     // elasticity for cushion bounces

	// Friction & damping
	inline static float cushion_friction = 0.16f;  
	inline static float table_friction = 0.16f;  // only used in impact tangential exchange
	inline static float linear_damping = 0.985f;  // treated as "per-second": pow(0.99, dt*60), Ball friction
	inline static float angular_damping = 0.82f;  // spin lasts longer (per-second)

	inline static float spin_transfer_coef = 0.25f;     // how strongly spin is transferred in collisions

};
