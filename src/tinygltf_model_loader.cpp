/*!
 *  \author    Viktor Zoutman
 *  \date      2019-2020
 *  \copyright GNU General Public License v3.0
 */

#include "tinygltf_model_loader.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include <tiny_gltf.h>
#include <mat4x4.hpp>
#include <functional>
#include <mat3x3.hpp>
#include <vec2.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include <utility>

#include "util/log.hpp"

TinyGLTFModelLoader::TinyGLTFModelLoader()
	: ModelLoader(std::vector<std::string>{ ".gltf" })
{

}

inline std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>> ComputeTangents(MeshData const & mesh_data)
{
	size_t num_vertices = mesh_data.m_positions.size();
	std::vector<glm::vec3> tanA(num_vertices, { 0, 0, 0 });
	std::vector<glm::vec3> tanB(num_vertices, { 0, 0, 0 });

	if (mesh_data.m_uvw.empty())
	{
		return { tanA, tanB };
	}

	// (1)
	for (size_t i = 0; i < mesh_data.m_indices.size(); i += 3) {
		size_t i0 = mesh_data.m_indices[i];
		size_t i1 = mesh_data.m_indices[i + 1];
		size_t i2 = mesh_data.m_indices[i + 2];

		glm::vec3 pos0 = mesh_data.m_positions[i0];
		glm::vec3 pos1 = mesh_data.m_positions[i1];
		glm::vec3 pos2 = mesh_data.m_positions[i2];

		// TODO is this correct?
		auto tex0 = glm::vec2{ mesh_data.m_uvw[i0].x, mesh_data.m_uvw[i0].y };
		auto tex1 = glm::vec2{ mesh_data.m_uvw[i1].x, mesh_data.m_uvw[i1].y };
		auto tex2 = glm::vec2{ mesh_data.m_uvw[i2].x, mesh_data.m_uvw[i2].y };

		glm::vec3 edge1, edge2;
		edge1 = pos1 - pos0;
		edge2 = pos2 - pos0;

		glm::vec2 uv1, uv2;
		uv1 = tex1 - tex0;
		uv2 = tex2 - tex0;

		float r = 1.0f / (uv1.x * uv2.y - uv1.y * uv2.x);

		glm::vec3 tangent(
				((edge1.x * uv2.y) - (edge2.x * uv1.y)) * r,
				((edge1.y * uv2.y) - (edge2.y * uv1.y)) * r,
				((edge1.z * uv2.y) - (edge2.z * uv1.y)) * r
		);

		glm::vec3 bitangent(
				((edge1.x * uv2.x) - (edge2.x * uv1.x)) * r,
				((edge1.y * uv2.x) - (edge2.y * uv1.x)) * r,
				((edge1.z * uv2.x) - (edge2.z * uv1.x)) * r
		);

		tanA[i0] = tangent;
		tanA[i1] = tangent;
		tanA[i2] = tangent;

		tanB[i0] = bitangent;
		tanB[i1] = bitangent;
		tanB[i2] = bitangent;

	}

	return { tanA, tanB };
}

