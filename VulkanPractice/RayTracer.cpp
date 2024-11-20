#include "RayTracer.h"
#include "RayTracingPipeline.h"

//Using the following link as a referencce: https://github.com/WilliamLewww/vulkan_ray_tracing_minimal_abstraction/blob/master/ray_pipeline/src/main.cpp	
	//GO THROUGH EVERYTHING AND MAKE SURE SCRATCH BUFFERS ARE FREED!!!!
void RayTracer::CreateLightAndPassVarsToRayTracer() {
	LightSource light;
	light.dir = glm::normalize(glm::vec3(0, -1, 1));
	light.intensity = 1.0;
	light.pos = glm::vec3(0, 2, 2);
	light.type = 0;
	ResourceManager::manager->lightList.push_back(light);
	mainCommandPool = &ResourceManager::manager->commandPool;
	mainDescSetLayout = &ResourceManager::manager->pipelineList[0].descriptorSetLayout;
	mainDescSets = &ResourceManager::manager->pipelineList[0].descriptorSets;
	mainGraphicsQueue = &ResourceManager::manager->graphicsQueue;
	mainLogicalDevice = &ResourceManager::manager->device;
	mainPhysicalDevice = &ResourceManager::manager->physicalDevice;
	mainSurface = &ResourceManager::manager->surface;
	mainLightSource = &ResourceManager::manager->lightList[0];
	heightRef = ResourceManager::manager->height;
	widthRef = ResourceManager::manager->width;
	currentFrameRef = &ResourceManager::manager->currentFrame;
	mainSwapChainFormat = &ResourceManager::manager->swapChainImageFormat;
	rayTracerImageAvailableSemaphores = &ResourceManager::manager->imageAvailableSemaphores;
	rayTracerFinishedSemaphores = &ResourceManager::manager->renderFinishedSemaphores;
	rayTracerFences = &ResourceManager::manager->inFlightFences;
	rayTracerPresentQueue = &ResourceManager::manager->presentQueue;
	rayTracerSwapchain = &ResourceManager::manager->swapChain;
	rayTracerSwapchainImages = &ResourceManager::manager->swapChainImages;
}
	void RayTracer::setupRayTracer() {
		if (mainLogicalDevice == nullptr) { 
			throw std::runtime_error("main Logical Device is null or expired\n");
		}
#ifndef NDEBUG
		ResourceManager::manager->pvkSetDebugUtilsObjectNameEXT =
			(PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkSetDebugUtilsObjectNameEXT");
#endif
		initRayTracing();
		for (Mesh mesh : ResourceManager::manager->meshList) {
			modelToBottomLevelAccelerationStructure(mesh);
		}
		createTopLevelAccelerationStructure();
		createRayTracerImageAndImageView();
		//We might need to let the ray tracer handle the ray tracing pipelines instead of resource manager
		//For now we will just add them to the resource managaer since that seems fitting
		RayTracingPipeline pipeline;
		ResourceManager::manager->pipelineList.push_back(pipeline);
		int endIndex = ResourceManager::manager->pipelineList.size();
		refRayTracingPipeline = (RayTracingPipeline *) &ResourceManager::manager->pipelineList[endIndex];
		refRayTracingPipeline->createRayTracerDescriptorSetLayout();
		refRayTracingPipeline->createRayTracerDescriptorPool();
		refRayTracingPipeline->createRayTracerDescriptorSets();
		refRayTracingPipeline->createRayTracingPipeline();
		refRayTracingPipeline->createShaderBindingTable();
	}
	//Check to see if our GPU supports raytracing
	void RayTracer::initRayTracing()
	{
		if (mainLogicalDevice == nullptr) {
			throw std::runtime_error("main Logical Device is null or expired\n");
		}
		if (mainPhysicalDevice == nullptr) {
			throw std::runtime_error("main Physical Device is null or expired\n");
		}
		//Setup function pointers for ray trace functions
		VkDevice logicDevice = *mainLogicalDevice;
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice;
		pvkGetBufferDeviceAddressKHR =
			reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkGetBufferDeviceAddressKHR"));
		pvkCreateRayTracingPipelinesKHR =
			reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkCreateRayTracingPipelinesKHR"));
		pvkGetAccelerationStructureBuildSizesKHR =
			reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkGetAccelerationStructureBuildSizesKHR"));
		pvkCreateAccelerationStructureKHR =
			reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkCreateAccelerationStructureKHR"));
		pvkDestroyAccelerationStructureKHR =
			reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkDestroyAccelerationStructureKHR"));
		pvkGetAccelerationStructureDeviceAddressKHR =
			reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
		pvkCmdBuildAccelerationStructuresKHR =
			reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkCmdBuildAccelerationStructuresKHR"));
		pvkGetRayTracingShaderGroupHandlesKHR =
			reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(
				logicDevice, "vkGetRayTracingShaderGroupHandlesKHR"));
		pvkCmdTraceRaysKHR =
			reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(logicDevice,
				"vkCmdTraceRaysKHR"));

		// Requesting ray tracing properties
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		prop2.pNext = &rayTracingProperties;
		vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);
	}//BUG: I think the BLAS scratch buffers are setup wrong. there is a bottomLevelAccelerationStructureBuffer and buffer handle. Investigate
	void RayTracer::modelToBottomLevelAccelerationStructure(Mesh& mesh) {

		if (mainLogicalDevice == nullptr) {
			throw std::runtime_error("Main Logical Device is null or expired\n");
		}
		if (mainPhysicalDevice == nullptr) {
			throw std::runtime_error("Main Physical Device is null or expired\n");
		}
		RayTracerMeshInfo meshInfo;
		meshInfo.referenceMeshIndex = mesh.resourceListIndex;
		mesh.referenceRayTracerMeshInfoIndex = bottomLevelMeshInfoList.size() + 1;
		VkDevice logicalDevice = *mainLogicalDevice;
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice;
		VkBufferDeviceAddressInfo vInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		vInfo.pNext = NULL;
		vInfo.buffer = mesh.vertexBuffer;
		VkDeviceAddress vertexAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &vInfo);
		vInfo.buffer = mesh.indexBuffer;
		VkDeviceAddress indexAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &vInfo);
		uint32_t umaxPrimativeCount = static_cast<uint32_t>(mesh.primativeCount);

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		//Vertex buffer description
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexStride = sizeof(Vertex);
		//Index buffer description
		triangles.indexData.deviceAddress = indexAddress;
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.maxVertex = mesh.vertexCount;
		triangles.pNext = NULL;
		triangles.transformData.deviceAddress = 0;

		VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeom.geometry.triangles = triangles;
		asGeom.pNext = NULL;

		VkAccelerationStructureBuildGeometryInfoKHR bottomLevelAccelerationBuildGeometryInfoKHR{};
		bottomLevelAccelerationBuildGeometryInfoKHR.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		bottomLevelAccelerationBuildGeometryInfoKHR.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		bottomLevelAccelerationBuildGeometryInfoKHR.flags = 0;
		bottomLevelAccelerationBuildGeometryInfoKHR.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		bottomLevelAccelerationBuildGeometryInfoKHR.srcAccelerationStructure = VK_NULL_HANDLE;
		bottomLevelAccelerationBuildGeometryInfoKHR.dstAccelerationStructure = VK_NULL_HANDLE;
		bottomLevelAccelerationBuildGeometryInfoKHR.pGeometries = &asGeom;
		bottomLevelAccelerationBuildGeometryInfoKHR.geometryCount = 1;
		bottomLevelAccelerationBuildGeometryInfoKHR.ppGeometries = NULL;
		bottomLevelAccelerationBuildGeometryInfoKHR.scratchData.deviceAddress = 0;
		bottomLevelAccelerationBuildGeometryInfoKHR.pNext = NULL;

		VkAccelerationStructureBuildSizesInfoKHR bottomLevelAccelerationBuildSizesInfo{};
		bottomLevelAccelerationBuildSizesInfo.sType =
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		bottomLevelAccelerationBuildSizesInfo.pNext = NULL;
		bottomLevelAccelerationBuildSizesInfo.updateScratchSize = 0;
		bottomLevelAccelerationBuildSizesInfo.buildScratchSize = 0;
		bottomLevelAccelerationBuildSizesInfo.accelerationStructureSize = 0;
		std::vector<uint32_t> bottomLevelMaxPrimitiveCountList = { umaxPrimativeCount };
		pvkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&bottomLevelAccelerationBuildGeometryInfoKHR,
			bottomLevelMaxPrimitiveCountList.data(),
			&bottomLevelAccelerationBuildSizesInfo);
		//Create buffer to store acceleration structure
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };

		VkBufferCreateInfo bottomLevelAccelerationStructureBufferCreateInfo;
		bottomLevelAccelerationStructureBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bottomLevelAccelerationStructureBufferCreateInfo.pNext = NULL;
			bottomLevelAccelerationStructureBufferCreateInfo.flags = 0;
			bottomLevelAccelerationStructureBufferCreateInfo.size = bottomLevelAccelerationBuildSizesInfo.accelerationStructureSize;
			bottomLevelAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
			bottomLevelAccelerationStructureBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bottomLevelAccelerationStructureBufferCreateInfo.queueFamilyIndexCount = 1;
			bottomLevelAccelerationStructureBufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
			if (vkCreateBuffer(logicalDevice, &bottomLevelAccelerationStructureBufferCreateInfo, nullptr, &meshInfo.bottomLevelAccelerationStructureBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create buffer for bASS");
			}
#ifndef NDEBUG
			ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(meshInfo.bottomLevelAccelerationStructureBuffer)
				, "Bottom Level Accelertation Structure Buffer");
