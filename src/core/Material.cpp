#include "../precompiled.h"
#include "Material.hpp"

void Material::Bind(const std::shared_ptr<Shader>& shader) const
{
	// Scalars / colors
	shader->SetVec3(diffuse, "material.diffuse");
	shader->SetVec3(ambient, "material.ao");
	shader->SetFloat(metallic, "material.metallic");
	shader->SetFloat(roughness, "material.roughness");
	shader->SetFloat(dissolve, "material.dissolve");

	// Reset all has* (prevents stale state if previous draw had a map)
	shader->SetBool(false, "material.hasDiffuseMap");
	shader->SetBool(false, "material.hasRoughnessMap");
	shader->SetBool(false, "material.hasNormalMap");
	shader->SetBool(false, "material.hasAoMap");
	shader->SetBool(false, "material.hasMetallicMap");

	if (diffuse_texture)
	{
		glActiveTexture(GL_TEXTURE4);
		diffuse_texture->Bind();

		shader->SetBool(true, "material.hasDiffuseMap");
		shader->SetInt(4, "material.diffuseMap");
	}

	if (roughness_texture)
	{
		glActiveTexture(GL_TEXTURE5);
		roughness_texture->Bind();

		shader->SetBool(true, "material.hasRoughnessMap");
		shader->SetInt(5, "material.roughnessMap");
	}

	if (normal_texture)
	{
		glActiveTexture(GL_TEXTURE6);
		normal_texture->Bind();

		shader->SetBool(true, "material.hasNormalMap");
		shader->SetInt(6, "material.normalMap");
	}

	if (ao_texture)
	{
		glActiveTexture(GL_TEXTURE7);
		ao_texture->Bind();

		shader->SetBool(true, "material.hasAoMap");
		shader->SetInt(7, "material.aoMap");
	}

	if (metallic_texture)
	{
		glActiveTexture(GL_TEXTURE8);
		metallic_texture->Bind();

		shader->SetBool(true, "material.hasMetallicMap");
		shader->SetInt(8, "material.metallicMap");
	}

	// Safety: leave active unit at 0 (not required but avoids surprises)
	glActiveTexture(GL_TEXTURE0);
}

void Material::Unbind(const std::shared_ptr<Shader>& shader) const
{
	// Clear flags so the next material starts clean even if it doesn’t call Bind
	if (diffuse_texture)
		shader->SetBool(false, "material.hasDiffuseMap");

	if (roughness_texture)
		shader->SetBool(false, "material.hasRoughnessMap");

	if (normal_texture)
		shader->SetBool(false, "material.hasNormalMap");

	if (ao_texture)
		shader->SetBool(false, "material.hasAoMap");

	if (metallic_texture)
		shader->SetBool(false, "material.hasMetallicMap");

	glActiveTexture(GL_TEXTURE0);
}
