#include "../precompiled.h"
#include "Vegetation.hpp"

void Vegetation::Init()
{
	plant_object_ = std::make_unique<Object>(Config::vegetation_model_path);
}

void Vegetation::Generate(const Terrain& terrain, int count, float terrainBound)
{
	if (!plant_object_)
	{
		Init();
	}

	plants_.clear();
	plants_.reserve(static_cast<size_t>(count));

	std::mt19937 rng(1337);

	std::uniform_real_distribution<float> posDist(-terrainBound, terrainBound);
	std::uniform_real_distribution<float> scaleDist(
		Config::vegetation_min_scale,
		Config::vegetation_max_scale
	);
	std::uniform_real_distribution<float> rotationDist(0.0f, glm::two_pi<float>());

	std::uniform_real_distribution<float> leanDist(-0.04f, 0.04f);

	std::uniform_real_distribution<float> widthDist(0.80f, 1.15f);
	std::uniform_real_distribution<float> heightDist(0.70f, 1.20f);

	std::uniform_real_distribution<float> tintR(0.88f, 1.08f);
	std::uniform_real_distribution<float> tintG(0.82f, 1.02f);
	std::uniform_real_distribution<float> tintB(0.55f, 0.78f);

	for (int i = 0; i < count; ++i)
	{
		Plant plant;

		plant.position.x = posDist(rng);
		plant.position.z = posDist(rng);
		plant.position.y = terrain.GetHeightAt(plant.position.x, plant.position.z);

		plant.scale = scaleDist(rng);
		plant.rotation = rotationDist(rng);

		plant.lean = glm::vec2(
			leanDist(rng),
			leanDist(rng)
		);

		plant.nonUniformScale = glm::vec3(
			widthDist(rng),
			heightDist(rng),
			widthDist(rng)
		);

		plant.colorTint = glm::vec3(
			tintR(rng),
			tintG(rng),
			tintB(rng)
		);

		plant.bend = 0.0f;
		plant.eaten = false;

		plants_.push_back(plant);
	}
}

void Vegetation::Update(float dt)
{
	for (Plant& plant : plants_)
	{
		plant.bend *= std::pow(0.88f, dt * 60.0f);

		if (std::abs(plant.bend) < 0.001f)
		{
			plant.bend = 0.0f;
		}
	}
}

void Vegetation::Draw(const std::shared_ptr<Shader>& shader) const
{
	if (!plant_object_)
	{
		return;
	}

	shader->Bind();

	shader->SetBool(false, "uUseObjectColor");
	shader->SetBool(false, "uUseVertexColor");
	shader->SetBool(true, "uUseMaterial");
	shader->SetBool(true, "uUseTexture");

	for (const Plant& plant : plants_)
	{
		if (plant.eaten)
		{
			continue;
		}

		const glm::mat4 model = BuildPlantModel(plant);
		shader->SetVec3(plant.colorTint, "uColorTint");
		plant_object_->DrawWithModelMatrix(shader, model);
	}

	shader->SetVec3(glm::vec3(1.0f), "uColorTint");

	shader->SetBool(false, "uUseMaterial");
	shader->SetBool(false, "uUseTexture");

	shader->Unbind();
}

void Vegetation::DrawDepth(const std::shared_ptr<Shader>& shader) const
{
	if (!plant_object_)
	{
		return;
	}

	for (const Plant& plant : plants_)
	{
		if (plant.eaten)
		{
			continue;
		}

		const glm::mat4 model = BuildPlantModel(plant);
		plant_object_->DrawDepthWithModelMatrix(shader, model);
	}
}

bool Vegetation::TryEat(const glm::vec3& snailPosition, float radius)
{
	for (Plant& plant : plants_)
	{
		if (plant.eaten)
		{
			continue;
		}

		const glm::vec2 snailXZ{ snailPosition.x, snailPosition.z };
		const glm::vec2 plantXZ{ plant.position.x, plant.position.z };

		if (glm::distance(snailXZ, plantXZ) <= radius)
		{
			plant.eaten = true;
			return true;
		}
	}

	return false;
}

bool Vegetation::TryHitShell(const glm::vec3& shellPosition, float radius)
{
	bool hit = false;

	for (Plant& plant : plants_)
	{
		if (plant.eaten)
		{
			continue;
		}

		const glm::vec2 shellXZ{ shellPosition.x, shellPosition.z };
		const glm::vec2 plantXZ{ plant.position.x, plant.position.z };

		if (glm::distance(shellXZ, plantXZ) <= radius)
		{
			plant.bend = 0.55f;
			hit = true;
		}
	}

	return hit;
}

glm::mat4 Vegetation::BuildPlantModel(const Plant& plant) const
{
	glm::mat4 model{ 1.0f };

	model = glm::translate(model, plant.position);

	model = glm::rotate(
		model,
		plant.rotation,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	model = glm::rotate(
		model,
		plant.lean.x,
		glm::vec3(1.0f, 0.0f, 0.0f)
	);

	model = glm::rotate(
		model,
		plant.lean.y,
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	if (plant.bend != 0.0f)
	{
		model = glm::rotate(
			model,
			plant.bend,
			glm::vec3(1.0f, 0.0f, 0.0f)
		);
	}

	model = glm::scale(model, plant.nonUniformScale * plant.scale);

	return model;
}