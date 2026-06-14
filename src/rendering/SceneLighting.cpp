#include "../precompiled.h"
#include "SceneLighting.hpp"

SceneLighting::~SceneLighting()
{
	if (shadow_map_ != 0)
	{
		glDeleteTextures(1, &shadow_map_);
		shadow_map_ = 0;
	}

	if (shadow_fbo_ != 0)
	{
		glDeleteFramebuffers(1, &shadow_fbo_);
		shadow_fbo_ = 0;
	}
}

void SceneLighting::Init()
{
	CreateShadowResources();
	UpdateLightSpaceMatrix();
}

void SceneLighting::CreateShadowResources()
{
	glGenFramebuffers(1, &shadow_fbo_);

	glGenTextures(1, &shadow_map_);
	glBindTexture(GL_TEXTURE_2D, shadow_map_);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_DEPTH_COMPONENT24,
		Config::shadow_width,
		Config::shadow_height,
		0,
		GL_DEPTH_COMPONENT,
		GL_FLOAT,
		nullptr
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D,
		shadow_map_,
		0
	);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneLighting::UpdateLightSpaceMatrix()
{
	const glm::vec3 lightDir = glm::normalize(Config::sun_direction);

	const glm::vec3 sceneCenter{ 0.0f, 0.0f, 0.0f };
	const glm::vec3 lightPos = sceneCenter - lightDir * Config::sun_shadow_distance;

	const glm::mat4 lightView = glm::lookAt(
		lightPos,
		sceneCenter,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	const float e = Config::shadow_extent;

	const glm::mat4 lightProjection = glm::ortho(
		-e,
		e,
		-e,
		e,
		Config::shadow_near_plane,
		Config::shadow_far_plane
	);

	light_space_matrix_ = lightProjection * lightView;
}

void SceneLighting::BeginShadowPass()
{
	UpdateLightSpaceMatrix();

	glViewport(0, 0, Config::shadow_width, Config::shadow_height);
	glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
	glClear(GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	// Do not cull here because vegetation cards are two-sided.
	// If we cull, alpha-card shadows become unstable or disappear.
	glDisable(GL_CULL_FACE);
}

void SceneLighting::EndShadowPass(int viewportWidth, int viewportHeight)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, viewportWidth, viewportHeight);

	glDisable(GL_CULL_FACE);
}

void SceneLighting::BindForScene(
	const std::shared_ptr<Shader>& shader,
	const glm::vec3& cameraPosition
) const
{
	shader->Bind();

	shader->SetVec3(glm::normalize(Config::sun_direction), "uSunDirection");
	shader->SetVec3(Config::sun_color, "uSunColor");
	shader->SetVec3(Config::ambient_color, "uAmbientColor");
	shader->SetVec3(cameraPosition, "uCameraPos");

	shader->SetMat4(light_space_matrix_, "lightSpaceMatrix");
	shader->SetMat4(light_space_matrix_, "uLightSpaceMatrix");

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, shadow_map_);

	shader->SetInt(9, "shadowMap");
	shader->SetInt(9, "uShadowMap");

	shader->SetBool(Config::enable_shadows, "uEnableShadows");

	shader->SetFloat(Config::shadow_bias_min, "uShadowBiasMin");
	shader->SetFloat(Config::shadow_bias_max, "uShadowBiasMax");
	shader->SetFloat(Config::shadow_strength, "uShadowStrength");

	shader->Unbind();
}