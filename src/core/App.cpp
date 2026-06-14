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

	glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	glClearColor(0.10f, 0.14f, 0.18f, 1.0f);

	terrain_shader_ = std::make_shared<Shader>(
		Config::terrain_vertex_path,
		Config::terrain_fragment_path
	);

	background_shader_ = std::make_shared<Shader>(
		Config::background_vertex_path,
		Config::background_fragment_path
	);

	depth_shader_ = std::make_shared<Shader>(
		Config::depth_vertex_path,
		Config::depth_fragment_path
	);

	screen_shader_ = std::make_shared<Shader>(
		Config::post_vertex_path,
		Config::screen_fragment_path
	);

	blur_shader_ = std::make_shared<Shader>(
		Config::post_vertex_path,
		Config::blur_fragment_path
	);

	environment_ = std::make_unique<Environment>();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	InitPostProcessing();

	lighting_ = std::make_unique<SceneLighting>();
	lighting_->Init();

	LoadTerrainTexture();

	terrain_ = std::make_unique<Terrain>();
	terrain_->Generate(Config::terrain_resolution, Config::terrain_size);
	terrain_->SetTexture(terrain_texture_);

	vegetation_ = std::make_unique<Vegetation>();
	vegetation_->Init();
	vegetation_->Generate(*terrain_, Config::vegetation_count, Config::terrain_bound);

	snail_ = std::make_unique<Snail>();
	snail_->Init();

	menu_ = std::make_unique<Menu>(window_->GetWidth(), window_->GetHeight());

	text_renderer_ = std::make_unique<TextRenderer>();
	text_renderer_->Init();

}

