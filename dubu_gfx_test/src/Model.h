#pragma once

#include <map>

#include <assimp/scene.h>
#include <dubu_gfx/dubu_gfx.h>
#include <glm/glm.hpp>

#include "Texture.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;

	static auto GetBindingDescription() {
		return vk::VertexInputBindingDescription{
		    .binding   = 0,
		    .stride    = sizeof(Vertex),
		    .inputRate = vk::VertexInputRate::eVertex,
		};
	}

	static auto GetAttributeDescriptions() {
		return std::array<vk::VertexInputAttributeDescription, 3>{
		    vk::VertexInputAttributeDescription{
		        .location = 0,
		        .binding  = 0,
		        .format   = vk::Format::eR32G32B32Sfloat,
		        .offset   = offsetof(Vertex, position),
		    },
		    vk::VertexInputAttributeDescription{
		        .location = 1,
		        .binding  = 0,
		        .format   = vk::Format::eR32G32B32Sfloat,
		        .offset   = offsetof(Vertex, normal),
		    },
		    vk::VertexInputAttributeDescription{
		        .location = 2,
		        .binding  = 0,
		        .format   = vk::Format::eR32G32Sfloat,
		        .offset   = offsetof(Vertex, texCoord),
		    },
		};
	}
};

class Mesh {
public:
	struct CreateInfo {
		vk::Device                    device         = {};
		vk::PhysicalDevice            physicalDevice = {};
		dubu::gfx::QueueFamilyIndices queueFamilies  = {};
		vk::Queue                     graphicsQueue  = {};
		std::vector<Vertex>           vertices       = {};
		std::vector<uint32_t>         indices        = {};
		uint32_t                      materialIndex  = {};
	};

public:
	Mesh(const CreateInfo& createInfo);

	[[nodiscard]] const vk::Buffer& GetVertexBuffer() const {
		return mVertexBuffer->GetBuffer();
	}
	[[nodiscard]] const vk::Buffer& GetIndexBuffer() const {
		return mIndexBuffer->GetBuffer();
	}
	[[nodiscard]] const uint32_t GetIndexCount() const { return mIndexCount; }
	[[nodiscard]] const uint32_t GetMaterialIndex() const {
		return mMaterialIndex;
	}

private:
	std::unique_ptr<dubu::gfx::Buffer> mVertexBuffer  = {};
	std::unique_ptr<dubu::gfx::Buffer> mIndexBuffer   = {};
	uint32_t                           mIndexCount    = {};
	uint32_t                           mMaterialIndex = {};
};

class Model {
public:
	struct CreateInfo {
		vk::Device                    device              = {};
		vk::PhysicalDevice            physicalDevice      = {};
		dubu::gfx::QueueFamilyIndices queueFamilies       = {};
		vk::Queue                     graphicsQueue       = {};
		std::filesystem::path         filepath            = {};
		vk::DescriptorPool            descriptorPool      = {};
		vk::DescriptorSetLayout       descriptorSetLayout = {};
	};

	struct Material {
		std::unique_ptr<dubu::gfx::DescriptorSet> mDescriptorSet = {};
	};

public:
	Model(const CreateInfo& createInfo);

	void RecordCommands(
	    vk::PipelineLayout                      pipelineLayout,
	    std::vector<dubu::gfx::DrawingCommand>& drawingCommands);

	vk::DescriptorImageInfo LoadTexture(const CreateInfo& createInfo,
	                                    const aiScene*    scene,
	                                    aiMaterial*       material,
	                                    aiTextureType     textureType,
	                                    unsigned int      index);

private:
	std::vector<Mesh>              mMeshes    = {};
	std::map<std::string, Texture> mTextures  = {};
	std::vector<Material>          mMaterials = {};
};