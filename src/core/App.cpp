#include "../precompiled.h"
#include "App.hpp"

App::App() :
	window_(std::make_unique<Window>()),
	camera_(std::make_unique<Camera>())
{
	camera_->Init();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	glClearColor(0.10f, 0.14f, 0.18f, 1.0f);

	terrain_shader_ = std::make_shared<Shader>(
		"terrain.vertexshader",
		"terrain.fragmentshader"
	);

	terrain_ = std::make_unique<Terrain>();
	terrain_->Generate(100, 20.0f);
}

App::~App()
{
}

void App::Run()
{
	while (!window_->ShouldClose())
	{
		glfwPollEvents();

		if (window_->Resized())
		{
			OnResize();
		}

		OnUpdate();

		glfwSwapBuffers(window_->GetGLFWWindow());
	}
}

void App::OnUpdate()
{
	const double current_frame = glfwGetTime();
	delta_time_ = current_frame - last_frame_;
	last_frame_ = current_frame;

	ProcessInput();

	if (camera_)
	{
		camera_->Update(static_cast<float>(delta_time_));
	}

	Render();
}

void App::ProcessInput()
{
	GLFWwindow* glfw_window = window_->GetGLFWWindow();

	if (glfwGetKey(glfw_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		window_->SetCloseFlag();
	}
}

void App::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (terrain_ && terrain_shader_ && camera_)
	{
		terrain_shader_->Bind();
		terrain_shader_->SetMat4(camera_->GetViewMatrix(), "uView");
		terrain_shader_->SetMat4(camera_->GetProjectionMatrix(), "uProjection");
		terrain_shader_->Unbind();

		terrain_->Draw(terrain_shader_);
	}
}

void App::OnResize() const
{
	const int new_width = window_->GetWidth();
	const int new_height = window_->GetHeight();

	glViewport(0, 0, new_width, new_height);

	if (camera_)
	{
		camera_->UpdateProjectionMatrix(new_width, new_height);
	}

	window_->ResetResizedFlag();
}