App::~App()
{
	DestroyPostProcessing();
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

	if (menu_)
	{
		menu_->Draw(has_started_, paused_);

		const MenuAction action = menu_->ConsumeAction();

		switch (action)
		{
		case MenuAction::Play:
			has_started_ = true;
			paused_ = false;
			previous_escape_state_ = GLFW_RELEASE;
			previous_pause_state_ = GLFW_RELEASE;
			break;

		case MenuAction::Resume:
			paused_ = false;
			previous_escape_state_ = GLFW_RELEASE;
			previous_pause_state_ = GLFW_RELEASE;
			break;

		case MenuAction::Reset:
			has_started_ = true;
			paused_ = false;

			if (terrain_)
			{
				vegetation_ = std::make_unique<Vegetation>();
				vegetation_->Init();
				vegetation_->Generate(*terrain_, Config::vegetation_count, Config::terrain_bound);
			}

			if (snail_)
			{
				snail_ = std::make_unique<Snail>();
				snail_->Init();
			}
			break;

		case MenuAction::Exit:
			window_->SetCloseFlag();
			break;

		default:
			break;
		}
	}

	const bool gameplayActive = has_started_ && !paused_;

	if (gameplayActive)
	{
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

		if (snail_ && vegetation_)
		{
			const glm::vec3 snailPosition = snail_->GetPosition();

			if (snail_->IsShellMode())
			{
				const bool hitVegetation = vegetation_->TryHitShell(
					snailPosition,
					Config::vegetation_collision_radius
				);

				if (hitVegetation)
				{
					snail_->SlowShell(Config::vegetation_shell_slowdown);
				}
			}
			else
			{
				const bool ateVegetation = vegetation_->TryEat(
					snailPosition,
					Config::vegetation_collision_radius
				);

				if (ateVegetation)
				{
					snail_->ApplySpeedBoost(
						Config::vegetation_eat_boost_duration,
						Config::vegetation_eat_speed_multiplier
					);
				}
			}
		}
	}

	if (vegetation_)
	{
		vegetation_->Update(static_cast<float>(delta_time_));
	}

	const bool menuMode =
		!has_started_ ||
		paused_ ||
		(menu_ && menu_->IsAnyModalOpen());

	if (menuMode)
	{
		glfwSetInputMode(window_->GetGLFWWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else
	{
		glfwSetInputMode(window_->GetGLFWWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	Render();
}

void App::ProcessInput()
{
	GLFWwindow* glfw_window = window_->GetGLFWWindow();

	const int escape_state = glfwGetKey(glfw_window, GLFW_KEY_ESCAPE);
	const int pause_state = glfwGetKey(glfw_window, GLFW_KEY_P);

	const bool escape_edge =
		escape_state == GLFW_PRESS &&
		previous_escape_state_ == GLFW_RELEASE;

	const bool pause_edge =
		pause_state == GLFW_PRESS &&
		previous_pause_state_ == GLFW_RELEASE;

	if (has_started_)
	{
		const bool modalOpen = menu_ && menu_->IsAnyModalOpen();

		if (pause_edge)
		{
			if (!modalOpen)
			{
				paused_ = !paused_;
			}
		}

		if (escape_edge)
		{
			if (!modalOpen)
			{
				paused_ = !paused_;
			}
			// If a modal is open, Menu::Draw() closes it.
			// App does not also toggle pause.
		}
	}

	previous_escape_state_ = escape_state;
	previous_pause_state_ = pause_state;
}

void App::Render()
{
	RenderShadowPass();

	BeginSceneFramebuffer();

	if (terrain_shader_ && camera_)
	{
		terrain_shader_->Bind();

		terrain_shader_->SetMat4(camera_->GetViewMatrix(), "uView");
		terrain_shader_->SetMat4(camera_->GetProjectionMatrix(), "uProjection");

		terrain_shader_->SetVec3(glm::normalize(Config::sun_direction), "uSunDirection");
		terrain_shader_->SetVec3(Config::sun_color, "uSunColor");
		terrain_shader_->SetVec3(Config::ambient_color, "uAmbientColor");

		terrain_shader_->SetVec3(camera_->GetPosition(), "uCameraPos");

		terrain_shader_->SetVec3(Config::fog_color, "uFogColor");
		terrain_shader_->SetFloat(Config::fog_start, "uFogStart");
		terrain_shader_->SetFloat(Config::fog_end, "uFogEnd");

		terrain_shader_->SetVec3(glm::vec3(1.0f), "uColorTint");

		terrain_shader_->SetInt(0, "uTerrainTexture");

		terrain_shader_->SetInt(1, "irradianceMap");
		terrain_shader_->SetInt(2, "prefilterMap");
		terrain_shader_->SetInt(3, "brdfLUT");

		terrain_shader_->SetInt(4, "material.diffuseMap");
		terrain_shader_->SetInt(5, "material.roughnessMap");
		terrain_shader_->SetInt(6, "material.normalMap");
		terrain_shader_->SetInt(7, "material.aoMap");
		terrain_shader_->SetInt(8, "material.metallicMap");

		if (environment_)
		{
			environment_->Prepare();
		}

		if (lighting_)
		{
			lighting_->BindForScene(terrain_shader_, camera_->GetPosition());
		}

		terrain_shader_->Unbind();
	}

	if (terrain_ && terrain_shader_)
	{
		terrain_->Draw(terrain_shader_);
	}

	if (vegetation_ && terrain_shader_)
	{
		vegetation_->Draw(terrain_shader_);
	}

	if (snail_ && terrain_shader_)
	{
		snail_->Draw(terrain_shader_);
	}

	if (environment_ && background_shader_ && camera_)
	{
		background_shader_->Bind();
		background_shader_->SetMat4(camera_->GetViewMatrix(), "viewMatrix");
		background_shader_->SetMat4(camera_->GetProjectionMatrix(), "projectionMatrix");
		background_shader_->Unbind();

		environment_->Draw(background_shader_);
	}

	EndSceneFramebuffer();

	const bool blurMenuBackground =
		menu_ &&
		(!has_started_ || paused_ || menu_->IsAnyModalOpen());

	RenderPostProcess(blurMenuBackground);

	if (menu_ && text_renderer_)
	{
		text_renderer_->Render(menu_->GetTextsMutable());
	}
}

void App::RenderShadowPass()
{
	if (!lighting_ || !depth_shader_)
	{
		return;
	}

	lighting_->BeginShadowPass();

	depth_shader_->Bind();
	depth_shader_->SetMat4(lighting_->GetLightSpaceMatrix(), "lightSpaceMatrix");
	depth_shader_->SetMat4(lighting_->GetLightSpaceMatrix(), "uLightSpaceMatrix");
	depth_shader_->Unbind();

	if (terrain_)
	{
		terrain_->DrawDepth(depth_shader_);
	}

	if (vegetation_)
	{
		vegetation_->DrawDepth(depth_shader_);
	}

	if (snail_)
	{
		snail_->DrawDepth(depth_shader_);
	}

	lighting_->EndShadowPass(window_->GetWidth(), window_->GetHeight());
}


void App::OnResize()
{
	const int new_width = window_->GetWidth();
	const int new_height = window_->GetHeight();

	glViewport(0, 0, new_width, new_height);

	if (camera_)
	{
		camera_->UpdateProjectionMatrix(new_width, new_height);
	}

	ResizePostProcessing(new_width, new_height);

	if (menu_)
	{
		menu_->Update(new_width, new_height);
	}

	if (text_renderer_)
	{
		text_renderer_->UpdateProjectionMatrix(new_width, new_height);
		text_renderer_->Update();
	}

	window_->ResetResizedFlag();
}

void App::LoadTerrainTexture()
{
	const char* texturePath = Config::terrain_texture_path;

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

void App::InitPostProcessing()
{
	const float quadVertices[] =
	{
		-1.0f,  1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,

		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f
	};

	glGenVertexArrays(1, &screen_vao_);
	glGenBuffers(1, &screen_vbo_);

	glBindVertexArray(screen_vao_);
	glBindBuffer(GL_ARRAY_BUFFER, screen_vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0,
		2,
		GL_FLOAT,
		GL_FALSE,
		4 * sizeof(float),
		reinterpret_cast<void*>(0)
	);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		1,
		2,
		GL_FLOAT,
		GL_FALSE,
		4 * sizeof(float),
		reinterpret_cast<void*>(2 * sizeof(float))
	);

	glBindVertexArray(0);

	ResizePostProcessing(window_->GetWidth(), window_->GetHeight());
}

void App::ResizePostProcessing(int width, int height)
{
	if (scene_fbo_ != 0)
	{
		glDeleteFramebuffers(1, &scene_fbo_);
		scene_fbo_ = 0;
	}

	if (scene_color_texture_ != 0)
	{
		glDeleteTextures(1, &scene_color_texture_);
		scene_color_texture_ = 0;
	}

	if (scene_depth_rbo_ != 0)
	{
		glDeleteRenderbuffers(1, &scene_depth_rbo_);
		scene_depth_rbo_ = 0;
	}

	if (pingpong_fbo_[0] != 0)
	{
		glDeleteFramebuffers(2, pingpong_fbo_);
		pingpong_fbo_[0] = 0;
		pingpong_fbo_[1] = 0;
	}

	if (pingpong_color_[0] != 0)
	{
		glDeleteTextures(2, pingpong_color_);
		pingpong_color_[0] = 0;
		pingpong_color_[1] = 0;
	}

	glGenFramebuffers(1, &scene_fbo_);
	glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);

	glGenTextures(1, &scene_color_texture_);
	glBindTexture(GL_TEXTURE_2D, scene_color_texture_);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB16F,
		width,
		height,
		0,
		GL_RGB,
		GL_FLOAT,
		nullptr
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D,
		scene_color_texture_,
		0
	);

	glGenRenderbuffers(1, &scene_depth_rbo_);
	glBindRenderbuffer(GL_RENDERBUFFER, scene_depth_rbo_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER,
		GL_DEPTH_STENCIL_ATTACHMENT,
		GL_RENDERBUFFER,
		scene_depth_rbo_
	);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Scene framebuffer incomplete." << std::endl;
	}

	glGenFramebuffers(2, pingpong_fbo_);
	glGenTextures(2, pingpong_color_);

	for (int i = 0; i < 2; ++i)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);

		glBindTexture(GL_TEXTURE_2D, pingpong_color_[i]);

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB16F,
			width,
			height,
			0,
			GL_RGB,
			GL_FLOAT,
			nullptr
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D,
			pingpong_color_[i],
			0
		);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cerr << "Blur framebuffer incomplete: " << i << std::endl;
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::DestroyPostProcessing()
{
	if (screen_vbo_ != 0)
	{
		glDeleteBuffers(1, &screen_vbo_);
		screen_vbo_ = 0;
	}

	if (screen_vao_ != 0)
	{
		glDeleteVertexArrays(1, &screen_vao_);
		screen_vao_ = 0;
	}

	if (scene_depth_rbo_ != 0)
	{
		glDeleteRenderbuffers(1, &scene_depth_rbo_);
		scene_depth_rbo_ = 0;
	}

	if (scene_color_texture_ != 0)
	{
		glDeleteTextures(1, &scene_color_texture_);
		scene_color_texture_ = 0;
	}

	if (scene_fbo_ != 0)
	{
		glDeleteFramebuffers(1, &scene_fbo_);
		scene_fbo_ = 0;
	}

	if (pingpong_color_[0] != 0)
	{
		glDeleteTextures(2, pingpong_color_);
		pingpong_color_[0] = 0;
		pingpong_color_[1] = 0;
	}

	if (pingpong_fbo_[0] != 0)
	{
		glDeleteFramebuffers(2, pingpong_fbo_);
		pingpong_fbo_[0] = 0;
		pingpong_fbo_[1] = 0;
	}
}

