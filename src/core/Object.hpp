#pragma once
#include "../precompiled.h"
#include "../core/Material.hpp"
#include "../core/Mesh.hpp"
#include "../core/Shader.hpp"
#include "Logger.hpp"

class Object
{
public:
	virtual ~Object() = default;
	Object() = default;
	explicit Object(const std::string& path);

	virtual void Draw(const std::shared_ptr<Shader>& shader);
	void DrawWithModelMatrix(const std::shared_ptr<Shader>& shader, const glm::mat4& modelMatrix);
	void DrawDepthWithModelMatrix(
		const std::shared_ptr<Shader>& shader,
		const glm::mat4& modelMatrix
	);
	void DrawAlphaDepthWithModelMatrix(
		const std::shared_ptr<Shader>& shader,
		const glm::mat4& modelMatrix
	);
	void Translate(const glm::vec3& translation);
	void Scale(const glm::vec3& scale);
	void Rotate(const glm::vec3& rotation_axis, float angle);

	glm::vec3 translation_{ 0.0f, 0.0f, 0.0f };
	glm::vec3 scale_{ 1.0f, 1.0f, 1.0f };
	glm::vec3 rotation_axis_{ 0.0f, 0.0f, 0.0f };
	float angle_{ 0.0f };

	[[nodiscard]] glm::mat4 GetModelMatrix() const;

	// Add public setter methods
	void SetMeshes(const std::vector<std::shared_ptr<Mesh>>& meshes) { meshes_ = meshes; }
	void SetMaterials(const std::vector<std::shared_ptr<Material>>& materials) { materials_ = materials; }

	// Add public getter methods
	const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const {
		return meshes_;
	}
	const std::vector<std::shared_ptr<Material>>& GetMaterials() const {
		return materials_;
	}
	static std::shared_ptr<Shader> GetShader();

	bool HasValidMesh() const;

protected:

	std::vector<std::shared_ptr<Material>> materials_{};
	std::vector<std::shared_ptr<Mesh>> meshes_{}; 
};
