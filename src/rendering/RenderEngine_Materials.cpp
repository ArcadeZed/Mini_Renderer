// This file will be included at the end of RenderEngine.cpp
// Contains multi-material texture loading functions

// ============================================================================
// MULTI-MATERIAL TEXTURE LOADING
// ============================================================================

void RenderEngine::loadMaterialTextures() {
    const auto& materials = scene.objects[0].mesh.getMaterials();

    if (materials.empty()) {
        std::cerr << "No materials found in mesh!" << std::endl;
        return;
    }

    materialResources.clear();
    materialResources.reserve(materials.size());

    std::cout << "Loading " << materials.size() << " material textures..." << std::endl;

    for (size_t i = 0; i < materials.size(); i++) {
        const Material& mat = materials[i];
        MaterialResources matRes;

        std::cout << "  Material [" << i << "]: " << mat.name;

        // Check if material has a texture
        if (mat.diffuseTexturePath.empty()) {
            std::cout << " - No texture, using default" << std::endl;
            // Use default white texture (or fallback to legacy textureImage)
            matRes.textureImage = textureImage;  // Fallback
            matRes.textureImageView = textureImageView;
            matRes.textureImageMemory = VK_NULL_HANDLE;  // Don't own it
            matRes.mipLevels = mipLevels;
            materialResources.push_back(matRes);
            continue;
        }

        std::cout << " - Texture: " << mat.diffuseTexturePath << std::endl;

        // Load texture from file
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(mat.diffuseTexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            std::cerr << "    Failed to load texture! Using default." << std::endl;
            // Fallback to default texture
            matRes.textureImage = textureImage;
            matRes.textureImageView = textureImageView;
            matRes.textureImageMemory = VK_NULL_HANDLE;
            matRes.mipLevels = mipLevels;
            materialResources.push_back(matRes);
            continue;
        }

        VkDeviceSize imageSize = texWidth * texHeight * 4;
        std::cout << "    Loaded: " << texWidth << "x" << texHeight << " channels: " << texChannels << std::endl;

        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        resource.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(context.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(context.getDevice(), stagingBufferMemory);

        stbi_image_free(pixels);

        // Calculate mip levels
        matRes.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        // Create image
        resource.createImage(texWidth, texHeight, matRes.mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    matRes.textureImage, matRes.textureImageMemory);

        // Transition layout and copy
        resource.transitionImageLayout(matRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        resource.copyBufferToImage(stagingBuffer, matRes.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        // Generate mipmaps
        generateMipmaps(matRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, matRes.mipLevels);

        // Cleanup staging buffer
        vkDestroyBuffer(context.getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(context.getDevice(), stagingBufferMemory, nullptr);

        // Create image view
        matRes.textureImageView = resource.createImageView(matRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB, matRes.mipLevels);

        std::cout << "    Created VkImage + VkImageView with " << matRes.mipLevels << " mip levels" << std::endl;

        materialResources.push_back(matRes);
    }

    std::cout << "All material textures loaded successfully!" << std::endl;
}

void RenderEngine::createMaterialDescriptorSets() {
    if (materialResources.empty()) {
        std::cerr << "No material resources loaded!" << std::endl;
        return;
    }

    std::cout << "Creating " << materialResources.size() << " descriptor sets (one per material)..." << std::endl;

    // Allocate descriptor sets (one per material)
    std::vector<VkDescriptorSetLayout> layouts(materialResources.size(), descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorManager->getPool();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(materialResources.size());
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> tempSets(materialResources.size());
    if (vkAllocateDescriptorSets(context.getDevice(), &allocInfo, tempSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate material descriptor sets!");
    }

    // Update each material's descriptor set
    for (size_t i = 0; i < materialResources.size(); i++) {
        materialResources[i].descriptorSet = tempSets[i];

        // Binding 0: UBO (same for all materials - use frame 0's UBO)
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[0];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        // Binding 1: Texture sampler
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = materialResources[i].textureImageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = materialResources[i].descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = materialResources[i].descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(context.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        std::cout << "  Descriptor set [" << i << "] created for material" << std::endl;
    }

    std::cout << "All material descriptor sets created!" << std::endl;
}

void RenderEngine::cleanupMaterialResources() {
    for (auto& matRes : materialResources) {
        // Only cleanup resources we own (not fallback references)
        if (matRes.textureImageMemory != VK_NULL_HANDLE) {
            vkDestroyImageView(context.getDevice(), matRes.textureImageView, nullptr);
            vkDestroyImage(context.getDevice(), matRes.textureImage, nullptr);
            vkFreeMemory(context.getDevice(), matRes.textureImageMemory, nullptr);
        }
    }
    materialResources.clear();
}
