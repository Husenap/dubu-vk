#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>

glm::vec3 AiToGlm(aiVector3D v) {
	return {v.x, v.y, v.z};
}

Mesh::Mesh(const CreateInfo& createInfo) {
	{
		dubu::gfx::Buffer stagingBuffer(dubu::gfx::Buffer::CreateInfo{
		    .device         = createInfo.device,
		    .physicalDevice = createInfo.physicalDevice,
		    .size           = static_cast<uint32_t>(createInfo.vertices.size() *
                                          sizeof(createInfo.vertices[0])),
		    .usage          = vk::BufferUsageFlagBits::eTransferSrc,
		    .sharingMode    = vk::SharingMode::eExclusive,
		    .memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible |
		                        vk::MemoryPropertyFlagBits::eHostCoherent,
		});

		stagingBuffer.SetData(createInfo.vertices);

		mVertexBuffer =
		    std::make_unique<dubu::gfx::Buffer>(dubu::gfx::Buffer::CreateInfo{
		        .device         = createInfo.device,
		        .physicalDevice = createInfo.physicalDevice,
		        .size  = static_cast<uint32_t>(createInfo.vertices.size() *
                                              sizeof(createInfo.vertices[0])),
		        .usage = vk::BufferUsageFlagBits::eVertexBuffer |
		                 vk::BufferUsageFlagBits::eTransferDst,
		        .sharingMode      = vk::SharingMode::eExclusive,
		        .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
		    });

		mVertexBuffer->SetData(stagingBuffer.GetBuffer(),
		                       createInfo.queueFamilies,
		                       createInfo.graphicsQueue);
	}
	{
		dubu::gfx::Buffer stagingBuffer(dubu::gfx::Buffer::CreateInfo{
		    .device         = createInfo.device,
		    .physicalDevice = createInfo.physicalDevice,
		    .size           = static_cast<uint32_t>(createInfo.indices.size() *
                                          sizeof(createInfo.indices[0])),
		    .usage          = vk::BufferUsageFlagBits::eTransferSrc,
		    .sharingMode    = vk::SharingMode::eExclusive,
		    .memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible |
		                        vk::MemoryPropertyFlagBits::eHostCoherent,
		});

		stagingBuffer.SetData(createInfo.indices);

		mIndexBuffer =
		    std::make_unique<dubu::gfx::Buffer>(dubu::gfx::Buffer::CreateInfo{
		        .device         = createInfo.device,
		        .physicalDevice = createInfo.physicalDevice,
		        .size  = static_cast<uint32_t>(createInfo.indices.size() *
                                              sizeof(createInfo.indices[0])),
		        .usage = vk::BufferUsageFlagBits::eIndexBuffer |
		                 vk::BufferUsageFlagBits::eTransferDst,
		        .sharingMode      = vk::SharingMode::eExclusive,
		        .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
		    });

		mIndexBuffer->SetData(stagingBuffer.GetBuffer(),
		                      createInfo.queueFamilies,
		                      createInfo.graphicsQueue);
	}

	mIndexCount = static_cast<uint32_t>(createInfo.indices.size());
}

