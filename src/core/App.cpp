#include "../precompiled.h"
#include "App.hpp"
#include <stb_image.h>


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

	LoadTerrainTexture();

	terrain_ = std::make_unique<Terrain>();
	terrain_->Generate(100, 20.0f);

	snail_ = std::make_unique<Snail>();
	snail_->Init();
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

	if (snail_ && terrain_)
	{
		snail_->Update(
			static_cast<float>(delta_time_),
			window_->GetGLFWWindow(),
			*terrain_
		);
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

	if (terrain_shader_ && camera_)
	{
		terrain_shader_->Bind();
		terrain_shader_->SetMat4(camera_->GetViewMatrix(), "uView");
		terrain_shader_->SetMat4(camera_->GetProjectionMatrix(), "uProjection");

		terrain_shader_->SetInt(0, "uTerrainTexture");

		// Αν έχεις SetBool στη Shader class:
		terrain_shader_->SetBool(terrain_texture_ != 0, "uUseTexture");

		// Αν ΔΕΝ έχεις SetBool, σβήσε την πάνω γραμμή και βάλε:
		// terrain_shader_->SetInt(terrain_texture_ != 0 ? 1 : 0, "uUseTexture");

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, terrain_texture_);

		terrain_shader_->Unbind();
	}

	if (terrain_ && terrain_shader_)
	{
		terrain_->Draw(terrain_shader_);
	}

	if (snail_ && terrain_shader_)
	{
		snail_->Draw(terrain_shader_);
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

void App::LoadTerrainTexture()
{
	const char* texturePath = "assets/textures/terrain/grass.jpg";

	int width = 0;
	int height = 0;
	int channels = 0;

	stbi_set_flip_vertically_on_load(true);

	unsigned char* data = stbi_load(texturePath, &width, &height, &channels, 0);

	if (!data)
	{
		std::cerr << "Failed to load terrain texture: " << texturePath << std::endl;
		terrain_texture_ = 0;
		return;
	}

	GLenum format = GL_RGB;

	if (channels == 1)
	{
		format = GL_RED;
	}
	else if (channels == 3)
	{
		format = GL_RGB;
	}
	else if (channels == 4)
	{
		format = GL_RGBA;
	}

	glGenTextures(1, &terrain_texture_);
	glBindTexture(GL_TEXTURE_2D, terrain_texture_);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		format,
		width,
		height,
		0,
		format,
		GL_UNSIGNED_BYTE,
		data
	);

	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(data);

	std::cout << "Loaded terrain texture: " << texturePath
		<< " (" << width << "x" << height << ", channels=" << channels << ")"
		<< std::endl;
}