void App::BeginSceneFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);
	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	glClearColor(0.10f, 0.14f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void App::EndSceneFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());
}

void App::RenderFullscreenQuad() const
{
	glBindVertexArray(screen_vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void App::RenderPostProcess(bool blurBackground)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	GLuint finalTexture = scene_color_texture_;

	const int blurAmount = blurBackground
		? (!has_started_ ? 2 : 5)
		: 0;

	if (blurAmount > 0 && blur_shader_)
	{
		bool horizontal = true;
		bool firstIteration = true;
		int lastTarget = 0;

		const glm::vec2 texel(
			1.0f / static_cast<float>(std::max(window_->GetWidth(), 1)),
			1.0f / static_cast<float>(std::max(window_->GetHeight(), 1))
		);

		blur_shader_->Bind();
		blur_shader_->SetInt(0, "src");
		blur_shader_->SetVec2(texel, "texel");

		for (int i = 0; i < blurAmount; ++i)
		{
			const int target = horizontal ? 1 : 0;
			const int source = horizontal ? 0 : 1;

			glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[target]);
			glViewport(0, 0, window_->GetWidth(), window_->GetHeight());
			glClear(GL_COLOR_BUFFER_BIT);

			const glm::vec2 dir = horizontal
				? glm::vec2(1.0f, 0.0f)
				: glm::vec2(0.0f, 1.0f);

			blur_shader_->SetVec2(dir, "dir");

			glActiveTexture(GL_TEXTURE0);

			if (firstIteration)
			{
				glBindTexture(GL_TEXTURE_2D, scene_color_texture_);
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, pingpong_color_[source]);
			}

			RenderFullscreenQuad();

			lastTarget = target;
			horizontal = !horizontal;
			firstIteration = false;
		}

		blur_shader_->Unbind();

		finalTexture = pingpong_color_[lastTarget];
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	glClearColor(0.10f, 0.14f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	screen_shader_->Bind();

	screen_shader_->SetInt(0, "src");

	// Old repo screen shader optional controls.
	// Neutral defaults: image passes through unless we intentionally enable effects.
	screen_shader_->SetVec4(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), "tint");
	screen_shader_->SetFloat(blurBackground ? 0.28f : 0.08f, "vignette");

	screen_shader_->SetFloat(0.0f, "aberration");
	screen_shader_->SetFloat(0.0f, "sharpen");
	screen_shader_->SetFloat(0.0f, "grain");
	screen_shader_->SetFloat(0.0f, "scanlines");

	screen_shader_->SetFloat(1.00f, "saturation");
	screen_shader_->SetFloat(1.00f, "contrast");

	// Keep gamma neutral because your scene shaders already tone-map/gamma.
	// If later you remove gamma from terrain/background, set this to 2.2.
	screen_shader_->SetFloat(1.00f, "gamma");

	screen_shader_->SetFloat(static_cast<float>(glfwGetTime()), "time");

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, finalTexture);

	RenderFullscreenQuad();

	glBindTexture(GL_TEXTURE_2D, 0);
	screen_shader_->Unbind();

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}