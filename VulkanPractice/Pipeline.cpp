#include "common.h"
#include "ResourceManager.h"
#include "Pipeline.h"
#include "Texture.h"

//Create the descriptor sets
void Pipeline::createMainDescriptorSets() {
    //Allocate data for the descriptor sets
    //We want a descriptor set for every texture
    uint32_t descCount = MAX_FRAMES_IN_FLIGHT * ResourceManager::manager->textureList.size();
    std::vector<VkDescriptorSetLayout> layouts(descCount, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(descCount);
    allocInfo.pSetLayouts = layouts.data();
    descriptorSets.resize(descCount);
    if (vkAllocateDescriptorSets(ResourceManager::manager->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
    //Configure the sets and pass them to sets
    for (size_t i = 0; i < descCount; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i%2];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = ResourceManager::manager->textureList[i/2].textureImageView;
        imageInfo.sampler = ResourceManager::manager->textureList[i/2].textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(ResourceManager::manager->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data()
            , 0, nullptr);
    }

}
//Descriptor sets cant be created directly. They must be allocated like command buffers
void Pipeline::createMainDescriptorPool() {
    uint32_t descCount = MAX_FRAMES_IN_FLIGHT * ResourceManager::manager->textureList.size();
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(descCount);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(descCount);
    if (vkCreateDescriptorPool(ResourceManager::manager->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}
//Create the uniform object buffers
void Pipeline::createMainUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    //Create buffers for the frames that are being worked on in parallel
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ResourceManager::manager->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]
            , true);
#ifndef NDEBUG
        setDebugObjectName(ResourceManager::manager->device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(uniformBuffers[i]), "Uniform Buffer Object " + i);
#endif
        //We dont want to remap memory all the time since mem mapping is costly
        //Having the buffers mapped this way means we can update whenever we want!
        vkMapMemory(ResourceManager::manager->device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);

    }

}

