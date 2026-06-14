#pragma once

#include "../precompiled.h"
#include "../core/Shader.hpp"
#include "../Config.hpp"

class SceneLighting final
{
public:
	SceneLighting() = default;
	~SceneLighting();

	void Init();

	void BeginShadowPass();
	void EndShadowPass(int viewportWidth, int viewportHeight);

	void BindForScene(
		const std::shared_ptr<Shader>& shader,
		const glm::vec3& cameraPosition
	) const;

	const glm::mat4& GetLightSpaceMatrix() const { return light_space_matrix_; }
	GLuint GetShadowMap() const { return shadow_map_; }

private:
	void CreateShadowResources();
	void UpdateLightSpaceMatrix();

private:
	GLuint shadow_fbo_ = 0;
	GLuint shadow_map_ = 0;

	glm::mat4 light_space_matrix_{ 1.0f };
};