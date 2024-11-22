#include "common.h"
#include "ResourceManager.h"
#include "RayTracingPipeline.h"
#include "RayTracer.h"

void RayTracingPipeline::createRayTracerDescriptorSetLayout() {
	//Creating the layout
	if (refRayTracer->mainLogicalDevice == nullptr) {
		throw std::runtime_error("Main Logical Device is expired / null!\n");
	}
	if (refRayTracer->mainPhysicalDevice == nullptr) {
		throw std::runtime_error("Main Physical Device is expired / null!\n");
	}
	if (refRayTracer->mainSwapChainFormat == nullptr) {
		throw std::runtime_error("Main SwapChain Format is expired / null!\n");
	}
	VkDevice logicalDevice = *refRayTracer->mainLogicalDevice;
	VkPhysicalDevice physicalDevice = *refRayTracer->mainPhysicalDevice;
	VkFormat swapChainFormat = *refRayTracer->mainSwapChainFormat;
	//topLevelAccelerationStructure binding
	VkDescriptorSetLayoutBinding accStructureBinding;
	accStructureBinding.binding = 0;
	accStructureBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	accStructureBinding.descriptorCount = 1;
	accStructureBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accStructureBinding.pImmutableSamplers = nullptr;
	//Image binding
	VkDescriptorSetLayoutBinding imageBinding;
	imageBinding.binding = 1;
	imageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	imageBinding.descriptorCount = 1;
	imageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imageBinding.pImmutableSamplers = nullptr;
	//Layout to be used by model infomation
	VkDescriptorSetLayoutBinding vertexLayoutBinding{};
	vertexLayoutBinding.binding = 2;
	vertexLayoutBinding.descriptorCount = 1;
	vertexLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertexLayoutBinding.pImmutableSamplers = nullptr;
	vertexLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	VkDescriptorSetLayoutBinding indexLayoutBinding{};
	indexLayoutBinding.binding = 3;
	indexLayoutBinding.descriptorCount = 1;
	indexLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indexLayoutBinding.pImmutableSamplers = nullptr;
	indexLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	//Add Material bindings
	VkDescriptorSetLayoutBinding materialLayoutBinding{};
	materialLayoutBinding.binding = 4;
	materialLayoutBinding.descriptorCount = 1;
	materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialLayoutBinding.pImmutableSamplers = nullptr;
	materialLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	VkDescriptorSetLayoutBinding materialIndexLayoutBinding{};
	materialIndexLayoutBinding.binding = 5;
	materialIndexLayoutBinding.descriptorCount = 1;
	materialIndexLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialIndexLayoutBinding.pImmutableSamplers = nullptr;
	materialIndexLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	//Combining all the layouts
	std::array<VkDescriptorSetLayoutBinding, 6> bindings = { accStructureBinding, imageBinding,
	vertexLayoutBinding, indexLayoutBinding,materialLayoutBinding,materialIndexLayoutBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	layoutInfo.pNext = NULL;
	layoutInfo.flags = 0;
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data(); //Array of bindings
	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}
void RayTracingPipeline::createRayTracerDescriptorPool() {
	if (refRayTracer->mainLogicalDevice == nullptr) {
		throw std::runtime_error("Main Logical Device is expired / null!\n");
	}
	VkDevice logicalDevice = *refRayTracer->mainLogicalDevice;
	int descCount = MAX_FRAMES_IN_FLIGHT * ResourceManager::manager->maxModelCount;
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(descCount);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(descCount);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(descCount);
	if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}
//Create Descriptor Sets for all possible models -- See if we can reduce this!
void RayTracingPipeline::createRayTracerDescriptorSets() {
	//Allocate data for the descriptor sets
	//Creating the layout
	if (refRayTracer->mainLogicalDevice == nullptr) {
		throw std::runtime_error("Main Logical Device is expired / null!\n");
	}
	VkDevice logicalDevice = *refRayTracer->mainLogicalDevice;
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT * ResourceManager::manager->maxModelCount,
		descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT*ResourceManager::manager->maxModelCount);
	allocInfo.pSetLayouts = layouts.data();
	descriptorSets.resize(allocInfo.descriptorSetCount);
	if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}
}
//Call this to update the descriptor sets based on how many models currently exist
void RayTracingPipeline::updateRayTracerDescriptorSets(uint32_t frameIndex) {
	if (refRayTracer->mainLogicalDevice == nullptr) {
		throw std::runtime_error("Main Logical Device is expired / null!\n");
	}
	VkDevice logicalDevice = *refRayTracer->mainLogicalDevice;
	uint32_t rayDescCount = ResourceManager::manager->modelList.size();

	//Configure the sets and pass them to sets
	for (size_t i = 0; i < rayDescCount; i+= 2) {
		int descIndex = i + frameIndex;
		Model *m = &ResourceManager::manager->modelList[descIndex];
		Mesh* refMesh = &ResourceManager::manager->meshList[m->referenceMeshIndex];
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageLayout = {};
		imageInfo.imageView = refRayTracer->rayTracerImageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSetAccelerationStructureKHR writeStuct;
		writeStuct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		writeStuct.pNext = NULL;
		writeStuct.accelerationStructureCount = 1;
		writeStuct.pAccelerationStructures = refRayTracer->getTopLevelAccelerationStructure(); //Have a getter for top level

		VkDescriptorBufferInfo vertexInfo;
		vertexInfo.buffer = refMesh->vertexBuffer;
		vertexInfo.offset = 0;
		vertexInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo indexInfo;
		indexInfo.buffer = refMesh->indexBuffer;
		indexInfo.offset = 0;
		indexInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo materialInfo;
		materialInfo.buffer = refMesh->materialBuffer;
		materialInfo.offset = 0;
		materialInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo materialIndexInfo;
		materialIndexInfo.buffer = refMesh->materialIndexBuffer;
		materialIndexInfo.offset = 0;
		materialIndexInfo.range = VK_WHOLE_SIZE;
		//Assigning Descriptor infomation to bindings in layout
		std::array<VkWriteDescriptorSet, 6> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[descIndex];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pNext = &writeStuct;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[descIndex];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = descriptorSets[descIndex];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].pBufferInfo = &vertexInfo;

		descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[3].dstSet = descriptorSets[descIndex];
		descriptorWrites[3].dstBinding = 3;
		descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].dstArrayElement = 0;
		descriptorWrites[3].pBufferInfo = &indexInfo;

		descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[4].dstSet = descriptorSets[descIndex];
		descriptorWrites[4].dstBinding = 4;
		descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[4].descriptorCount = 1;
		descriptorWrites[4].dstArrayElement = 0;
		descriptorWrites[4].pBufferInfo = &materialInfo;

		descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[5].dstSet = descriptorSets[descIndex];
		descriptorWrites[5].dstBinding = 5;
		descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[5].descriptorCount = 1;
		descriptorWrites[5].dstArrayElement = 0;
		descriptorWrites[5].pBufferInfo = &materialIndexInfo;

		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data()
			, 0, nullptr);
	}
}