void Pipeline::createMainDescriptorSetLayout() {
    //We need a descriptor set to help send the infomation over to the shader
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0; //Which index you want to bind to for the shader
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; //Kind of descriptor set
    uboLayoutBinding.descriptorCount = 1; //How many descriptor sets in the array
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; //which stage we are sending it to
    uboLayoutBinding.pImmutableSamplers = nullptr;
    //Create descriptor set for the image sampler
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
    //Placing bindings into the layout
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data(); //Array of bindings

    if (vkCreateDescriptorSetLayout(ResourceManager::manager->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

}

//Creates a render pass object which tells Vulkan about framebuffer attachemnts, color and depth buffers
    //How many samples to use for each of the them, and how their contents should be handled throughout rendering process
void Pipeline::createMainRenderPass() {
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
//Move to pipeline?
//Create the resources for the color buffer used for multisampling
void Pipeline::createColorResources() {
    VkFormat colorFormat = ResourceManager::manager->swapChainImageFormat;

    ResourceManager::manager->createImage(ResourceManager::manager->swapChainExtent.width, ResourceManager::manager->swapChainExtent.height, 1, msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
    colorImageView = ResourceManager::manager->createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}
//Move to pipeline?
//create the resources for depth testing
void Pipeline::createDepthResources() {
    VkFormat depthFormat = ResourceManager::manager->findDepthFormat();
    ResourceManager::manager->createImage(ResourceManager::manager->swapChainExtent.width, ResourceManager::manager->swapChainExtent.height, 1, msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = ResourceManager::manager->createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    //I want to keep the transition explicit even though it isn't nesseary
    ResourceManager::manager->transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED
        , VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

}
//Pipeline specific, move to pipeline
    //Create framebuffers for drawing images
void Pipeline::createFramebuffers() {
    ResourceManager::manager->swapChainFramebuffers.resize(ResourceManager::manager->swapChainImageViews.size());
    //Iterate through images view and frame frame buffers from them
    for (size_t i = 0; i < ResourceManager::manager->swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            colorImageView,
            depthImageView,
            ResourceManager::manager->swapChainImageViews[i]
        };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = ResourceManager::manager->swapChainExtent.width;
        framebufferInfo.height = ResourceManager::manager->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(ResourceManager::manager->device, &framebufferInfo, nullptr, &ResourceManager::manager->swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}
//Move to Pipeline, maybe even as a static function?
void Pipeline::createDefaultGraphicsPipeline() {
    isDefaultPipeline = true;
    if (ResourceManager::manager->pipelineList.size() < 1) {
        ResourceManager::manager->pipelineList.push_back(*this);
    }
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
    //Shader stage pipeline creation section (vert shader)
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; //Obligatory Type 
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; //Which pipeline stage will be used
    vertShaderModule = vertShaderModule;
    vertShaderStageInfo.module = vertShaderModule; //Module containing the code;
    vertShaderStageInfo.pName = "main"; //Entrypoint to invoke
    //can use pSpecializationInfo to add constants assoiated with the shader
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    //Vertex Input: Describes that spacing between data and wether data is per vertex or per instance
    //Also handles attribute descriptions: types of attibutes passed to the vertex shader, which binding
    //Leaving this blank for now since vertex info is currently hard coded
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; //Provide the binding description for vertices
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); //Provide the attribute descriptions for vertices
    //Input assembly stage: describes what kind of geometery will be drawn from the vertices and if primateitve restart is enabled
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; //What geometry will be drawn: We want triangles
    /*Going to be handled layer. This should be enable if these werent in the dynamic state
    //Viewports and scissors
    //Viewport: Region of the framebuffer the output will be rendered to
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExenet.width; //Want to width to match the extent of the images from swapchain
    viewport.height = (float)swapChainExenet.height; //same with height
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    //Scissors: Cut out sections of the image when rendering. Acts like a filter
    VkRect2D scissor{};
    scissor.offset = { 0,0 };
    scissor.extent = swapChainExenet;
    */
    //Without dynamic st
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    //This stage contains the few properites that can be changed without recreating the pipeline
    //Simplfies setup as having viewport and scissor state be dynamic is more flexible then having them baked in
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    //Rasterizer setup: converts the geomerty into fragments that can be colored
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE; //If true, the frags that are beyond the near/far plane are clamped rather than discared
    rasterizer.rasterizerDiscardEnable = VK_FALSE; //if true, discards the geometry. disables any output to framebuffer
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; //Determiens if polygons are filled, drawn as outline, or are just points
    rasterizer.lineWidth = 1.0f; //determines the line width
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; //enables back culling
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //determines how to find the front face
    rasterizer.depthBiasEnable = VK_FALSE; //Can create a bias to alter depth values based on frag slope
    rasterizer.depthBiasConstantFactor = 0.0f; //optional
    rasterizer.depthBiasClamp = 0.0; //optional
    rasterizer.depthBiasSlopeFactor = 0.0f; //optional
    //Multisampling setup: One of the ways to peform anit aliasing
    //combines fragment shader results of multiple polygons that rasterizze to the same pixel
    //Disabled for now, will come back to this later
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = msaaSamples;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional
    //Need to enable depth testing and stencil testing if using their respective buffers
    //Will be skipping both for ow
    //Color blending
    //This is the first struct needed for it that handles the config per attached frame buffer
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    //Second struct that handles global color blending settings
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional
    //Depth testing
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    //Pipeline layout creation: Handles uniform values and dynamic values pushed to shaders. Can be updated at draw time
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1; // Optional
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
    //pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    //pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(ResourceManager::manager->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2; //2 stages, one for vertex and one for frag
    pipelineInfo.pStages = shaderStages;
    //Attach the previous steps
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.subpass = 0; //index of the subpass
    //Can create a new graphics pipeline from an existing pipeline
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    //pipelineInfo.basePipelineIndex = -1; // Optional
    if (vkCreateGraphicsPipelines(ResourceManager::manager->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &ResourceManager::manager->pipelineList[0].pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    vkDestroyShaderModule(ResourceManager::manager->device, fragShaderModule, nullptr);
    vkDestroyShaderModule(ResourceManager::manager->device, vertShaderModule, nullptr);
}
//Move to Pipeline
//Module to handle shader programs compiled into the vulkan byte code
VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //The bytecode pointer is a uint32 and not a char, hence the cast
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(ResourceManager::manager->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule; //A thin wrapper around the byte code. Compliation + linking occurs at graphics pipeline time
}
//Only makes sense in main pipeline - Move this there
void Pipeline::recordDrawCallCommandBuffer(VkCommandBuffer commandBuffer,Mesh m, uint32_t imageIndex) {
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
    renderPassInfo.renderPass = renderPass;
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
    //Bind the commandBuffer to the pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ResourceManager::manager->pipelineList[0].pipeline);

    //Setup the viewport and scissors state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(ResourceManager::manager->swapChainExtent.width);
    viewport.height = static_cast<float>(ResourceManager::manager->swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = ResourceManager::manager->swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    //Assign the vertex buffer to the command buffer
    VkBuffer vertexBuffers[] = { m.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    //Update descriptor sets -- This hasnt been updated. It will not bind the correct desc. FIX LATER!
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[ResourceManager::manager->currentFrame], 0, nullptr);
    //The actual draw call!
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(m.indices.size()), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

//Move to pipleine and rename to updateMainUniformBuffers
void Pipeline::updateMainUniformBuffers(uint32_t currentFrame) {
    if (!isDefaultPipeline) return;
    //Using chrono to keep track of time independent of framerate
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    UniformBufferObject ubo{};
    //We create an indentity matrix and rotate based on the time
    ubo.model = glm::mat4(0.25f);
    ubo.model[3][3] = 1.0f;
    ubo.model[3][2] = -1.0f;
    ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.model = glm::rotate(ubo.model, time * glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    //Create a camera matrix at pos 2,2,2 look at 0 0 0, with up being Z
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    //Create a perspective based projection matrix for our camera
    ubo.proj = glm::perspective(glm::radians(45.0f), ResourceManager::manager->swapChainExtent.width / (float)ResourceManager::manager->swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1; //Y-coord for clip coords is inverted. This fixes that (GLM designed for openGL)
    //ubo.colorAdd = glm::vec4(abs(cos(time)), abs(sin(time)), abs(tan(time)), 1);
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void Pipeline::free() {
    //Former Swapchain function cleanup
//free color buffer
    vkDestroyImageView(ResourceManager::manager->device, colorImageView, nullptr);
    vkDestroyImage(ResourceManager::manager->device, colorImage, nullptr);
    vkFreeMemory(ResourceManager::manager->device, colorImageMemory, nullptr);
    //Free depth buffer
    vkDestroyImageView(ResourceManager::manager->device, depthImageView, nullptr);
    vkDestroyImage(ResourceManager::manager->device, depthImage, nullptr);
    vkFreeMemory(ResourceManager::manager->device, depthImageMemory, nullptr);

    //End of Swap Chain resource Cleanup
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(ResourceManager::manager->device, uniformBuffers[i], nullptr);
        vkFreeMemory(ResourceManager::manager->device, uniformBuffersMemory[i], nullptr);
    }
    if (!isDefaultPipeline) vkDestroyDescriptorSetLayout(ResourceManager::manager->device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(ResourceManager::manager->device, descriptorPool, nullptr);
}