#endif
		//Look to see if our graphics card and our blAS has a local bit for our buffer
		uint32_t bottomLevelAccelerationStructureMemoryTypeIndex 
			= findBufferMemoryTypeIndex(logicalDevice,physicalDevice, meshInfo.bottomLevelAccelerationStructureBuffer
				,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkMemoryAllocateFlagsInfo defaultFlagsBLAS = getDefaultAllocationFlags();
		VkMemoryRequirements bottomLevelAccelerationStructureMemoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, 
			meshInfo.bottomLevelAccelerationStructureBuffer, &bottomLevelAccelerationStructureMemoryRequirements);
		//We are now allocating memory to the blAS
		VkMemoryAllocateInfo bottomLevelAccelerationStructureMemoryAllocateInfo;
		bottomLevelAccelerationStructureMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		bottomLevelAccelerationStructureMemoryAllocateInfo.pNext = NULL;//&defaultFlagsBLAS;
		bottomLevelAccelerationStructureMemoryAllocateInfo.allocationSize = bottomLevelAccelerationStructureMemoryRequirements.size;
		bottomLevelAccelerationStructureMemoryAllocateInfo.memoryTypeIndex = bottomLevelAccelerationStructureMemoryTypeIndex;
		
		if (vkAllocateMemory(logicalDevice, &bottomLevelAccelerationStructureMemoryAllocateInfo, nullptr, &meshInfo.bottomLevelAccelerationStructureDeviceMemory) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt allocate memory for buffer!");
		}
	
		//Bind the allocated memory
		if (vkBindBufferMemory(logicalDevice, meshInfo.bottomLevelAccelerationStructureBuffer, meshInfo.bottomLevelAccelerationStructureDeviceMemory,0) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt bind memory to buffer!");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(meshInfo.bottomLevelAccelerationStructureDeviceMemory)
			, "Bottom Level Acceleration Structure Device Memory");
#endif
		//Create acc structure
		VkAccelerationStructureCreateInfoKHR bottomLevelAccelerationStructureInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		bottomLevelAccelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		bottomLevelAccelerationStructureInfo.createFlags = 0;
		bottomLevelAccelerationStructureInfo.buffer = meshInfo.bottomLevelAccelerationStructureBuffer;
		bottomLevelAccelerationStructureInfo.offset = 0;
		bottomLevelAccelerationStructureInfo.size = bottomLevelAccelerationBuildSizesInfo.accelerationStructureSize;
		bottomLevelAccelerationStructureInfo.deviceAddress = 0;
		bottomLevelAccelerationStructureInfo.pNext = NULL;
	
		if (pvkCreateAccelerationStructureKHR(logicalDevice, &bottomLevelAccelerationStructureInfo, nullptr, &meshInfo.bottomLevelAccelerationStructure) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt create bottom acceleration structure");
		}

		//Building Bottom level acceleartion structure
		VkAccelerationStructureDeviceAddressInfoKHR bottomLevelAccelerationStructureDeviceAddressInfo;
		bottomLevelAccelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		bottomLevelAccelerationStructureDeviceAddressInfo.pNext = NULL;
		bottomLevelAccelerationStructureDeviceAddressInfo.accelerationStructure = meshInfo.bottomLevelAccelerationStructure;
		VkDeviceAddress bottomLevelAddress;
		bottomLevelAddress = pvkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &bottomLevelAccelerationStructureDeviceAddressInfo);
		VkBufferCreateInfo bottomLevelAccelerationStructureScratchBufferCreateInfo;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.flags = 0;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.size = bottomLevelAccelerationBuildSizesInfo.buildScratchSize;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.queueFamilyIndexCount = 1;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		bottomLevelAccelerationStructureScratchBufferCreateInfo.pNext = NULL;

		VkBuffer bottomLevelAccelerationStructureScratchBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(logicalDevice, &bottomLevelAccelerationStructureScratchBufferCreateInfo, NULL, &bottomLevelAccelerationStructureScratchBufferHandle) != VK_SUCCESS) {
			throw std::runtime_error("Buffer for building blAS cannot be made!");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bottomLevelAccelerationStructureScratchBufferHandle)
			, "Bottom Level Accelertation Structure Scratch Buffer");
