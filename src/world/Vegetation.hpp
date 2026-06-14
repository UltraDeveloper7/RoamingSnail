#pragma once

#include "../precompiled.h"
#include "../core/Shader.hpp"
#include "../core/Object.hpp"
#include "../world/Terrain.hpp"

class Vegetation
{
public:
	struct Plant
	{
		glm::vec3 position{ 0.0f };

		float scale = 1.0f;
		float rotation = 0.0f;
		glm::vec2 lean{ 0.0f };
		float bend = 0.0f;

		glm::vec3 nonUniformScale{ 1.0f };
		glm::vec3 colorTint{ 1.0f };

		bool eaten = false;
	};

public:
	Vegetation() = default;
	~Vegetation() = default;

	void Init();
	void Generate(const Terrain& terrain, int count, float terrainBound);
	void Update(float dt);
	void Draw(const std::shared_ptr<Shader>& shader) const;
	void DrawDepth(const std::shared_ptr<Shader>& shader) const;

	bool TryEat(const glm::vec3& snailPosition, float radius);
	bool TryHitShell(const glm::vec3& shellPosition, float radius);

private:
	glm::mat4 BuildPlantModel(const Plant& plant) const;

private:
	std::unique_ptr<Object> plant_object_ = nullptr;
	std::vector<Plant> plants_;
};