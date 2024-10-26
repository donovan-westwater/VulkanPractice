#include "common.h"
#include "ResourceManager.h"
#include "RayTracer.h"

void ResourceManager::initVulkan() {
    //Graphics Initalization Section
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createMainRenderPass();
    Pipeline defaultPipeline;
    pipelineList.push_back(defaultPipeline);
    pipelineList[0].createMainDescriptorSetLayout();
    //This is where you would start to make pipelines for different shaders 
    pipelineList[0].createDefaultGraphicsPipeline();
    //create command pool for command buffers
    createCommandPool();
    //Create color buffer
    pipelineList[0].createColorResources();
    //Create depth buffer
    pipelineList[0].createDepthResources();
    //Create framebuffers to draw the actual images with
    pipelineList[0].createFramebuffers();
    //TEXTURE LOADING WAS HERE
    //Load in the model -- TODO: add function to add a blank model to vector and then load it
    Model test1;
    modelList.push_back(test1);
    modelList[0].loadModel("Models/dino.obj",MATERIALS_PATH,TEXTURES_PATH+"dino.png");
    modelList[0].setScale(glm::vec3(.25f));
    modelList[0].setPosition(glm::vec3(0, 0, 0));
    //modelList[0].setRotation(glm::vec3(0, glm::radians(90.0f), 0));
    Model test2;
    modelList.push_back(test2);
    modelList[1].loadModel("Models/Guilmon.obj", MATERIALS_PATH, TEXTURES_PATH+"guilmon.png");
    modelList[1].setScale(glm::vec3(.25f));
    modelList[1].setPosition(glm::vec3(1, 0, 0));
    modelList[1].setRotation(glm::vec3(glm::radians(90.0f),0, 0));
    //create ubo buffer
    //pipelineList[0].createMainUniformBuffers();
    pipelineList[0].createMainDescriptorPool();
    pipelineList[0].createMainDescriptorSets();
    createCommandBuffers();
    //Raytracing pipeline section

    createSyncObjects(); //create objects for syncing cpu with gpu

    //Player Logic Initalization
    mainCamera.cameraInit();
}
//Creates a render pass object which tells Vulkan about framebuffer attachemnts, color and depth buffers
    //How many samples to use for each of the them, and how their contents should be handled throughout rendering process
void ResourceManager::createMainRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = ResourceManager::manager->swapChainImageFormat; //should match format of swapchain
    colorAttachment.samples = msaaSamples; //multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //Should clear before renderig
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; //should store after rendering
    //Stenicl ops: We dont care about stencils right now since we aren't using it
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //This bit is important for texturing
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //dont care about inital layout
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //Images to be presented in the swap chain
    //Create a second description for color becuase we need to go over the image again to resolve the multiple samples
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = ResourceManager::manager->swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    //Subpasses and attachment references
    //Useful to have passes that sequential stack on eachother for post processing
    //sticking to a single pass
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; //which attachment to reference by index
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //species which layout we would like to have during a subpass that uses this reference
    //Create second attachment for the resolution description
    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    //Depth Testing
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = ResourceManager::manager->findDepthFormat();
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    //Finally the render pass itself
    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment,colorAttachmentResolve };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    //Create a subpass dependency to prevent a transation from occuring before the image is qcquired at the start
    //Specify indices of hte dependency and the dependent subpass
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    //Specify which operations to wait on
    //Prevents transition from happening unitl its necessary and allowed
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(ResourceManager::manager->device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

}
void ResourceManager::beginMainRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional Controls how command buffer will be used
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    //start the render pass which is the start of the drawing process
    //First binds attachments to render pass
    //Then defines render area size
    //Then defines the clear values
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = ResourceManager::manager->renderPass;
    renderPassInfo.framebuffer = ResourceManager::manager->swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = ResourceManager::manager->swapChainExtent;
    //Clear the depth buffer
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}
void ResourceManager::endMainRenderPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}
//All of the common vulkan functions should go here
//Transition layer, create buffer, create image etc.
//Any function left over in main should go here as a static function
//Dont make them inline! Debug is fine since it is small