#endif
		VkMemoryRequirements bottomLevelAccelerationStructureScratchMemoryReq;
		vkGetBufferMemoryRequirements(logicalDevice, bottomLevelAccelerationStructureScratchBufferHandle
			, &bottomLevelAccelerationStructureScratchMemoryReq);
		//Check to see what memory our graphics card has for the buffer
		uint32_t bottomLevelAccelerationStructureScratchMemoryTypeIndex = findBufferMemoryTypeIndex(logicalDevice, physicalDevice
			, bottomLevelAccelerationStructureScratchBufferHandle,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		VkMemoryAllocateInfo blASScratchMemoryAllocateInfo;
		blASScratchMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		blASScratchMemoryAllocateInfo.pNext = &defaultFlagsBLAS;
		blASScratchMemoryAllocateInfo.allocationSize = bottomLevelAccelerationStructureMemoryRequirements.size;
		blASScratchMemoryAllocateInfo.memoryTypeIndex = bottomLevelAccelerationStructureScratchMemoryTypeIndex;
		VkDeviceMemory bottomLevelAccelerationStructureDeviceScratchMemoryHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(logicalDevice, &blASScratchMemoryAllocateInfo, nullptr, &bottomLevelAccelerationStructureDeviceScratchMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Cannot allocate scratch memory!");
		}
		if (vkBindBufferMemory(logicalDevice, bottomLevelAccelerationStructureScratchBufferHandle, bottomLevelAccelerationStructureDeviceScratchMemoryHandle, 0) != VK_SUCCESS) {
			throw std::runtime_error("Cannot bind scratch memory!");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(bottomLevelAccelerationStructureDeviceScratchMemoryHandle)
			, "Bottom Level Acceleration Structure Device Scratch Memory");
#endif
		VkBufferDeviceAddressInfo blASScratchBufferDeviceAddressInfo;
		blASScratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		blASScratchBufferDeviceAddressInfo.pNext = NULL;
		blASScratchBufferDeviceAddressInfo.buffer = bottomLevelAccelerationStructureScratchBufferHandle;
		VkDeviceAddress blASScratchBuffeDeviceAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &blASScratchBufferDeviceAddressInfo);
		//Building the actual geometry
		//Set where we want the data to be saved to
		bottomLevelAccelerationBuildGeometryInfoKHR.pNext = NULL;
		bottomLevelAccelerationBuildGeometryInfoKHR.dstAccelerationStructure = meshInfo.bottomLevelAccelerationStructure;
		bottomLevelAccelerationBuildGeometryInfoKHR.scratchData.deviceAddress = blASScratchBuffeDeviceAddress;
		//BuildRangeInfo: the indices within the vertex arrays to source input geometry for the BLAS.
		VkAccelerationStructureBuildRangeInfoKHR blASBuildRangeInfo;
		blASBuildRangeInfo.primitiveCount = mesh.primativeCount;
		blASBuildRangeInfo.primitiveOffset = 0;
		blASBuildRangeInfo.transformOffset = 0;
		blASBuildRangeInfo.firstVertex = 0;
		//Create a read only array of 1
		const VkAccelerationStructureBuildRangeInfoKHR
			* bottomLevelAccelerationStructureBuildRangeInfos =
			&blASBuildRangeInfo;
		//Create the command buffers to submit the build command for the geometry
		//Allocate memory for command buffer
		if (mainCommandPool == nullptr) {
			throw std::runtime_error("Command Pool Expired / Null! Aborting BLAS creation!\n");
		}
		if(mainGraphicsQueue == nullptr) {
			throw std::runtime_error("Graphics Queue Expired / Null! Aborting BLAS creation!\n");
		}
		VkCommandPool commandPool = *mainCommandPool;
		VkQueue graphicsQueue = *mainGraphicsQueue;
		VkCommandBufferAllocateInfo commandBufferAllocationInfo{};
		commandBufferAllocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocationInfo.commandPool = commandPool;
		commandBufferAllocationInfo.commandBufferCount = 1;
		commandBufferAllocationInfo.pNext = NULL;
		//Memory transfer is executed using command buffers
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocationInfo, &commandBuffer);
		//Going to be completely unabstracted do to reference code
		VkCommandBufferBeginInfo bottomLevelCommandBufferBeginInfo;
		bottomLevelCommandBufferBeginInfo.pNext = NULL;
		bottomLevelCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bottomLevelCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		bottomLevelCommandBufferBeginInfo.pInheritanceInfo = NULL;
		if (vkBeginCommandBuffer(commandBuffer, &bottomLevelCommandBufferBeginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Ray tracing command buffer cant start!");
		}
		pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &bottomLevelAccelerationBuildGeometryInfoKHR, &bottomLevelAccelerationStructureBuildRangeInfos);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't end the command buffer");
		}
		//Submit and free the command buffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.pNext = NULL;
		//Helps use sync data between GPU and CPU
		VkFenceCreateInfo bottomLevelAccelerationStructureBuildFenceInfo;
		bottomLevelAccelerationStructureBuildFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		bottomLevelAccelerationStructureBuildFenceInfo.pNext = NULL;
		bottomLevelAccelerationStructureBuildFenceInfo.flags = 0;
		VkFence bottomLevelAccelerationStructureFence;
		if (vkCreateFence(logicalDevice, &bottomLevelAccelerationStructureBuildFenceInfo, nullptr, &bottomLevelAccelerationStructureFence) != VK_SUCCESS) {
			throw std::runtime_error("Fence failed to be created!");
		}
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, bottomLevelAccelerationStructureFence) != VK_SUCCESS) {
			throw std::runtime_error("Can't submit queue");
		}
		VkResult r = vkWaitForFences(logicalDevice, 1, &bottomLevelAccelerationStructureFence, true, UINT32_MAX);
		if (r != VK_SUCCESS && r != VK_TIMEOUT) {
			throw std::runtime_error("Failed to wait for fences");
		}
		vkDestroyFence(logicalDevice, bottomLevelAccelerationStructureFence, NULL);
		vkDestroyBuffer(logicalDevice, bottomLevelAccelerationStructureScratchBufferHandle,NULL);
		vkFreeMemory(logicalDevice, bottomLevelAccelerationStructureDeviceScratchMemoryHandle, NULL);
		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		bottomLevelMeshInfoList.push_back(meshInfo);
	}
	
	void RayTracer::createRayTracerImageAndImageView() {
		if (mainLogicalDevice == nullptr) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice == nullptr) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		if (mainSwapChainFormat == nullptr) {
			throw std::runtime_error("Main SwapChain Format is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice;
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice;
		VkFormat swapChainFormat = *mainSwapChainFormat;
		uint32_t queueFamilyIndex = findSimultaniousGraphicsAndPresentIndex(physicalDevice);
		//Settings for the image
		VkImageCreateInfo imageInfo{};
		imageInfo.pNext = NULL;
		imageInfo.queueFamilyIndexCount = 1;
		imageInfo.pQueueFamilyIndices = &queueFamilyIndex;
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(widthRef);
		imageInfo.extent.height = static_cast<uint32_t>(heightRef);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		//We need same format for the texels as the pixels in the buffer
		imageInfo.format = swapChainFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //discard textuals first transition
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT 
			| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional

		if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &rayTracerImage) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}
		//allocating memory to the image
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(logicalDevice, rayTracerImage, &memoryRequirements);

		VkMemoryAllocateInfo allocationInfo{};
		allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
		//Check to see what memory our graphics card has for the buffer
		uint32_t rayTraceImageMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memoryProperties.memoryTypeCount;
			x++) {
			if ((memoryRequirements.memoryTypeBits & (1 << x)) &&
				(memoryProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

				rayTraceImageMemoryTypeIndex = x;
				break;
			}
		}
		allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocationInfo.pNext = NULL;
		allocationInfo.allocationSize = memoryRequirements.size;
		allocationInfo.memoryTypeIndex = rayTraceImageMemoryTypeIndex;

		if (vkAllocateMemory(logicalDevice, &allocationInfo, nullptr, &rayTracerImageDeviceMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}
		//Bind the image to the allocated memory
		vkBindImageMemory(logicalDevice, rayTracerImage, rayTracerImageDeviceMemory, 0);
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(rayTracerImageDeviceMemory)
			, "Ray Trace Image Memory");
#endif
		//rtImage View
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = NULL;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = rayTracerImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapChainFormat;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &rayTracerImageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create ray tracing image view!");
		}
	}
	//Convert all the models into model info instances that we can pass to the top level acceleration struct for building
	//Make a refresh version by destorying buffer first and clearing instance list
	void RayTracer::InitalizeMeshInstances() {
		for (Model& model : ResourceManager::manager->modelList) {
			VkAccelerationStructureInstanceKHR instance;
			Mesh& refMesh = ResourceManager::manager->meshList[model.referenceMeshIndex];
			bottomLevelModelInstanceInfo.referenceModelIndices.push_back(model.resourceListIndex);
			VkAccelerationStructureKHR refBottomLevelAccelerationStructure = bottomLevelMeshInfoList[refMesh.referenceRayTracerMeshInfoIndex].bottomLevelAccelerationStructure;
			//Get the address to pass to the bl instance
			VkAccelerationStructureDeviceAddressInfoKHR bottomLevelAccelerationStructureDeviceAddressInfo;
			bottomLevelAccelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
			bottomLevelAccelerationStructureDeviceAddressInfo.accelerationStructure = refBottomLevelAccelerationStructure;
			bottomLevelAccelerationStructureDeviceAddressInfo.pNext = NULL;

			bottomLevelModelInstanceInfo.modelBottomLevelInstanceAddress = pvkGetAccelerationStructureDeviceAddressKHR(*mainLogicalDevice, &bottomLevelAccelerationStructureDeviceAddressInfo);
			//Look at createTopLevelAccelerationStructure to finish the rest of this.
			//Setup transform matrix
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 4; j++) {
					instance.transform.matrix[i][j] = model.modelMatrix[j][i];
				}
			}
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.accelerationStructureReference = bottomLevelModelInstanceInfo.modelBottomLevelInstanceAddress;
			instance.instanceCustomIndex = 0;
			instance.mask = 0xFF;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			bottomLevelModelInstanceInfo.modelBottomLevelInstances.push_back(instance);
		}
		//Create buffer for instance
		int modelListSize = ResourceManager::manager->modelList.size();
		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		//Create buffer for all of the instances of the for the models
		ResourceManager::manager->createBuffer(sizeof(VkAccelerationStructureInstanceKHR
				) * modelListSize, usageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				bottomLevelModelInstanceInfo.modelBottomLevelInstanceBuffer,
				bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory,
				true);
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(ResourceManager::manager->device,
			VkObjectType::VK_OBJECT_TYPE_BUFFER,
			reinterpret_cast<uint64_t>(bottomLevelModelInstanceInfo.modelBottomLevelInstanceBuffer)
			, "Bottom Level Instance Buffer");