void RayTracingPipeline::createRayTracingPipeline() {
	if (refRayTracer->mainLogicalDevice == nullptr) {
		throw std::runtime_error("Main Logical Device is expired / null!\n");
	}
	if (refRayTracer->mainPhysicalDevice == nullptr) {
		throw std::runtime_error("Main Physical Device is expired / null!\n");
	}
	if (refRayTracer->mainDescSetLayout == nullptr) {
		throw std::runtime_error("Main Desc. Set Layout Device is expired / null!\n");
	}
	VkDevice logicalDevice = *refRayTracer->mainLogicalDevice;
	VkPhysicalDevice physicalDevice = *refRayTracer->mainPhysicalDevice;
	VkDescriptorSetLayout descSetLayout = *refRayTracer->mainDescSetLayout;
	VkRayTracingPipelineCreateInfoKHR rayTracerPipeline;
	enum StagesIndies {
		eRaygen,
		eMiss,
		eClosestHit,
		eShaderGroupCount
	};
	//We control the order of execution and dataflow since the order isnt linear
	std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
	//RGen
	VkPipelineShaderStageCreateInfo stage;
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.pName = "main"; //Entry point for our pipeline
	stage.module = createShaderModule(readFile("Shaders/rgen.spv"));
	stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stage.pNext = NULL;
	stage.flags = 0;
	stage.pSpecializationInfo = NULL;
	stages[eRaygen] = stage;
	//RMiss
	stage.module = createShaderModule(readFile("Shaders/rmiss.spv"));
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eMiss] = stage;
	//R closest hit
	stage.module = createShaderModule(readFile("Shaders/rchit.spv"));
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[eClosestHit] = stage;
	//Create Shader Groups - Shader instances that will be called every frame!
	VkRayTracingShaderGroupCreateInfoKHR group;
	group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	group.pNext = NULL;
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;
	group.pShaderGroupCaptureReplayHandle = VK_NULL_HANDLE;
	//Raygen
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eRaygen;
	raytracingShaderGroups.push_back(group);
	//Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss;
	raytracingShaderGroups.push_back(group);
	//Closest Hit
	//Triangle hit includes any, close, and intersection shaders
	//We only have closest hit and it 
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	raytracingShaderGroups.push_back(group);
	//Create Shader Stages for ray tracing
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	//Push Constants are used to send infomation to all pipeline stages
	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(RayTracer::PushConstantRay);
	pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
		| VK_SHADER_STAGE_MISS_BIT_KHR;
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	std::vector<VkDescriptorSetLayout> rtDescSetLayout = { descriptorSetLayout,descSetLayout };
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayout.size());
	pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayout.data();
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.pNext = NULL;
	//Finish the pipeline layout
	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to make the rt pipeline layout!");
	}
	rayTracerPipeline.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracerPipeline.pNext = NULL;
	rayTracerPipeline.flags = 0;
	rayTracerPipeline.stageCount = stages.size();
	rayTracerPipeline.pStages = stages.data();
	rayTracerPipeline.groupCount = raytracingShaderGroups.size();
	rayTracerPipeline.pGroups = raytracingShaderGroups.data();
	rayTracerPipeline.layout = pipelineLayout;
	rayTracerPipeline.maxPipelineRayRecursionDepth = refRayTracer->rayTracingProperties.maxRayRecursionDepth;
	rayTracerPipeline.basePipelineHandle = VK_NULL_HANDLE;
	rayTracerPipeline.basePipelineIndex = 0;
	rayTracerPipeline.pDynamicState = NULL;
	rayTracerPipeline.pLibraryInfo = NULL;
	rayTracerPipeline.pLibraryInterface = NULL;

	if (refRayTracer->pvkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracerPipeline, NULL, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to make the rt pipeline!");
	}
	//Get rid of the modules since we don't need them now
	for (auto& s : stages) {
		vkDestroyShaderModule(logicalDevice, s.module, nullptr);
	}
}