//Create, allocate, and bind the image to memory
void ResourceManager::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage
    , VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    //Settings for the image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    //We need same format for the texels as the pixels in the buffer
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //discard textuals first transition
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = numSamples; //Used for multisampling
    imageInfo.flags = 0; // Optional

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }
    //allocating memory to the image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }
    //Bind the image to the allocated memory
    vkBindImageMemory(device, image, imageMemory, 0);
}
//begins a command buffer to be used for a single use
VkCommandBuffer ResourceManager::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    //Memory transfer is executed using command buffers
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    //Start recording command buffer to transfer memory with
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

//ends the recording of a single frame
void ResourceManager::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    //Submit the command buffer to the queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue); //Wait for the transfer queue to become idle
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer); //Cleanup once we are done with the buffer
}

//Checking the physical memory of our GPU to see if we have room
uint32_t ResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    //Check the physical memory of our device
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    //Find a memory type that will work for our buffer
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) { //Use a bitmask to specify the bit field of suitible memory types
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void ResourceManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory
    , bool rayTracingMemAlloc) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size; //size of our buffer
    bufferInfo.usage = usage; //what kind of buffer is this?
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }
    //Query the memory requirements to make sure we have enough space to allocate for the vertex buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    //Allocate memory for the buffer
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    VkMemoryAllocateFlagsInfo rayTraceMemInfo;
    if (rayTracingMemAlloc) {
        rayTraceMemInfo = RayTracer::getDefaultAllocationFlags();
        allocInfo.pNext = &rayTraceMemInfo;
    }
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
    //Bind the allocated memory to the vertex buffer
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
#ifndef NDEBUG
    setDebugObjectName(device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer), "Temp Buffer");
    setDebugObjectName(device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<uint64_t>(bufferMemory), "Temp Buffer Memory");
#endif
}

//Copying vulkan buffer from src to dst 
void ResourceManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    //Declare a region of the memory to copy
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional Where are we copying from?
    copyRegion.dstOffset = 0; // Optional Where are we copying to?
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands(commandBuffer);

}

//Creates an image view
VkImageView ResourceManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlag, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    //subresource Range descibes the images's purpose and what part should be accesssed
    //THese imges are color targets without any mpimpaping levels or multiple layers
    viewInfo.subresourceRange.aspectMask = aspectFlag;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void ResourceManager::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    //pipeline barrir is used to synchonoize resources
    //Ensures buffer completes before reading from it
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image; //specify image effected by barrier
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0; // TODO Need to specify which operations need to do
    barrier.dstAccessMask = 0; // TODO
    VkPipelineStageFlags sourceStage; //Makes sure that we don't need to wait on anything
    VkPipelineStageFlags destinationStage; //Makes sure the shader waits on transfer writes
    //check for stenicl
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    //Undefined --> transfer destination
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } //Transfer dest --> Shader Reading
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }//Check for stenicl
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

//Copy buffer to image
void ResourceManager::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    //Selects a region to copy the image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };
    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    endSingleTimeCommands(commandBuffer);
}

//Find a format that works with depth testing
VkFormat ResourceManager::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat ResourceManager::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}
//Render loop only function - stay in main
void ResourceManager::createSyncObjects() {
    //make sure there are semaphores and fences for each concurrent frame
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //Prevents infinite blocking situation by signalling that the bit is used
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

}
//Universal function - Go in common
void ResourceManager::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT); //Resize to match the number of inflight frames
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; //Can be submitted to a queue for execution
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

//Universal - Move to common and adjust
//Create command pool for the command buffers
void ResourceManager::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); //Allows command buffers to be rerecorded indivisualy

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

