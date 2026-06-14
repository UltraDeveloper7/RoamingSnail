#pragma once

#include "../precompiled.h"
#include "../core/Shader.hpp"
#include "../core/Vertex.hpp"

class Terrain
{
public:
	Terrain() = default;
	~Terrain();

	void Generate(int resolution, float size);
	void Draw(const std::shared_ptr<Shader>& shader) const;
	void DrawDepth(const std::shared_ptr<Shader>& shader) const;

	float GetHeightAt(float x, float z) const;
	glm::vec3 GetNormalAt(float x, float z) const;

	void SetTexture(GLuint textureId);
	void SetModelMatrix(const glm::mat4& model);

	int GetResolution() const { return resolution_; }
	float GetSize() const { return size_; }
	float GetHalfSize() const { return half_size_; }

private:
	float GetProceduralHeight(float x, float z) const;
	void RecalculateNormals();
	glm::vec2 TransformTileUV(glm::vec2 uv, int tileX, int tileZ) const;

private:
	GLuint vao_ = 0;
	GLuint vbo_ = 0;
	GLuint ebo_ = 0;

	GLuint texture_id_ = 0;

	int resolution_ = 0;
	float size_ = 0.0f;
	float half_size_ = 0.0f;

	glm::mat4 model_ = glm::mat4(1.0f);

	std::vector<Vertex> vertices_;
	std::vector<unsigned int> indices_;
};