inline void LoadMesh(ModelData* model, tinygltf::Model const & tg_model, tinygltf::Node const & node, glm::mat4 parent_transform)
{
	auto mesh = tg_model.meshes[node.mesh];

	auto translation = node.translation;
	auto rotation = node.rotation;
	auto scale = node.scale;
	auto matrix = node.matrix;

	for (auto const & primitive : mesh.primitives)
	{
		std::size_t idx_buffer_offset = 0;
		std::size_t uv_buffer_offset = 0;
		std::size_t position_buffer_offset = 0;
		std::size_t normal_buffer_offset = 0;
		MeshData mesh_data{};

		{ // GET INDICES
			auto idx_accessor = tg_model.accessors[primitive.indices];

			const auto& buffer_view = tg_model.bufferViews[idx_accessor.bufferView];
			const auto& buffer = tg_model.buffers[buffer_view.buffer];
			const auto data_address = buffer.data.data() + buffer_view.byteOffset + idx_accessor.byteOffset;
			const auto byte_stride = idx_accessor.ByteStride(buffer_view);

			mesh_data.m_indices.resize(idx_buffer_offset + idx_accessor.count);

			memcpy(mesh_data.m_indices.data() + idx_buffer_offset, data_address, idx_accessor.count * byte_stride);
			idx_buffer_offset += idx_accessor.count * byte_stride;
		}

		// Get other attributes
		for (const auto& attrib : primitive.attributes)
		{
			const auto attrib_accessor = tg_model.accessors[attrib.second];
			const auto& buffer_view = tg_model.bufferViews[attrib_accessor.bufferView];
			const auto& buffer = tg_model.buffers[buffer_view.buffer];
			const auto data_address = buffer.data.data() + buffer_view.byteOffset + attrib_accessor.byteOffset;
			const auto byte_stride = attrib_accessor.ByteStride(buffer_view);
			const auto count = attrib_accessor.count;
			if (attrib.first == "POSITION")
			{
				mesh_data.m_positions.resize(position_buffer_offset + count);
				mesh_data.m_colors.resize(position_buffer_offset + count);

				memcpy(mesh_data.m_positions.data() + position_buffer_offset, data_address, count * byte_stride);
				position_buffer_offset += count * byte_stride;
			}
			else if (attrib.first == "NORMAL")
			{
				mesh_data.m_normals.resize(normal_buffer_offset + count);

				memcpy(mesh_data.m_normals.data() + normal_buffer_offset, data_address, count * byte_stride);
				normal_buffer_offset += count * byte_stride;
			}
			else if (attrib.first == "TEXCOORD_0")
			{
				std::vector<glm::vec2> f2_data;
				f2_data.resize(count);

				memcpy(f2_data.data(), data_address, count * byte_stride);

				for (auto& f2 : f2_data)
				{
					mesh_data.m_uvw.emplace_back(glm::vec3{ f2.x, f2.y*-1, 0 });
				}

				uv_buffer_offset += count * sizeof(glm::vec3);
			}
		}

		// Apply Transformation
		for (auto& position : mesh_data.m_positions)
		{
			position = parent_transform * glm::vec4(position,1);
		}

		auto tangent_bitangent = ComputeTangents(mesh_data);
		mesh_data.m_tangents = tangent_bitangent.first;
		mesh_data.m_bitangents = tangent_bitangent.second;
		mesh_data.m_uvw.resize(mesh_data.m_positions.size());
		mesh_data.m_material_id = primitive.material;

		model->m_meshes.push_back(mesh_data);
	}
}

ModelData* TinyGLTFModelLoader::LoadFromDisc(std::string const & path)
{
	tinygltf::Model tg_model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	if (!loader.LoadASCIIFromFile(&tg_model, &err, &warn, path))
	{
		LOGC("TinyGLTF Parsing Failed");
	}

	if (!warn.empty())
	{
		LOGW("TinyGLTF Warning: {}", warn);
	}

	if (!err.empty())
	{
		LOGE("TinyGLTF Error: {}", err);
	}

	auto model = new ModelData();

	std::function<void(int, glm::mat4)> recursive_func = [&](int node_id, glm::mat4 parent_transform)
	{
		auto node = tg_model.nodes[node_id];

		auto translation = node.translation;
		auto scale = node.scale;
		auto rotation = node.rotation;
		auto matrix = node.matrix;

		glm::mat4 transform;

		if (matrix.empty())
		{
			glm::mat4 translation_mat(1);
			glm::mat4 rotation_mat(1);
			glm::mat4 scale_mat(1);
			if (!translation.empty())
			{
				translation_mat = glm::translate(translation_mat,
											glm::vec3{(float)translation[0], (float)translation[1], (float)translation[2]});
			}
			if (!rotation.empty())
			{
				rotation_mat = glm::mat4_cast(glm::quat({ (float)rotation[0], (float)rotation[1], (float)rotation[2], (float)rotation[3] }));
			}
			if (!scale.empty())
			{
				scale_mat = glm::translate(scale_mat,
										 glm::vec3{(float)scale[0], (float)scale[1], (float)scale[2]});
			}

			transform = scale_mat * rotation_mat * translation_mat;
		}
		else
		{
			transform[0][0] = matrix[0];
			transform[0][1] = matrix[1];
			transform[0][2] = matrix[2];
			transform[0][3] = matrix[3];

			transform[1][0] = matrix[4];
			transform[1][1] = matrix[5];
			transform[1][2] = matrix[6];
			transform[1][3] = matrix[7];

			transform[2][0] = matrix[8];
			transform[2][1] = matrix[9];
			transform[2][2] = matrix[10];
			transform[2][3] = matrix[11];

			transform[3][0] = matrix[12];
			transform[3][1] = matrix[13];
			transform[3][2] = matrix[14];
			transform[3][3] = matrix[15];
			//glm::transpose(transform);
		}

		parent_transform = parent_transform * transform;

		if (node.mesh > -1)
		{
			LoadMesh(model, tg_model, node, parent_transform);
		}

		for (auto child_id : node.children)
		{
			recursive_func(child_id, parent_transform);
		}
	};

	glm::mat4 parent_transform(1);
	for (auto node_id : tg_model.scenes[tg_model.defaultScene].nodes)
	{
		recursive_func(node_id, parent_transform);
	}

	return model;
}