//Stay in main?
void ResourceManager::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size()); //Should be same size as the amount of images
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}
//Stays in main
//Cleans up swapChain related resources
void ResourceManager::cleanupSwapChain() {
    //free color buffer
    vkDestroyImageView(device, pipelineList[0].colorImageView, nullptr);
    vkDestroyImage(device, pipelineList[0].colorImage, nullptr);
    vkFreeMemory(device, pipelineList[0].colorImageMemory, nullptr);
    //Free depth buffer
    vkDestroyImageView(device, pipelineList[0].depthImageView, nullptr);
    vkDestroyImage(device, pipelineList[0].depthImage, nullptr);
    vkFreeMemory(device, pipelineList[0].depthImageMemory, nullptr);
    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

//Should create a new swap chain when the window is updated
void ResourceManager::recreateSwapChain() {
    //We should pause the rendering when the window is minimized
    //Otherwise Vulkan will freak out because the frame buffer size will become zero!
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    //We shouldn't recreate anything until we know it isn't being used!
    vkDeviceWaitIdle(device);
    //Clean everything up before creating new ones!
    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    //Pipeline Specific --- Use Default
    pipelineList[0].createColorResources();
    pipelineList[0].createDepthResources();
    pipelineList[0].createFramebuffers();
}
//stay in main
void ResourceManager::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapChainSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; //Decide how many images we want from swapchain
    if (useRayTracing) imageCount = swapChainSupport.capabilities.minImageCount;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; //Always 1 unless making something in steroscopic 3D
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //Edit this to do things like post processing
    if (useRayTracing) createInfo.imageUsage |= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; //Transfer src and dst bits
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //Alpha bit set to ignore alpha bit
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; //we dont care about obscured pixels
    createInfo.oldSwapchain = VK_NULL_HANDLE; //Swapchain can become invalid or unoptimized while running. Chain needs to be recreated. This is for that

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swapchain");
    }
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    swapChainExtent = extent;
    swapChainImageFormat = surfaceFormat.format;
}
//stay in main
void ResourceManager::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}
//stay in main
void ResourceManager::createLogicalDevice() {
    VkPhysicalDeviceBufferDeviceAddressFeatures
        physicalDeviceBufferDeviceAddressFeatures;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        physicalDeviceAccelerationStructureFeatures;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR
        physicalDeviceRayTracingPipelineFeatures;
    if (useRayTracing) {
        physicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
        physicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay = VK_FALSE;
        physicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddressMultiDevice = VK_FALSE;
        physicalDeviceBufferDeviceAddressFeatures.pNext = NULL;
        physicalDeviceBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

        physicalDeviceAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            physicalDeviceAccelerationStructureFeatures.pNext = &physicalDeviceBufferDeviceAddressFeatures,
            physicalDeviceAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
        physicalDeviceAccelerationStructureFeatures.accelerationStructureCaptureReplay = VK_FALSE;
        physicalDeviceAccelerationStructureFeatures.accelerationStructureIndirectBuild = VK_FALSE;
        physicalDeviceAccelerationStructureFeatures.accelerationStructureHostCommands = VK_FALSE;
        physicalDeviceAccelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

        physicalDeviceRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        physicalDeviceRayTracingPipelineFeatures.pNext = &physicalDeviceAccelerationStructureFeatures;
        physicalDeviceRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        physicalDeviceRayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
        physicalDeviceRayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
        physicalDeviceRayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
        physicalDeviceRayTracingPipelineFeatures.rayTraversalPrimitiveCulling = VK_FALSE;
    }
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    //Create a set for all unique queues nesseary for our program
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(),indices.presentFamily.value() };

    //Controls the priority of queues for multithreading purposes
    //This is required even if there is only 1 queue
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        //Describes the number of queues we want for a single queue family
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    //Specify which features we are going to be using
    VkPhysicalDeviceFeatures deviceFeatures{};
    if (useRayTracing) deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    if (useRayTracing) createInfo.pNext = &physicalDeviceRayTracingPipelineFeatures;
    createInfo.pEnabledFeatures = &deviceFeatures;
    //Will look like the instance create info but it is device specific
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    //Validation layers will are ignored by up to date implemnations
    //Still good to keep assigned to keep them compatible
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
#ifndef NDEBUG
    pvkSetDebugUtilsObjectNameEXT =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
            device, "vkSetDebugUtilsObjectNameEXT");
    setDebugObjectName(device, VkObjectType::VK_OBJECT_TYPE_DEVICE, reinterpret_cast<uint64_t>(device)
        , "Main Logical Device");
#endif // !NDEBUG
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