#endif
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(ResourceManager::manager->device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory)
			, "Bottom Level Instance Device Memory");
#endif
		//COPY OVER INFO
		void* hostbottomLevelGeometryInstanceMemoryBuffer;
		VkResult result =
			vkMapMemory(ResourceManager::manager->device
				, bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory,
				0, sizeof(VkAccelerationStructureInstanceKHR)*modelListSize, 0,
				&hostbottomLevelGeometryInstanceMemoryBuffer);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Can't map memory");
		}

		memcpy(hostbottomLevelGeometryInstanceMemoryBuffer,
			bottomLevelModelInstanceInfo.modelBottomLevelInstances.data(),
			sizeof(VkAccelerationStructureInstanceKHR)*modelListSize);

		vkUnmapMemory(ResourceManager::manager->device
			, bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory);
	}
	void RayTracer::refreshMeshInstances() {
		//If we get more models, rebuild from scratch
		//We should avoid dynamic memory but it couldnt hurt to add a refresh
		//CASE A: We haven't initalized anything yet
		if (bottomLevelModelInstanceInfo.modelBottomLevelInstanceBuffer == NULL
			|| bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory == NULL) {
			InitalizeMeshInstances();
		}
		//CASE B: Changed size after buffer already initalized 
		else if (bottomLevelModelInstanceInfo.modelBottomLevelInstances.size()
			!= ResourceManager::manager->modelList.size()) {
			vkDestroyBuffer(ResourceManager::manager->device,
				bottomLevelModelInstanceInfo.modelBottomLevelInstanceBuffer
			,nullptr);
			vkFreeMemory(ResourceManager::manager->device,
				bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory
				, nullptr);
			bottomLevelModelInstanceInfo.referenceModelIndices.clear();
			bottomLevelModelInstanceInfo.modelBottomLevelInstances.clear();
			InitalizeMeshInstances();
		}
		//CASE C: No change in model amount -> just update matrices
		else {
			//COPY OVER INFO FROM MODELS TO INSTANCES
			for (int k = 0; k < ResourceManager::manager->modelList.size();k++) {
				//Setup transform matrix
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 4; j++) {
						bottomLevelModelInstanceInfo.modelBottomLevelInstances[k].transform.matrix[i][j] 
							= ResourceManager::manager->modelList[k].modelMatrix[j][i];
					}
				}
			}
			//COPY OVER INFO FROM INSTANCES TO GPU
			int modelListSize = ResourceManager::manager->modelList.size();
			void* hostbottomLevelGeometryInstanceMemoryBuffer;
			VkResult result =
				vkMapMemory(ResourceManager::manager->device
					, bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory,
					0, sizeof(VkAccelerationStructureInstanceKHR) * modelListSize, 0,
					&hostbottomLevelGeometryInstanceMemoryBuffer);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("Can't map memory");
			}

			memcpy(hostbottomLevelGeometryInstanceMemoryBuffer,
				bottomLevelModelInstanceInfo.modelBottomLevelInstances.data(),
				sizeof(VkAccelerationStructureInstanceKHR) * modelListSize);

			vkUnmapMemory(ResourceManager::manager->device
				, bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory);
		}
	}
	void RayTracer::createTopLevelAccelerationStructure() {
		if (mainLogicalDevice == nullptr) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice == nullptr) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice;
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice;
		//Setup mesh instances / update mesh instances
		refreshMeshInstances();
		//We assume that we already have initalized the instances for each model
		//We have the instance data, so now we are going to get the geometry data to pass into topLevelAccelerationStructure
		VkBufferDeviceAddressInfo bottomLevelGeometryInstanceDeviceAddressInfo;
		bottomLevelGeometryInstanceDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bottomLevelGeometryInstanceDeviceAddressInfo.buffer = bottomLevelModelInstanceInfo.modelBottomLevelInstanceBuffer;
		bottomLevelGeometryInstanceDeviceAddressInfo.pNext = NULL;
		bottomLevelModelInstanceInfo.modelBottomLevelInstanceAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &bottomLevelGeometryInstanceDeviceAddressInfo);
		//Geo data setup for top level 
		VkAccelerationStructureGeometryDataKHR topLevelGeometryData;
		topLevelGeometryData.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		topLevelGeometryData.instances.arrayOfPointers = VK_FALSE;
		topLevelGeometryData.instances.data.deviceAddress = bottomLevelModelInstanceInfo.modelBottomLevelInstanceAddress;
		topLevelGeometryData.instances.pNext = NULL;
		//top level structure being preped to be built
		VkAccelerationStructureGeometryKHR topLevelAccelerationStructureGeometry;
		topLevelAccelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		topLevelAccelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topLevelAccelerationStructureGeometry.geometry = topLevelGeometryData;
		topLevelAccelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		topLevelAccelerationStructureGeometry.pNext = NULL;
		//Settings used to build the actual geo
		VkAccelerationStructureBuildGeometryInfoKHR topLevelAccelerationStructureBuildGeoInfo;
		topLevelAccelerationStructureBuildGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		topLevelAccelerationStructureBuildGeoInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		topLevelAccelerationStructureBuildGeoInfo.flags = 0;
		topLevelAccelerationStructureBuildGeoInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		topLevelAccelerationStructureBuildGeoInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		topLevelAccelerationStructureBuildGeoInfo.dstAccelerationStructure = VK_NULL_HANDLE;
		topLevelAccelerationStructureBuildGeoInfo.geometryCount = 1;
		topLevelAccelerationStructureBuildGeoInfo.pGeometries = &topLevelAccelerationStructureGeometry;
		topLevelAccelerationStructureBuildGeoInfo.ppGeometries = NULL;
		topLevelAccelerationStructureBuildGeoInfo.scratchData.deviceAddress = 0;
		topLevelAccelerationStructureBuildGeoInfo.pNext = NULL;
		//How much should be allocated to the topLevelAccelerationStructureGeometry?
		VkAccelerationStructureBuildSizesInfoKHR topLevelAccelerationStructureBuildSizesInfo;
		topLevelAccelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		topLevelAccelerationStructureBuildSizesInfo.accelerationStructureSize = 0;
		topLevelAccelerationStructureBuildSizesInfo.updateScratchSize = 0;
		topLevelAccelerationStructureBuildSizesInfo.buildScratchSize = 0;
		topLevelAccelerationStructureBuildSizesInfo.pNext = NULL;
		//We are only going to have 1 primative since we only have 1 top level geo
		std::vector<uint32_t> topLevelMaxPrimitiveCountList = { 1 };
		pvkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&topLevelAccelerationStructureBuildGeoInfo,
			topLevelMaxPrimitiveCountList.data(),
			&topLevelAccelerationStructureBuildSizesInfo);
		uint32_t simultaniousIndex = findSimultaniousGraphicsAndPresentIndex(ResourceManager::manager->physicalDevice);
		VkBufferCreateInfo topLevelAccelerationStructureBufferCreateInfo;
		topLevelAccelerationStructureBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		topLevelAccelerationStructureBufferCreateInfo.size = topLevelAccelerationStructureBuildSizesInfo.accelerationStructureSize;
		topLevelAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
		topLevelAccelerationStructureBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		topLevelAccelerationStructureBufferCreateInfo.queueFamilyIndexCount = 1;
		topLevelAccelerationStructureBufferCreateInfo.pQueueFamilyIndices = &simultaniousIndex;
		topLevelAccelerationStructureBufferCreateInfo.pNext = NULL;
		topLevelAccelerationStructureBufferCreateInfo.flags = 0;
		//VkBuffer topLevelAccelerationStructureBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(logicalDevice, &topLevelAccelerationStructureBufferCreateInfo, nullptr, &topLevelAccelerationStructureBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Buffer for topLevelAccelerationStructure cannot be made!");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(topLevelAccelerationStructureBuffer)
			, "Top Level Accelertation Structure Buffer");