void RayTracingPipeline::createShaderBindingTable() {
	// So I can't query the GPU since I dont have the device feature for it. Will use physical device queryig instead
	//Maps which shaders we should call for different entrypoints
	//Setting up buffer offsets to store the shader handles in
	//32bit for RG, 16 for miss,padd out another 16, and finally 16 for hit
	if (refRayTracer->mainLogicalDevice == nullptr) {
		throw std::runtime_error("Main Logical Device is expired / null!\n");
	}
	if (refRayTracer->mainPhysicalDevice == nullptr) {
		throw std::runtime_error("Main Physical Device is expired / null!\n");
	}
	VkDevice logicalDevice = *refRayTracer->mainLogicalDevice;
	VkPhysicalDevice physicalDevice = *refRayTracer->mainPhysicalDevice;
	const uint32_t missCount = 1;
	const uint32_t hitCount = 1;
	const auto handleCount = 1 + missCount + hitCount;
	//Can't guarintee that alignment matches group size or handle so we should round up to nearest bit
	const uint32_t handleSize = refRayTracer->rayTracingProperties.shaderGroupHandleSize;
	const uint32_t handleAlign = refRayTracer->rayTracingProperties.shaderGroupHandleAlignment;
	const uint32_t baseAlign = refRayTracer->rayTracingProperties.shaderGroupBaseAlignment;

	//Aligns handles using handle and base alignments, offset by the size
	//See this page for formula explaination: https://en.wikipedia.org/wiki/Data_structure_alignment
	const uint32_t handleSizeAligned = (handleSize + (handleAlign - 1)) & ~(handleAlign - 1);
	rayGenerationRegion.stride = (handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
	rayGenerationRegion.size = rayGenerationRegion.stride;
	rayMissRegion.stride = handleSizeAligned;
	rayMissRegion.size = (missCount * handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
	rayHitRegion.stride = handleSizeAligned;
	rayHitRegion.size = (hitCount * handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
	//Vector to store the handles to each shader
	const uint32_t dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	const auto result = refRayTracer->pvkGetRayTracingShaderGroupHandlesKHR(logicalDevice, pipeline, 0, handleCount, dataSize, handles.data());
	//Im breaking my consistentcy rules because this way is so much better and I want to demonstrate the better way to do this valdiatioN!
	assert(result == VK_SUCCESS);

	//Allocate buffer for SBT
	uint32_t queueFamilyIndex = refRayTracer->findSimultaniousGraphicsAndPresentIndex(physicalDevice);
	VkDeviceSize shaderBindingTableSize = rayGenerationRegion.size + rayMissRegion.size + rayHitRegion.size;
	//create and bind buffer for SBT
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = shaderBindingTableSize; //size of our buffer
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR; //what kind of buffer is this?
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.queueFamilyIndexCount = 1;
	bufferInfo.pQueueFamilyIndices = &queueFamilyIndex;
	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &shaderBindingTableBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create Shader Binding Table buffer!");
	}
#ifndef NDEBUG
	ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(shaderBindingTableBuffer)
		, "Shader Binding Table Buffer");
#endif
	//Query the memory requirements to make sure we have enough space to allocate for the vertex buffer
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, shaderBindingTableBuffer, &memoryRequirements);
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
	//Check to see what memory our graphics card has for the buffer
	uint32_t shaderBindingTableMemoryTypeIndex =
		refRayTracer->findBufferMemoryTypeIndex(logicalDevice, physicalDevice, shaderBindingTableBuffer
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	//Allocate memory for the buffer
	const VkMemoryAllocateFlagsInfo flags = refRayTracer->getDefaultAllocationFlags();
	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &flags;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = shaderBindingTableMemoryTypeIndex;
	shaderBindingTableDeviceMemory = VK_NULL_HANDLE;
	if (vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, &shaderBindingTableDeviceMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate vertex buffer memory!");
	}
	//Bind the allocated memory to the sbt buffer
	if (vkBindBufferMemory(logicalDevice, shaderBindingTableBuffer, shaderBindingTableDeviceMemory, 0)) {
		throw std::runtime_error("Failed to allocate memory to sbt buffer");
	}
#ifndef NDEBUG
	ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
		, reinterpret_cast<uint64_t>(shaderBindingTableDeviceMemory)
		, "Shader Binding Table Memory");
#endif

	//Storing device addresses for the shader groups
	VkBufferDeviceAddressInfo info;
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	info.pNext = nullptr;
	info.buffer = shaderBindingTableBuffer;
	VkDeviceAddress shaderBindingTableAddress = refRayTracer->pvkGetBufferDeviceAddressKHR(logicalDevice, &info);
	rayGenerationRegion.deviceAddress = shaderBindingTableAddress;
	rayMissRegion.deviceAddress = shaderBindingTableAddress + rayGenerationRegion.size;
	rayHitRegion.deviceAddress = shaderBindingTableAddress + rayGenerationRegion.size + rayMissRegion.size;

	auto getHandle = [&](int i) { return handles.data() + (uint32_t)i * handleSize; };
	//Map the SBT buffer and write in the handles
	void* hostShaderBindingTableMemoryBuffer;
	if (vkMapMemory(logicalDevice, shaderBindingTableDeviceMemory, 0, shaderBindingTableSize, 0, &hostShaderBindingTableMemoryBuffer)
		!= VK_SUCCESS) {
		throw std::runtime_error("Failed to map memory sbt to buffer");
	}
	auto* pShaderBindingTableBuffer = reinterpret_cast<uint8_t*>(hostShaderBindingTableMemoryBuffer);
	uint8_t* pData = nullptr;
	uint32_t handleIdx = 0;
	//Copy data over for Raygen
	pData = pShaderBindingTableBuffer;
	memcpy(pData, getHandle(handleIdx++), handleSize);
	//Point base of pointer to miss shader(s)
	pData = pShaderBindingTableBuffer + rayGenerationRegion.size;
	//for loop to copy multiple miss shaders in case we add more
	for (uint32_t c = 0; c < missCount; c++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += rayMissRegion.stride;
	}
	//Point base of pointer to miss shader(s)
	pData = pShaderBindingTableBuffer + rayGenerationRegion.size + rayMissRegion.size;
	//for loop to copy multiple miss shaders in case we add more
	for (uint32_t c = 0; c < hitCount; c++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += rayHitRegion.stride;
	}
	//CLeanup resources
	vkUnmapMemory(logicalDevice, shaderBindingTableDeviceMemory);
}

VkStridedDeviceAddressRegionKHR* RayTracingPipeline::getShaderRegionAddress(int regionNumber) {
	switch (regionNumber)
	{
	case 0:
		return &rayGenerationRegion;
		break;
	case 1:
		return &rayMissRegion;
		break;
	case 2:
		return &rayHitRegion;
		break;
	case 3:
		return &rayCallRegion;
		break;
	default:
		return &rayGenerationRegion;
		break;
	}
}

void RayTracingPipeline::cleanup() {
	//Ray Tracing Pipeline
	vkDestroyDescriptorSetLayout(ResourceManager::manager->device, descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(ResourceManager::manager->device, descriptorPool, nullptr);
	vkDestroyPipeline(ResourceManager::manager->device, pipeline, nullptr);
	vkDestroyPipelineLayout(ResourceManager::manager->device, pipelineLayout, nullptr);
	//Shader Binding Table
	vkDestroyBuffer(ResourceManager::manager->device, shaderBindingTableBuffer, nullptr);
	vkFreeMemory(ResourceManager::manager->device, shaderBindingTableDeviceMemory, nullptr);
}