//Figure out how many samples we can do with our device safely
//takes in both color buffer and depth buffers into account
VkSampleCountFlagBits ResourceManager::getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(ResourceManager::manager->physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}
//stay in main
void ResourceManager::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount < 1) {
        throw std::runtime_error("failed to find GPUs with Vulkan support");
    }
    //Check to see if there is a GPU for Vulkan to use
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            ResourceManager::manager->physicalDevice = device;
            ResourceManager::manager->msaaSamples = ResourceManager::manager->getMaxUsableSampleCount();
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}
//stay in main
bool ResourceManager::isDeviceSuitable(VkPhysicalDevice device) {
    //We can query some details about the device to determine suitablity
    QueueFamilyIndices indices = findQueueFamilies(device);
    //Right now we will select any GPU that has a swap chain
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        //Check if we have at least 1 presentMode and 1 image format
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    //check if we have antiscopy
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    //Later on we can add code to deterine which GPU is best for our purposes 
    //and could even rank them by some heuristic and select the best one!
    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}
//stay in main
bool ResourceManager::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    //Create a vector to store list of avalible extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    //Create a set of required extensions
    //Remove a name from the set of required names if the available extension is in the set
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    //If supported, all the names should be removed from the set
    return requiredExtensions.empty();
}
//stay in main
//Everything in  vulkan rquires commands to be submitted to a queue
//Different kinds of queues are come from different queue families
//We need to able to check which kinds of queue families are supported by the device
QueueFamilyIndices ResourceManager::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    //Retrive a list of queue familes
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties>queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    //QueuFamilyProperies contains details like what operations are acceptiable and the number of queues that cant be created 
    int i = 0;
    for (const auto& queueFamiliy : queueFamilies) {
        //Look for a queueFamily that supports Grpahics
        if (queueFamiliy.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        //Check to see if the queue family supports presenting window surfaces
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) {
            break;
        }
        i++;
    }
    return indices;

}
//stay in main
//Even if there is swapchain support, that doesn't mean it will for us.
//We need to figure out the details of the device's swapchain to see if there is a match
SwapChainSupportDetails ResourceManager::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}
//Chooses the correct surface format
VkSurfaceFormatKHR ResourceManager::chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    //Format specifies the color channels and types
    //color space dtermines if SRBG color space is supported for not
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    //If we can't find anything ideal just pick the first format we can find
    return availableFormats[0];
}
//stay in main
//Pick a presentation mode for the swap chain
VkPresentModeKHR ResourceManager::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    //If mailbox mode is supported just pick that, other pick the default mode
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}
//Pick a swap extent
VkExtent2D ResourceManager::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    //Pick a resolution that we would prefer
    //The range of possible resolutions it defined here
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void ResourceManager::resourceCleanUp() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        //Unmapping memory
        vkUnmapMemory(device, pipelineList[0].uniformBuffersMemory[i]);

    }
    for (int i = 0; i < meshList.size(); i++) {
        meshList[i].free();
    }
    for (int i = 0; i < textureList.size(); i++) {
        textureList[i].free();
    }
    //Pipeline cleanup
    for(int i = 0; i < pipelineList.size();i++){
        pipelineList[i].free();
    }
    //---Descriptor Set layout destruction here
    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }
    //Mesh Destruction Infomation
    for (int i = 0; i < meshList.size(); i++) {
        meshList[i].free();
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    //--- command pool destruction here
    //for (auto framebuffer : swapChainFramebuffers) {
    //     vkDestroyFramebuffer(device, framebuffer, nullptr);
    //}
    for (int i = 0; i < pipelineList.size(); i++) {
        vkDestroyPipeline(device, pipelineList[i].pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineList[i].pipelineLayout, nullptr);
    }
    vkDestroyRenderPass(device, renderPass, nullptr);
    //for (auto imageView : swapChainImageViews) {
    //    vkDestroyImageView(device, imageView, nullptr);
    //}
    //---swapchain destruction here
    //---Logical device destruction here;

    //---Surface Would be Destoried here
    //---Instance would be destroied here

    glfwDestroyWindow(window); //Free up memory used by window

    glfwTerminate(); //Clean up GLFW
}