Model::Model(const CreateInfo& createInfo) {
	Assimp::Importer importer;
	const aiScene*   scene = importer.ReadFile(
        createInfo.filepath.string().c_str(),
        aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs);

	if (!scene) {
		DUBU_LOG_ERROR("Failed to load model: {}", createInfo.filepath);
		DUBU_LOG_ERROR("Assimp error: {}", importer.GetErrorString());
		return;
	}

	{
		Texture  defaultTexture;
		uint32_t color = 0xffffffff;
		defaultTexture.LoadFromMemory(
		    {.device         = createInfo.device,
		     .physicalDevice = createInfo.physicalDevice,
		     .queueFamilies  = createInfo.queueFamilies,
		     .graphicsQueue  = createInfo.graphicsQueue},
		    {1, 1},
		    &color,
		    4);
		mTextures["default"] = std::move(defaultTexture);
	}

	mMaterials.resize(scene->mNumMaterials);
	for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials;
	     ++materialIndex) {
		mMaterials[materialIndex].mDescriptorSet =
		    std::make_unique<dubu::gfx::DescriptorSet>(
		        dubu::gfx::DescriptorSet::CreateInfo{
		            .device             = createInfo.device,
		            .descriptorPool     = createInfo.descriptorPool,
		            .descriptorSetCount = 1,
		            .layouts            = {createInfo.descriptorSetLayout},
		        });

		aiMaterial* material = scene->mMaterials[materialIndex];

		std::vector<vk::DescriptorImageInfo> imageDescriptors = {
		    LoadTexture(createInfo,
		                scene,
		                material,
		                AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE),
		    LoadTexture(
		        createInfo,
		        scene,
		        material,
		        AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE),
		    LoadTexture(createInfo, scene, material, aiTextureType_NORMALS, 0),
		};

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets{};
		for (std::size_t i = 0; i < imageDescriptors.size(); ++i) {
			writeDescriptorSets.push_back(vk::WriteDescriptorSet{
			    .dstBinding      = static_cast<uint32_t>(i),
			    .descriptorCount = 1,
			    .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
			    .pImageInfo      = &imageDescriptors[i],
			});
		}

		mMaterials[materialIndex].mDescriptorSet->UpdateDescriptorSets(
		    0, writeDescriptorSets);
	}

	for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
		aiMesh* mesh = scene->mMeshes[i];

		Mesh::CreateInfo meshInfo{
		    .device         = createInfo.device,
		    .physicalDevice = createInfo.physicalDevice,
		    .queueFamilies  = createInfo.queueFamilies,
		    .graphicsQueue  = createInfo.graphicsQueue,
		    .materialIndex  = mesh->mMaterialIndex,
		};

		meshInfo.vertices.resize(mesh->mNumVertices);
		meshInfo.indices.resize(mesh->mNumFaces * 3);

		for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices;
		     ++vertexIndex) {
			meshInfo.vertices[vertexIndex] = Vertex{
			    .position = AiToGlm(mesh->mVertices[vertexIndex]),
			    .normal   = AiToGlm(mesh->mNormals[vertexIndex]),
			    .texCoord = AiToGlm(mesh->mTextureCoords[0][vertexIndex]),
			};
		}
		for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces;
		     ++faceIndex) {
			meshInfo.indices[faceIndex * 3 + 0] =
			    mesh->mFaces[faceIndex].mIndices[0];
			meshInfo.indices[faceIndex * 3 + 1] =
			    mesh->mFaces[faceIndex].mIndices[1];
			meshInfo.indices[faceIndex * 3 + 2] =
			    mesh->mFaces[faceIndex].mIndices[2];
		}

		mMeshes.push_back(Mesh(meshInfo));
	}
}

void Model::RecordCommands(
    vk::PipelineLayout                      pipelineLayout,
    std::vector<dubu::gfx::DrawingCommand>& drawingCommands) {
	for (auto& mesh : mMeshes) {
		drawingCommands.push_back(
		    dubu::gfx::DrawingCommands::BindDescriptorSets{
		        .bindPoint      = vk::PipelineBindPoint::eGraphics,
		        .pipelineLayout = pipelineLayout,
		        .firstSet       = 1,
		        .descriptorSets = {mMaterials[mesh.GetMaterialIndex()]
		                               .mDescriptorSet->GetDescriptorSet()},
		    });
		drawingCommands.push_back(dubu::gfx::DrawingCommands::BindVertexBuffers{
		    .buffers = {mesh.GetVertexBuffer()},
		    .offsets = {0},
		});
		drawingCommands.push_back(dubu::gfx::DrawingCommands::BindIndexBuffer{
		    .buffer    = mesh.GetIndexBuffer(),
		    .offset    = 0,
		    .indexType = vk::IndexType::eUint32,
		});
		drawingCommands.push_back(dubu::gfx::DrawingCommands::DrawIndexed{
		    .indexCount    = mesh.GetIndexCount(),
		    .instanceCount = 1,
		});
	}
}

vk::DescriptorImageInfo Model::LoadTexture(const CreateInfo& createInfo,
                                           const aiScene*    scene,
                                           aiMaterial*       material,
                                           aiTextureType     textureType,
                                           unsigned int      index) {
	aiString texturePath;
	if (material->GetTexture(textureType, index, &texturePath) ==
	    aiReturn_SUCCESS) {
		auto it = mTextures.find(texturePath.C_Str());
		if (it == mTextures.end()) {
			auto textureData = scene->GetEmbeddedTexture(texturePath.C_Str());

			Texture newTexture;
			if (textureData->mHeight == 0) {
				newTexture.LoadFromCompressedMemory(
				    {
				        .device         = createInfo.device,
				        .physicalDevice = createInfo.physicalDevice,
				        .queueFamilies  = createInfo.queueFamilies,
				        .graphicsQueue  = createInfo.graphicsQueue,
				    },
				    textureData->pcData,
				    static_cast<std::size_t>(textureData->mWidth));
			} else {
				newTexture.LoadFromMemory(
				    {.device         = createInfo.device,
				     .physicalDevice = createInfo.physicalDevice,
				     .queueFamilies  = createInfo.queueFamilies,
				     .graphicsQueue  = createInfo.graphicsQueue},
				    {textureData->mWidth, textureData->mHeight},
				    textureData->pcData,
				    static_cast<std::size_t>(textureData->mWidth *
				                             textureData->mHeight * 4));
			}

			it = mTextures.emplace(texturePath.C_Str(), std::move(newTexture))
			         .first;
		}

		return it->second.GetImageInfo();
	} else {
		return mTextures["default"].GetImageInfo();
	}
}
