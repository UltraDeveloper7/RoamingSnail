#include "../precompiled.h"
#include "Loader.hpp"

#include "stb_image.h"


// -----------------------------
// small file-local helpers
// -----------------------------
namespace {
	[[noreturn]] void throwf(const std::string& msg, const std::string& path) {
		throw std::exception((msg + ": " + path).c_str());
	}

	// normalize a relative path key for the cache (keeps your original key as given)
	std::string normalizeKey(const std::string& rel) {
		return rel;
	}
} 


// ============================================================================
// Model loading (OBJ via tinyobj)
// ============================================================================

void Loader::LoadModel(const std::string& path, std::vector<std::shared_ptr<Mesh>>& meshes, std::vector<std::shared_ptr<Material>>& materials)
{
	tinyobj::ObjReaderConfig reader_config;
	reader_config.vertex_color = false;
	reader_config.triangulation_method = "earcut";

	const auto model_path = std::filesystem::current_path() / "assets/models" / path;
	reader_config.mtl_search_path = model_path.parent_path().string();

	tinyobj::ObjReader reader;

	if (!reader.ParseFromFile(model_path.string(), reader_config))
	{
		std::cerr << "TinyObj failed to parse model." << std::endl;
		std::cerr << "Input path: " << path << std::endl;
		std::cerr << "Resolved path: " << model_path.string() << std::endl;
		std::cerr << "TinyObj error: " << reader.Error() << std::endl;
		std::cerr << "TinyObj warning: " << reader.Warning() << std::endl;

		throwf("Failed to load model", path);
	}

	auto& temp_materials = reader.GetMaterials();
	auto& temp_attrib = reader.GetAttrib();
	auto& temp_shapes = reader.GetShapes();

	meshes.clear();
	materials.clear();

	LoadMaterials(materials, temp_materials);
	LoadMeshes(meshes, temp_shapes, temp_attrib);
}

// ============================================================================
// Textures (8-bit + HDR)
// ============================================================================
std::shared_ptr<Texture> Loader::LoadTexture(const std::string& path)
{
	if (path.empty())
		return nullptr;

	if (unique_textures_.contains(path))
		return unique_textures_[path];

	int channels, width, height;
	const auto image_path = std::filesystem::current_path() / "assets/textures" / path;

	if (!stbi_info(image_path.string().c_str(), &width, &height, &channels))
	{
		std::cerr << "Texture not found or cannot be decoded: "
			<< image_path.string() << std::endl;
		return nullptr;
	}

	unsigned char* image_data = stbi_load(image_path.string().c_str(), &width, &height, &channels, 0);
	
	std::cout
		<< "Loaded texture: " << image_path.string()
		<< " | channels=" << channels
		<< " | size=" << width << "x" << height
		<< std::endl;

	if (!image_data)
	{
		std::cerr << "stbi_load failed for image: "
			<< image_path.string() << std::endl;
		return nullptr;
	}

	const auto texture = std::make_shared<Texture>(image_data, width, height, channels);

	stbi_image_free(image_data);

	unique_textures_.insert({ path, texture });
	return texture;
}

std::shared_ptr<Texture> Loader::LoadEnvironment(const std::string& path)
{
	int channels, width, height;
	const auto image_path = std::filesystem::current_path() / "assets/hdr" / path;

	stbi_set_flip_vertically_on_load(true);

	if (!stbi_info(image_path.string().c_str(), &width, &height, &channels)) {
		throwf("HDR cannot be found or decoded", path);
	}

	float* hdr_data = stbi_loadf(image_path.string().c_str(), &width, &height, &channels, 3);

	stbi_set_flip_vertically_on_load(false);
	if (!hdr_data) {
		throwf("stbi_loadf failed for HDR image", path);
	}

	const auto texture = std::make_shared<Texture>(hdr_data, width, height);

	stbi_image_free(hdr_data);

	return texture;
}

