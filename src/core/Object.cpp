#include "../precompiled.h"
#include "Object.hpp"
#include "../core/Loader.hpp"

Object::Object(const std::string& path)
{
	Loader::LoadModel(path, meshes_, materials_);
}

void Object::Draw(const std::shared_ptr<Shader>& shader)
{
	shader->Bind();

	const glm::mat4 modelMatrix = GetModelMatrix();

	shader->SetMat4(modelMatrix, "uModel");
	shader->SetMat4(modelMatrix, "modelMatrix");

	for (const auto& mesh : meshes_)
	{
		const auto material = materials_[mesh->GetMaterialId()];

		material->Bind(shader);
		mesh->Bind();
		mesh->Draw();
		mesh->Unbind();
		material->Unbind(shader);
	}

	shader->Unbind();
}

void Object::DrawWithModelMatrix(const std::shared_ptr<Shader>& shader, const glm::mat4& modelMatrix)
{
	shader->Bind();

	shader->SetMat4(modelMatrix, "uModel");
	shader->SetMat4(modelMatrix, "modelMatrix");

	for (const auto& mesh : meshes_)
	{
		const auto material = materials_[mesh->GetMaterialId()];

		material->Bind(shader);
		mesh->Bind();
		mesh->Draw();
		mesh->Unbind();
		material->Unbind(shader);
	}

	shader->Unbind();
}

void Object::DrawDepthWithModelMatrix(
	const std::shared_ptr<Shader>& shader,
	const glm::mat4& modelMatrix
)
{
	shader->Bind();

	shader->SetMat4(modelMatrix, "modelMatrix");
	shader->SetMat4(modelMatrix, "uModel");

	for (const auto& mesh : meshes_)
	{
		mesh->Bind();
		mesh->Draw();
		mesh->Unbind();
	}

	shader->Unbind();
}

void Object::DrawAlphaDepthWithModelMatrix(
	const std::shared_ptr<Shader>& shader,
	const glm::mat4& modelMatrix
)
{
	shader->Bind();

	shader->SetMat4(modelMatrix, "modelMatrix");
	shader->SetMat4(modelMatrix, "uModel");

	for (const auto& mesh : meshes_)
	{
		const auto material = materials_[mesh->GetMaterialId()];

		shader->SetBool(false, "uDepthUseAlphaCutout");
		shader->SetBool(false, "material_hasDiffuseMap");

		if (material && material->diffuse_texture)
		{
			glActiveTexture(GL_TEXTURE4);
			material->diffuse_texture->Bind();

			shader->SetBool(true, "uDepthUseAlphaCutout");
			shader->SetBool(true, "material_hasDiffuseMap");
			shader->SetInt(4, "material_diffuseMap");
		}

		mesh->Bind();
		mesh->Draw();
		mesh->Unbind();
	}

	shader->SetBool(false, "uDepthUseAlphaCutout");
	shader->SetBool(false, "material_hasDiffuseMap");

	glActiveTexture(GL_TEXTURE0);

	shader->Unbind();
}

void Object::Translate(const glm::vec3& translation)
{
	translation_ += translation;
}

void Object::Scale(const glm::vec3& scale)
{
	scale_ *= scale;
}

void Object::Rotate(const glm::vec3& rotation_axis, const float angle)
{
	if (glm::length(rotation_axis) != 0)
	{
		rotation_axis_ = rotation_axis;
		angle_ = glm::mod(angle_, glm::two_pi<float>());
		angle_ += angle;
	}
}

glm::mat4 Object::GetModelMatrix() const
{
	auto model_matrix = glm::mat4(1.0f);

	if (glm::length(translation_) > Config::min_change)
		model_matrix = glm::translate(model_matrix, translation_);

	if (glm::length(scale_) > Config::min_change)
		model_matrix = glm::scale(model_matrix, scale_);

	if (angle_ > Config::min_change)
		model_matrix = glm::rotate(model_matrix, angle_, rotation_axis_);

	return model_matrix;
}


bool Object::HasValidMesh() const {
	return !meshes_.empty();
}

std::shared_ptr<Shader> Object::GetShader()
{
	static const auto shader = std::make_shared<Shader>(Config::vertex_path, Config::fragment_path);
	return shader;
}