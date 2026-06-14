#pragma once

#include "../precompiled.h"
#include "../interface/Window.hpp"
#include "../interface/Camera.hpp"
#include "../interface/Menu.hpp"
#include "../interface/TextRenderer.hpp"
#include "../core/Shader.hpp"
#include "../core/Environment.hpp"
#include "../world/Terrain.hpp"
#include "../world/Vegetation.hpp"
#include "../player/Snail.hpp"
#include "../rendering/SceneLighting.hpp"



class App
{
public:
	App();
	~App();

	App(const App&) = delete;
	App(App&&) = delete;
	App& operator=(const App&) = delete;
	App& operator=(App&&) = delete;

	void Run();

private:
	void OnUpdate();
	void OnResize();
	void ProcessInput();
	void Render();
	void LoadTerrainTexture();
	void RenderShadowPass();

private:
	std::unique_ptr<Window> window_ = nullptr;
	std::unique_ptr<Camera> camera_ = nullptr;

	std::unique_ptr<Environment> environment_ = nullptr;
	std::shared_ptr<Shader> background_shader_ = nullptr;

	std::shared_ptr<Shader> terrain_shader_ = nullptr;

	std::unique_ptr<Vegetation> vegetation_ = nullptr;

	std::unique_ptr<Terrain> terrain_ = nullptr;
	std::unique_ptr<Snail> snail_ = nullptr;

	std::unique_ptr<SceneLighting> lighting_ = nullptr;
	std::shared_ptr<Shader> depth_shader_ = nullptr;

	std::unique_ptr<Menu> menu_ = nullptr;
	std::unique_ptr<TextRenderer> text_renderer_ = nullptr;

	bool has_started_ = false;
	bool paused_ = false;

	int previous_pause_state_ = GLFW_RELEASE;
	int previous_escape_state_ = GLFW_RELEASE;

	GLuint terrain_texture_ = 0;

	double delta_time_ = 0.0;
	double last_frame_ = 0.0;

	std::shared_ptr<Shader> screen_shader_ = nullptr;
	std::shared_ptr<Shader> blur_shader_ = nullptr;

	GLuint scene_fbo_ = 0;
	GLuint scene_color_texture_ = 0;
	GLuint scene_depth_rbo_ = 0;

	GLuint pingpong_fbo_[2]{ 0, 0 };
	GLuint pingpong_color_[2]{ 0, 0 };

	GLuint screen_vao_ = 0;
	GLuint screen_vbo_ = 0;

	void InitPostProcessing();
	void ResizePostProcessing(int width, int height);
	void DestroyPostProcessing();

	void BeginSceneFramebuffer();
	void EndSceneFramebuffer();

	void RenderFullscreenQuad() const;
	void RenderPostProcess(bool blurBackground);

};