// ============================================================================
// Materials & Meshes (private helpers)
// ============================================================================
void Loader::LoadMaterials(std::vector<std::shared_ptr<Material>>& materials, const std::vector<tinyobj::material_t>& temp_materials)
{
	for (const auto& material : temp_materials)
	{
		materials.push_back(std::make_shared<Material>
			(
				material.name,
				glm::vec3{ material.diffuse[0], material.diffuse[1], material.diffuse[2] },
				glm::vec3{ material.ambient[0], material.ambient[1], material.ambient[2] },
				material.roughness,
				material.metallic,
				material.dissolve,
				LoadTexture(material.diffuse_texname),
				LoadTexture(material.ambient_texname),
				LoadTexture(material.roughness_texname),
				LoadTexture(material.metallic_texname),
				LoadTexture(material.alpha_texname),
				LoadTexture(material.normal_texname)
			));
	}
}

void Loader::LoadMeshes(std::vector<std::shared_ptr<Mesh>>& meshes, const std::vector<tinyobj::shape_t>& temp_shapes, const tinyobj::attrib_t& temp_attrib)
{
	meshes.reserve(temp_shapes.size());
	
	for (const auto& shape : temp_shapes)
	{
		std::unordered_map<Vertex, uint32_t> unique_vertices;
		unique_vertices.reserve(shape.mesh.indices.size());

		std::vector<Vertex> vertices{};
		std::vector<unsigned> indices{};
		vertices.reserve(shape.mesh.indices.size());
		indices.reserve(shape.mesh.indices.size());

		size_t index_offset = 0;
		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
		{
			for (size_t v = 0; v < 3; v++)
			{
				Vertex vertex{};
				const auto index = shape.mesh.indices[index_offset + v];

				vertex.position =
				{
					temp_attrib.vertices[3 * index.vertex_index + 0],
					temp_attrib.vertices[3 * index.vertex_index + 1],
					temp_attrib.vertices[3 * index.vertex_index + 2],
				};

				if (index.normal_index >= 0)
				{
					vertex.normal =
					{
						temp_attrib.normals[3 * index.normal_index + 0],
						temp_attrib.normals[3 * index.normal_index + 1],
						temp_attrib.normals[3 * index.normal_index + 2],
					};
				}

				if (index.texcoord_index >= 0)
				{
					vertex.uv =
					{
						temp_attrib.texcoords[2 * index.texcoord_index + 0],
						temp_attrib.texcoords[2 * index.texcoord_index + 1],
					};
				}

				if (!unique_vertices.contains(vertex))
				{
					unique_vertices[vertex] = static_cast<unsigned>(vertices.size());
					vertices.push_back(vertex);
				}
				indices.push_back(unique_vertices[vertex]);
			}
			index_offset += 3;
		}

		int materialId = 0;

		if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0)
		{
			materialId = shape.mesh.material_ids[0];
		}

		meshes.push_back(std::make_shared<Mesh>(vertices, indices, materialId));
	}
}


// ============================================================================
// Convenience wrappers 
// ============================================================================
std::vector<std::shared_ptr<Material>> Loader::GetMaterials(const std::string& modelPath)
{
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<Mesh>> meshes;

	// Load the model to extract its materials
	LoadModel(modelPath, meshes, materials);

	return materials;
}

std::shared_ptr<Material> Loader::GetMaterialByName(const std::string& name, const std::vector<std::shared_ptr<Material>>& materials)
{
	for (const auto& material : materials) {
		if (material->name == name) {
			return material;
		}
	}
	return nullptr; // Return nullptr if material is not found
}

void Loader::UpdateMaterialDiffuseColor(const std::string& materialName, const glm::vec3& newColor, std::vector<std::shared_ptr<Material>>& materials)
{
	auto material = GetMaterialByName(materialName, materials);
	if (material) {
		material->diffuse = newColor;
		Logger::Log("Updated material " + materialName + " diffuse color to: " + glm::to_string(newColor));
	}
	else {
		Logger::Log("Material " + materialName + " not found!");
	}
}
