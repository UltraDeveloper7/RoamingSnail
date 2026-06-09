#pragma once

#include "../precompiled.h"
#include "../interface/Window.hpp"
#include "../interface/Camera.hpp"
#include "../core/Shader.hpp"
#include "../world/Terrain.hpp"
#include "../world/Skybox.hpp"
#include "../player/Snail.hpp"


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
	void OnResize() const;
	void ProcessInput();
	void Render();
	void LoadTerrainTexture();

private:
	std::unique_ptr<Window> window_ = nullptr;
	std::unique_ptr<Camera> camera_ = nullptr;

	std::shared_ptr<Shader> terrain_shader_ = nullptr;
	std::shared_ptr<Shader> skybox_shader_ = nullptr;

	std::unique_ptr<Terrain> terrain_ = nullptr;
	std::unique_ptr<Skybox> skybox_ = nullptr;
	std::unique_ptr<Snail> snail_ = nullptr;


	GLuint terrain_texture_ = 0;

	double delta_time_ = 0.0;
	double last_frame_ = 0.0;
};