#endif
		//Check to see what memory our graphics card has for the buffer
		VkMemoryRequirements topLevelAccelerationStructureMemoryRequirements;
		vkGetBufferMemoryRequirements(
			logicalDevice, topLevelAccelerationStructureBuffer,
			&topLevelAccelerationStructureMemoryRequirements);

		uint32_t topLevelAccelerationStructureMemoryTypeIndex = findBufferMemoryTypeIndex(logicalDevice
			, physicalDevice, topLevelAccelerationStructureBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//Allocate memory to buffer the topLevelAccelerationStructure will be stored on
		VkMemoryAllocateInfo topLevelAccelerationStructureMemoryAllocateInfo;
		topLevelAccelerationStructureMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		topLevelAccelerationStructureMemoryAllocateInfo.allocationSize = topLevelAccelerationStructureMemoryRequirements.size;
		topLevelAccelerationStructureMemoryAllocateInfo.memoryTypeIndex = topLevelAccelerationStructureMemoryTypeIndex;
		topLevelAccelerationStructureMemoryAllocateInfo.pNext = NULL;

		VkDeviceMemory topLevelAccelerationStructureDeviceMemoryHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(logicalDevice, &topLevelAccelerationStructureMemoryAllocateInfo
			, NULL, &topLevelAccelerationStructureDeviceMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate memory for the topLevelAccelerationStructure buffer");
		}
		if (vkBindBufferMemory(logicalDevice, topLevelAccelerationStructureBuffer,topLevelAccelerationStructureDeviceMemoryHandle,0) != VK_SUCCESS) {
			throw std::runtime_error("Failed to bind the memory to the buffer from the device");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(topLevelAccelerationStructureDeviceMemoryHandle)
			, "Top Level Acceleration Structure Device Memory");
#endif
		topLevelAccelerationStructureDeviceMemory = topLevelAccelerationStructureDeviceMemoryHandle;
		//The settings for the topLevelAccelerationStructure
		VkAccelerationStructureCreateInfoKHR topLevelAccelerationStructureCreateInfo;
		topLevelAccelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		topLevelAccelerationStructureCreateInfo.createFlags = 0;
		topLevelAccelerationStructureCreateInfo.buffer = topLevelAccelerationStructureBuffer;
		topLevelAccelerationStructureCreateInfo.offset = 0;
		topLevelAccelerationStructureCreateInfo.size = topLevelAccelerationStructureBuildSizesInfo.accelerationStructureSize;
		topLevelAccelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		topLevelAccelerationStructureCreateInfo.deviceAddress = 0;
		topLevelAccelerationStructureCreateInfo.pNext = NULL;
		if (pvkCreateAccelerationStructureKHR(logicalDevice, &topLevelAccelerationStructureCreateInfo, NULL, &topLevelAccelerationStructure) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create the topLevelAccelerationStructure");
		}
		//Building the topLevelAccelerationStructure
		//Setup the address we are going to build the topLevelAccelerationStructure on
		//Building here means populating the structure with data and such
		VkAccelerationStructureDeviceAddressInfoKHR topLevelAccelerationStructureDeviceAddressInfo;
		topLevelAccelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		topLevelAccelerationStructureDeviceAddressInfo.accelerationStructure = topLevelAccelerationStructure;
		topLevelAccelerationStructureDeviceAddressInfo.pNext = NULL;

		VkDeviceAddress topLevelAccelerationStructureDeviceAddress =
			pvkGetAccelerationStructureDeviceAddressKHR(
				logicalDevice, &topLevelAccelerationStructureDeviceAddressInfo);
		//We are going to make a temporary buffer to help store info related to building the topLevelAccelerationStructure
		VkBufferCreateInfo topLevelAccelerationStructureScratchBufferCreateInfo;
		topLevelAccelerationStructureScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			topLevelAccelerationStructureScratchBufferCreateInfo.pNext = NULL;
			topLevelAccelerationStructureScratchBufferCreateInfo.flags = 0;
			topLevelAccelerationStructureScratchBufferCreateInfo.size = topLevelAccelerationStructureBuildSizesInfo.buildScratchSize;
			topLevelAccelerationStructureScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			topLevelAccelerationStructureScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			topLevelAccelerationStructureScratchBufferCreateInfo.queueFamilyIndexCount = 1;
			topLevelAccelerationStructureScratchBufferCreateInfo.pQueueFamilyIndices = &simultaniousIndex;

		VkBuffer topLevelAccelerationStructureScratchBuffer= VK_NULL_HANDLE;
		if (vkCreateBuffer(
			logicalDevice, &topLevelAccelerationStructureScratchBufferCreateInfo, NULL,
			&topLevelAccelerationStructureScratchBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Scratch memory buffer couldnt be built");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER
			, reinterpret_cast<uint64_t>(topLevelAccelerationStructureScratchBuffer)
			, "Top Level Accelertation Structure Scratch Buffer");
#endif
		//Get memory req to determine what kinds of memory the GPU has
		VkMemoryRequirements topLevelAccelerationStructureScratchMemoryRequirments;
		vkGetBufferMemoryRequirements(logicalDevice, topLevelAccelerationStructureScratchBuffer, &topLevelAccelerationStructureScratchMemoryRequirments);
		topLevelAccelerationStructureMemoryTypeIndex = findBufferMemoryTypeIndex(logicalDevice
			, physicalDevice, topLevelAccelerationStructureScratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//Time to allocate memory to the buffer we are going to build on
		VkMemoryAllocateInfo topLevelAccelerationStructureScratchMemoryAllocateInfo;
		VkMemoryAllocateFlagsInfo defaultFlagsScratch = getDefaultAllocationFlags();
		topLevelAccelerationStructureScratchMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		topLevelAccelerationStructureScratchMemoryAllocateInfo.pNext = &defaultFlagsScratch;
		topLevelAccelerationStructureScratchMemoryAllocateInfo.allocationSize = topLevelAccelerationStructureScratchMemoryRequirments.size;
		topLevelAccelerationStructureScratchMemoryAllocateInfo.memoryTypeIndex = topLevelAccelerationStructureMemoryTypeIndex;
		VkDeviceMemory topLevelAccelerationStructureDeviceScratchMemoryHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(logicalDevice, &topLevelAccelerationStructureScratchMemoryAllocateInfo, NULL, &topLevelAccelerationStructureDeviceScratchMemoryHandle)
			!= VK_SUCCESS) {
			throw std::runtime_error("Couldn't allocate memory to scratch buffer for topLevelAccelerationStructure");
		}
		//Need to bind the allocated memory to the scratch buffer so we know where to build it
		if (vkBindBufferMemory(logicalDevice, topLevelAccelerationStructureScratchBuffer, topLevelAccelerationStructureDeviceScratchMemoryHandle, 0)
			!= VK_SUCCESS) {
			throw std::runtime_error("Couldn't bind memeory for hte scratch buffer");
		}
#ifndef NDEBUG
		ResourceManager::setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(topLevelAccelerationStructureDeviceScratchMemoryHandle)
			, "Top Level Acceleration Structure Scratch Device Memory");
#endif
		//We need to get the device address of the scratch buffer so we can direct future code to the correct
		//place to build the topl level geometry!
		//This gets us the info used to get the actual addresss
		VkBufferDeviceAddressInfo topLevelAccelerationStructureScratchBufferDeviceAddressInfo;
		topLevelAccelerationStructureScratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		topLevelAccelerationStructureScratchBufferDeviceAddressInfo.pNext = NULL;
		topLevelAccelerationStructureScratchBufferDeviceAddressInfo.buffer = topLevelAccelerationStructureScratchBuffer;
		//Time to actually get the device address and use it to tell the scratch buffer where to build the topLevelAccelerationStructure
		VkDeviceAddress topLevelAccelerationStructureScratchBufferDeviceAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &topLevelAccelerationStructureScratchBufferDeviceAddressInfo);
		topLevelAccelerationStructureBuildGeoInfo.dstAccelerationStructure = topLevelAccelerationStructure;
		topLevelAccelerationStructureBuildGeoInfo.scratchData.deviceAddress = topLevelAccelerationStructureScratchBufferDeviceAddress;
		//We need to tell the pipeline what offsets to expect for the geometry 
		VkAccelerationStructureBuildRangeInfoKHR topLevelAccelerationStructureSBuildRangeInfo;
		topLevelAccelerationStructureSBuildRangeInfo.firstVertex = 0;
		topLevelAccelerationStructureSBuildRangeInfo.primitiveCount = 1;
		topLevelAccelerationStructureSBuildRangeInfo.primitiveOffset = 0;
		topLevelAccelerationStructureSBuildRangeInfo.transformOffset = 0;
		//We only have an array of 1 since there is only 1 primative here
		VkAccelerationStructureBuildRangeInfoKHR* topLevelAccelerationStructureSBuildRangeInfos = &topLevelAccelerationStructureSBuildRangeInfo;
		//We need to now create a command buffer and submit our memory transfer so we can build the topLevelAccelerationStructure
		//Allocate memory for command buffer
		if (mainCommandPool == nullptr) {
			throw std::runtime_error("Commaned pool has expired or is null!\n");
		}
		VkCommandPool commandPool = *mainCommandPool;
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.pNext = NULL;
		//Memory transfer is executed using command buffers
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);
		//Going to be completely unabstracted do to reference code
		VkCommandBufferBeginInfo topLevelCommandBufferBeginInfo;
		topLevelCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		topLevelCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		topLevelCommandBufferBeginInfo.pNext = NULL;
		topLevelCommandBufferBeginInfo.pInheritanceInfo = NULL;
		if (vkBeginCommandBuffer(commandBuffer, &topLevelCommandBufferBeginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Ray tracing command buffer cant start!");
		}
		//Add the command we want to submmit, which is to finally build the topLevelAccelerationStructure!
		pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &topLevelAccelerationStructureBuildGeoInfo, &topLevelAccelerationStructureSBuildRangeInfos);
		//End Wrap up our command buffer and submit it to the pool to run on the gpu!
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Ray tracing command buffer cant finish!");
		}
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = NULL;
		submitInfo.pWaitDstStageMask = NULL;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = NULL;
		submitInfo.pNext = NULL;
		//Get a fence for transfering the command buffer over
		VkFenceCreateInfo topLevelFenceInfo;
		topLevelFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		topLevelFenceInfo.pNext = NULL;
		topLevelFenceInfo.flags = 0;
		if (mainGraphicsQueue == nullptr) {
			throw std::runtime_error("Graphics Queue expired\n");
		}
		VkQueue graphicsQueue = *mainGraphicsQueue;
		VkFence topLevelFence;
		if (vkCreateFence(logicalDevice, &topLevelFenceInfo, nullptr, &topLevelFence) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't make the fence for the topLevelAccelerationStructure!");
		}
		//Submit the command buffer and check on the fences
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, topLevelFence) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't queue command buffer!");
		}
		VkResult r = vkWaitForFences(logicalDevice, 1, &topLevelFence, true, UINT32_MAX);
		if (r != VK_SUCCESS && r != VK_TIMEOUT) {
			throw std::runtime_error("Failed to wait for fences");
		}
		//Free up scratch buffers
		vkDestroyBuffer(logicalDevice, topLevelAccelerationStructureScratchBuffer, NULL);
		vkFreeMemory(logicalDevice,topLevelAccelerationStructureDeviceScratchMemoryHandle, NULL);
		//Free up our one time command buffer submission
		vkDestroyFence(logicalDevice, topLevelFence, NULL);
		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
	}
	void RayTracer::recreateTopLevelAccelerationStrucuture() {
		vkDestroyBuffer(ResourceManager::manager->device,
			topLevelAccelerationStructureBuffer,
			nullptr);
		pvkDestroyAccelerationStructureKHR(ResourceManager::manager->device,
			topLevelAccelerationStructure,
			nullptr);
		createTopLevelAccelerationStructure();

	}
	VkAccelerationStructureKHR* RayTracer::getTopLevelAccelerationStructure() {
		return &topLevelAccelerationStructure;
	}
	VkAccelerationStructureKHR* RayTracer::getBottomLevelAccelerationStructure(int index) {
		return &bottomLevelMeshInfoList.at(index).bottomLevelAccelerationStructure;
	}
	void RayTracer::rayTrace(VkCommandBuffer& cmdBuf,std::vector<void *>& uniBufferMMap, glm::vec4 clearColor) {
		if (mainLogicalDevice == nullptr) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice == nullptr) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		if (rayTracerFences == nullptr) {
			throw std::runtime_error("In flight fences are expired / null!\n");
		}
		if (rayTracerSwapchain == nullptr) {
			throw std::runtime_error("swapchain reference has expired / null!\n");
		}
		if (rayTracerImageAvailableSemaphores == nullptr) {
			throw std::runtime_error("available semaphores ref has expired / null!\n");
		}
		if (rayTracerFinishedSemaphores == nullptr) {
			throw std::runtime_error("finished semaphores ref has expired / null!\n");
		}
		if (rayTracerPresentQueue == nullptr) {
			throw std::runtime_error("Present Queue has expired / null!\n");
		}
		if (mainLightSource == nullptr) {
			throw std::runtime_error("main light source has expired / null!\n");
		}
		if (mainGraphicsQueue == nullptr) {
			throw std::runtime_error("Graphics Queue has expired\n");
		}
		if (rayTracerSwapchainImages == nullptr) {
			throw std::runtime_error("swachain images has expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice;
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice;
		std::vector<VkFence> fences = *rayTracerFences;
		uint32_t currentFrame = *currentFrameRef;
		VkSwapchainKHR swapChain = *rayTracerSwapchain;
		std::vector<VkDescriptorSet> mainDescSetVector = *mainDescSets;
		std::vector <VkSemaphore> availableSemaphores = *rayTracerImageAvailableSemaphores;
		std::vector <VkSemaphore> finishedSemaphores = *rayTracerFinishedSemaphores;
		VkQueue presentQueue = *rayTracerPresentQueue;
		VkQueue graphicsQueue = *mainGraphicsQueue;
		LightSource lightSource = *mainLightSource;
		std::vector<VkImage> swapchainImages = *rayTracerSwapchainImages;
		//submit queue
		//Wait for frame to be finished drawing
		VkResult fenceResult = vkWaitForFences(logicalDevice, 1, &(fences[currentFrame]), VK_TRUE, UINT64_MAX);
		if (fenceResult != VK_SUCCESS && fenceResult != VK_TIMEOUT) {
			std::cout << "Ray trace function failure!\n";
			throw std::runtime_error("failed to wait for fences!");
		}
		uint32_t imageIndex;
		//Make sure the chain is fresh so we know we can use it. This allows us to delay a fense reset and stop a deadlock
		VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, availableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			//recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		vkResetFences(logicalDevice, 1, &fences[currentFrame]);
		//update unform buffers and descriptor sets

		for (int i = 0; i < ResourceManager::manager->modelList.size(); i++) {
			Model* m = &ResourceManager::manager->modelList[i];
			m->testUpdate();
			m->updateUniformBuffers(currentFrame);
		}
		refRayTracingPipeline->updateRayTracerDescriptorSets(currentFrame);
		//Setup light source
		pushConstantRay.clearColor = clearColor;
		pushConstantRay.lightPos = lightSource.pos;
		pushConstantRay.lightIntensity = lightSource.intensity;
		pushConstantRay.lightType = lightSource.type;
		//Recreate top level structure after updating models and descriptor sets
		recreateTopLevelAccelerationStrucuture();
		//Command buffer setup
		vkResetCommandBuffer(cmdBuf, 0);

		//Building pipeline and layout
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional Controls how command buffer will be used
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(cmdBuf, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };

		//Sync resoureces to transfer out src image to the gpu!
		//Undefined -> General Layout
		VkImageMemoryBarrier rayTraceBarrier{};
		rayTraceBarrier.pNext = NULL;
		rayTraceBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		rayTraceBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		rayTraceBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		rayTraceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		rayTraceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		rayTraceBarrier.image = rayTracerImage; //specify image effected by rayTraceBarrier
		rayTraceBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		rayTraceBarrier.subresourceRange.baseMipLevel = 0;
		rayTraceBarrier.subresourceRange.levelCount = 1;
		rayTraceBarrier.subresourceRange.baseArrayLayer = 0;
		rayTraceBarrier.subresourceRange.layerCount = 1;
		rayTraceBarrier.srcAccessMask = 0; // TODO Need to specify which operations need to do
		rayTraceBarrier.dstAccessMask = 0; // TODO
		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL,
			1, &rayTraceBarrier);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, refRayTracingPipeline->pipeline);
		//Bind all of our descriptor sets we updated earlier
		for (int i = 0; i < ResourceManager::manager->modelList.size(); i++) {
			//Desc sets to bind
			std::vector<VkDescriptorSet> descSets{ 
				refRayTracingPipeline->descriptorSets.at(2*i+currentFrame)
				, (mainDescSetVector.at(2 * i + currentFrame))
			};
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
			refRayTracingPipeline->pipelineLayout, 0, (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
		}
		vkCmdPushConstants(cmdBuf, refRayTracingPipeline->pipelineLayout
			, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
			0, sizeof(PushConstantRay), &pushConstantRay);
		pvkCmdTraceRaysKHR(cmdBuf, refRayTracingPipeline->getShaderRegionAddress(0)
			, refRayTracingPipeline->getShaderRegionAddress(1)
			, refRayTracingPipeline->getShaderRegionAddress(2)
			, refRayTracingPipeline->getShaderRegionAddress(3)
			, widthRef, heightRef, 1);
		//Once the ray is traced, we can start copying the results over into the swap chain
		//We make a barrier so we can copy the rtImage into the swapChain
		VkImageMemoryBarrier swapchainCopyMemoryBarrier;
		swapchainCopyMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		swapchainCopyMemoryBarrier.pNext = NULL;
		swapchainCopyMemoryBarrier.srcAccessMask = 0;
		swapchainCopyMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		swapchainCopyMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		swapchainCopyMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		swapchainCopyMemoryBarrier.srcQueueFamilyIndex = queueFamilyIndices[0];
		swapchainCopyMemoryBarrier.dstQueueFamilyIndex = queueFamilyIndices[0];
		swapchainCopyMemoryBarrier.image = swapchainImages[currentFrame];
		VkImageSubresourceRange subRange;
		subRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subRange.baseMipLevel = 0;
		subRange.levelCount = 1;
		subRange.baseArrayLayer = 0;
		subRange.layerCount = 1;
		swapchainCopyMemoryBarrier.subresourceRange = subRange;

		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
			NULL, 1, &swapchainCopyMemoryBarrier);

		VkImageMemoryBarrier rayTraceCopyMemoryBarrier;
		rayTraceCopyMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		rayTraceCopyMemoryBarrier.pNext = NULL;
		rayTraceCopyMemoryBarrier.srcAccessMask = 0;
		rayTraceCopyMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		rayTraceCopyMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		rayTraceCopyMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		rayTraceCopyMemoryBarrier.srcQueueFamilyIndex = queueFamilyIndices[0];
		rayTraceCopyMemoryBarrier.dstQueueFamilyIndex = queueFamilyIndices[0];
		rayTraceCopyMemoryBarrier.image = rayTracerImage;
		rayTraceCopyMemoryBarrier.subresourceRange = subRange;
		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
			NULL, 1, &rayTraceCopyMemoryBarrier);
		VkOffset3D offset;
		offset.x = 0;
		offset.y = 0;
		offset.z = 0;
		VkImageSubresourceLayers subLayers;
		VkExtent3D swapExtent;
		swapExtent.width = static_cast<uint32_t>(widthRef);
		swapExtent.height = static_cast<uint32_t>(heightRef);
		swapExtent.depth = 1;
		subLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subLayers.mipLevel = 0;
		subLayers.baseArrayLayer = 0;
		subLayers.layerCount = 1;
		VkImageCopy imageCopy;
		imageCopy.srcSubresource = subLayers;
		imageCopy.srcOffset = offset;
		imageCopy.dstSubresource = subLayers;
		imageCopy.dstOffset = offset;
		imageCopy.extent = swapExtent;

		vkCmdCopyImage(cmdBuf, rayTracerImage,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			swapchainImages[currentFrame],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

		VkImageMemoryBarrier swapchainPresentMemoryBarrier;
		swapchainPresentMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		swapchainPresentMemoryBarrier.pNext = NULL;
		swapchainPresentMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		swapchainPresentMemoryBarrier.dstAccessMask = 0;
		swapchainPresentMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		swapchainPresentMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		swapchainPresentMemoryBarrier.srcQueueFamilyIndex = queueFamilyIndices[0];
		swapchainPresentMemoryBarrier.dstQueueFamilyIndex = queueFamilyIndices[0];
		swapchainPresentMemoryBarrier.image = swapchainImages[currentFrame];
		swapchainPresentMemoryBarrier.subresourceRange = subRange;

		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
			NULL, 1, &swapchainPresentMemoryBarrier);

		VkImageMemoryBarrier rayTraceWriteMemoryBarrier;
		rayTraceWriteMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			rayTraceWriteMemoryBarrier.pNext = NULL,
			rayTraceWriteMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			rayTraceWriteMemoryBarrier.dstAccessMask = 0,
			rayTraceWriteMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			rayTraceWriteMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			rayTraceWriteMemoryBarrier.srcQueueFamilyIndex = queueFamilyIndices[0],
			rayTraceWriteMemoryBarrier.dstQueueFamilyIndex = queueFamilyIndices[0],
			rayTraceWriteMemoryBarrier.image = rayTracerImage,
			rayTraceWriteMemoryBarrier.subresourceRange = subRange;

		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
			NULL, 1, &rayTraceWriteMemoryBarrier);
		if (vkEndCommandBuffer(cmdBuf) != VK_SUCCESS) {
			throw std::runtime_error("failed to end recording command buffer!");
		}
		//submit the command buffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		//Specify which semaphores to wait on before execution beings and in which stages of the pipeline to wait
		VkSemaphore waitSemaphores[] = { availableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		//Specify which command buffer to submit
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;
		//Specify which semaphores to signal once the command buffers finished execution
		VkSemaphore signalSemaphores[] = { finishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, fences[currentFrame]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}
		//Specify which semaphores to wait on
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		//specify the swap chain to present images to and the index for each chain
		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		//Presents the triangle we submitted to the queue
		result = vkQueuePresentKHR(presentQueue, &presentInfo);
		//If the swapchain is out dated, then we need to recreate it!
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			//framebufferResized = false;
			//recreateSwapChain();
			std::cout << "Don't have anything to deal with this yet";
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
	}
	uint32_t RayTracer::findBufferMemoryTypeIndex(VkDevice logicalDevice,VkPhysicalDevice physicalDevice
		, VkBuffer buffer,VkMemoryPropertyFlagBits flagBits) {
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
		uint32_t memoryTypeIndex = -1;
		for (uint32_t x = 0; x < memoryProperties.memoryTypeCount;
			x++) {

			if ((memoryRequirements.memoryTypeBits &
				(1 << x)) &&
				(memoryProperties.memoryTypes[x].propertyFlags & flagBits) == flagBits) {
				memoryTypeIndex = x;
				break;
			}
		}
		return memoryTypeIndex;
	}
	//From Main: FIGURE OUT HOW TO REPLACE THIS AND AVOID COPYING CODE!
	QueueFamilyIndices RayTracer::findQueueFamilies(VkPhysicalDevice device) {
		if (mainSurface == nullptr) {
			throw std::runtime_error("main Surface is null or expired\n");
		}
		VkSurfaceKHR surface = *mainSurface;
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
	uint32_t RayTracer::findSimultaniousGraphicsAndPresentIndex(VkPhysicalDevice phyDevice) {
		if (mainSurface == nullptr) {
			throw std::runtime_error("main Surface is null or expired\n");
		}
		VkSurfaceKHR surface = *mainSurface;

		uint32_t simultQueueFamilyIndex = -1;
		//Retrive a list of queue familes
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties>queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(phyDevice, &queueFamilyCount, queueFamilies.data());
		//QueuFamilyProperies contains details like what operations are acceptiable and the number of queues that cant be created 
		int i = 0;
		for (const auto& queueFamiliy : queueFamilies) {
			//Look for a queueFamily that supports Grpahics
			if (queueFamiliy.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(phyDevice, i, surface, &presentSupport);
				//Check to see if the queue family supports presenting window surfaces
				if (presentSupport) {
					simultQueueFamilyIndex = i;
					break;
				}
			}
			
			i++;
		}
		return simultQueueFamilyIndex;
	}
	
	void RayTracer::cleanup() {
		if (mainLogicalDevice == nullptr) {
			throw std::runtime_error("main Logical Device is null or expired\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice;

		//Acceleration Structures
		pvkDestroyAccelerationStructureKHR(logicalDevice, topLevelAccelerationStructure, nullptr);
		vkDestroyBuffer(logicalDevice, topLevelAccelerationStructureBuffer, nullptr);
		vkFreeMemory(logicalDevice, topLevelAccelerationStructureDeviceMemory, nullptr);
		for (RayTracerMeshInfo rMeshInfo : bottomLevelMeshInfoList) {
			pvkDestroyAccelerationStructureKHR(logicalDevice, rMeshInfo.bottomLevelAccelerationStructure, nullptr);
			vkDestroyBuffer(logicalDevice, rMeshInfo.bottomLevelAccelerationStructureBuffer, nullptr);
			vkFreeMemory(logicalDevice, rMeshInfo.bottomLevelAccelerationStructureDeviceMemory, nullptr);
		}
		//Model top level instances
		vkDestroyBuffer(logicalDevice, bottomLevelModelInstanceInfo.modelBottomLevelInstanceBuffer, nullptr);
		vkFreeMemory(logicalDevice, bottomLevelModelInstanceInfo.modelBottomLevelInstanceMemory, nullptr);
		//Cleanup the raytracing pipeline we are using
		refRayTracingPipeline->cleanup();
		// Ray Trace Image
		vkDestroyImageView(logicalDevice, rayTracerImageView, nullptr);
		vkDestroyImage(logicalDevice, rayTracerImage, nullptr);
		vkFreeMemory(logicalDevice, rayTracerImageDeviceMemory, nullptr);

	}
