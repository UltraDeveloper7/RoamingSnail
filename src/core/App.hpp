#pragma once

#include "../precompiled.h"
#include "../interface/Window.hpp"
#include "../interface/Camera.hpp"
#include "../core/Shader.hpp"
#include "../world/Terrain.hpp"

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

private:
	std::unique_ptr<Window> window_ = nullptr;
	std::unique_ptr<Camera> camera_ = nullptr;

	std::shared_ptr<Shader> terrain_shader_ = nullptr;
	std::unique_ptr<Terrain> terrain_ = nullptr;

	double delta_time_ = 0.0;
	double last_frame_ = 0.0;
};