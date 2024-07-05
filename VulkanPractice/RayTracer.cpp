#include "RayTracer.h"

//Using the following link as a referencce: https://github.com/WilliamLewww/vulkan_ray_tracing_minimal_abstraction/blob/master/ray_pipeline/src/main.cpp

	VkMemoryAllocateFlagsInfo RayTracer::getDefaultAllocationFlags() {
		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo;
			memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
			memoryAllocateFlagsInfo.pNext = NULL,
			memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
			memoryAllocateFlagsInfo.deviceMask = 0;
		return memoryAllocateFlagsInfo;
	}
	//GO THROUGH EVERYTHING AND MAKE SURE SCRATCH BUFFERS ARE FREED!!!!
	void RayTracer::setupRayTracer(VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t nOfVerts) {
		if (mainLogicalDevice.expired()) { 
			throw std::runtime_error("main Logical Device is null or expired\n");
		}
#ifndef NDEBUG
		pvkSetDebugUtilsObjectNameEXT =
			(PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
				*mainLogicalDevice.lock(), "vkSetDebugUtilsObjectNameEXT");
#endif
		initRayTracing();
		modelToBottomLevelAccelerationStructure(vertexBuffer, indexBuffer, nOfVerts);
		createTopLevelAccelerationStructure();
		createRayTracerImageAndImageView();
		createRayTracerDescriptorSetLayout();
		createRayTracerDescriptorPool();
		createRayTracerDescriptorSets(vertexBuffer,indexBuffer);
		createRayTracingPipeline();
		createShaderBindingTable();
	}
	//Check to see if our GPU supports raytracing
	void RayTracer::initRayTracing()
	{
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("main Logical Device is null or expired\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("main Physical Device is null or expired\n");
		}
		//Setup function pointers for ray trace functions
		VkDevice logicDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
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
	void RayTracer::modelToBottomLevelAccelerationStructure(VkBuffer &vertexBuffer, VkBuffer&indexBuffer, uint32_t nOfVerts) {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is null or expired\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is null or expired\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		VkBufferDeviceAddressInfo vInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		vInfo.pNext = NULL;
		vInfo.buffer = vertexBuffer;
		VkDeviceAddress vertexAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &vInfo);
		vInfo.buffer = indexBuffer;
		VkDeviceAddress indexAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &vInfo);
		uint32_t umaxPrimativeCount = static_cast<uint32_t>(maxPrimativeCount);

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		//Vertex buffer description
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexStride = sizeof(Vertex);
		//Index buffer description
		triangles.indexData.deviceAddress = indexAddress;
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.maxVertex = nOfVerts;
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
			//VkBuffer accelerationBufferHandle = VK_NULL_HANDLE;
			if (vkCreateBuffer(logicalDevice, &bottomLevelAccelerationStructureBufferCreateInfo, nullptr, &bottomLevelAccelerationStructureBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create buffer for bASS");
			}
#ifndef NDEBUG
			setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bottomLevelAccelerationStructureBuffer)
				, "Bottom Level Accelertation Structure Buffer");
#endif
		//Look to see if our graphics card and our blAS has a local bit for our buffer
		uint32_t bottomLevelAccelerationStructureMemoryTypeIndex 
			= findBufferMemoryTypeIndex(logicalDevice,physicalDevice,bottomLevelAccelerationStructureBuffer
				,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkMemoryAllocateFlagsInfo defaultFlagsBLAS = getDefaultAllocationFlags();
		VkMemoryRequirements bottomLevelAccelerationStructureMemoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, 
			bottomLevelAccelerationStructureBuffer, &bottomLevelAccelerationStructureMemoryRequirements);
		//We are now allocating memory to the blAS
		VkMemoryAllocateInfo bottomLevelAccelerationStructureMemoryAllocateInfo;
		bottomLevelAccelerationStructureMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		bottomLevelAccelerationStructureMemoryAllocateInfo.pNext = NULL;//&defaultFlagsBLAS;
		bottomLevelAccelerationStructureMemoryAllocateInfo.allocationSize = bottomLevelAccelerationStructureMemoryRequirements.size;
		bottomLevelAccelerationStructureMemoryAllocateInfo.memoryTypeIndex = bottomLevelAccelerationStructureMemoryTypeIndex;
		bottomLevelAccelerationStructureDeviceMemory = VK_NULL_HANDLE;
		if (vkAllocateMemory(logicalDevice, &bottomLevelAccelerationStructureMemoryAllocateInfo, nullptr, &bottomLevelAccelerationStructureDeviceMemory) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt allocate memory for buffer!");
		}
		//Bind the allocated memory
		if (vkBindBufferMemory(logicalDevice, bottomLevelAccelerationStructureBuffer,bottomLevelAccelerationStructureDeviceMemory,0) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt bind memory to buffer!");
		}
#ifndef NDEBUG
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(bottomLevelAccelerationStructureDeviceMemory)
			, "Bottom Level Acceleration Structure Device Memory");
#endif
		//Create acc structure
		VkAccelerationStructureCreateInfoKHR bottomLevelAccelerationStructureInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		bottomLevelAccelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		bottomLevelAccelerationStructureInfo.createFlags = 0;
		bottomLevelAccelerationStructureInfo.buffer = bottomLevelAccelerationStructureBuffer;
		bottomLevelAccelerationStructureInfo.offset = 0;
		bottomLevelAccelerationStructureInfo.size = bottomLevelAccelerationBuildSizesInfo.accelerationStructureSize;
		bottomLevelAccelerationStructureInfo.deviceAddress = 0;
		bottomLevelAccelerationStructureInfo.pNext = NULL;

		if (pvkCreateAccelerationStructureKHR(logicalDevice, &bottomLevelAccelerationStructureInfo, nullptr, &bottomLevelAccelerationStructure) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt create bottom acceleration structure");
		}
		//Building Bottom level acceleartion structure
		VkAccelerationStructureDeviceAddressInfoKHR bottomLevelAccelerationStructureDeviceAddressInfo;
		bottomLevelAccelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		bottomLevelAccelerationStructureDeviceAddressInfo.pNext = NULL;
		bottomLevelAccelerationStructureDeviceAddressInfo.accelerationStructure = bottomLevelAccelerationStructure;
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bottomLevelAccelerationStructureScratchBufferHandle)
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
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
		bottomLevelAccelerationBuildGeometryInfoKHR.dstAccelerationStructure = bottomLevelAccelerationStructure;
		bottomLevelAccelerationBuildGeometryInfoKHR.scratchData.deviceAddress = blASScratchBuffeDeviceAddress;
		//BuildRangeInfo: the indices within the vertex arrays to source input geometry for the BLAS.
		VkAccelerationStructureBuildRangeInfoKHR blASBuildRangeInfo;
		blASBuildRangeInfo.primitiveCount = maxPrimativeCount;
		blASBuildRangeInfo.primitiveOffset = 0;
		blASBuildRangeInfo.transformOffset = 0;
		blASBuildRangeInfo.firstVertex = 0;
		//Create a read only array of 1
		const VkAccelerationStructureBuildRangeInfoKHR
			* bottomLevelAccelerationStructureBuildRangeInfos =
			&blASBuildRangeInfo;
		//Create the command buffers to submit the build command for the geometry
		//Allocate memory for command buffer
		if (mainCommandPool.expired()) {
			throw std::runtime_error("Command Pool Expired / Null! Aborting BLAS creation!\n");
		}
		if(mainGraphicsQueue.expired()) {
			throw std::runtime_error("Graphics Queue Expired / Null! Aborting BLAS creation!\n");
		}
		VkCommandPool commandPool = *mainCommandPool.lock();
		VkQueue graphicsQueue = *mainGraphicsQueue.lock();
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
	}
	void RayTracer::createShaderBindingTable() {
		// So I can't query the GPU since I dont have the device feature for it. Will use physical device queryig instead
		//Maps which shaders we should call for different entrypoints
		//Setting up buffer offsets to store the shader handles in
		//32bit for RG, 16 for miss,padd out another 16, and finally 16 for hit
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		const uint32_t missCount = 1;
		const uint32_t hitCount = 1;
		const auto handleCount = 1 + missCount + hitCount;
		//Can't guarintee that alignment matches group size or handle so we should round up to nearest bit
		const uint32_t handleSize = rayTracingProperties.shaderGroupHandleSize;
		const uint32_t handleAlign = rayTracingProperties.shaderGroupHandleAlignment;
		const uint32_t baseAlign = rayTracingProperties.shaderGroupBaseAlignment;

		//Aligns handles using handle and base alignments, offset by the size
		//See this page for formula explaination: https://en.wikipedia.org/wiki/Data_structure_alignment
		const uint32_t handleSizeAligned = (handleSize + (handleAlign-1)) & ~(handleAlign - 1);
		rayGenerationRegion.stride = (handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
		rayGenerationRegion.size = rayGenerationRegion.stride;
		rayMissRegion.stride = handleSizeAligned;
		rayMissRegion.size = (missCount * handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
		rayHitRegion.stride = handleSizeAligned;
		rayHitRegion.size = (hitCount * handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
		//Vector to store the handles to each shader
		const uint32_t dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		const auto result = pvkGetRayTracingShaderGroupHandlesKHR(logicalDevice, raytracingPipeline, 0, handleCount, dataSize, handles.data());
		//Im breaking my consistentcy rules because this way is so much better and I want to demonstrate the better way to do this valdiatioN!
		assert(result == VK_SUCCESS);
		
		//Allocate buffer for SBT
		uint32_t queueFamilyIndex = findSimultaniousGraphicsAndPresentIndex(physicalDevice);
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(shaderBindingTableBuffer)
			, "Shader Binding Table Buffer");
#endif
		//Query the memory requirements to make sure we have enough space to allocate for the vertex buffer
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(logicalDevice ,shaderBindingTableBuffer, &memoryRequirements);
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
		//Check to see what memory our graphics card has for the buffer
		uint32_t shaderBindingTableMemoryTypeIndex = 
			findBufferMemoryTypeIndex(logicalDevice,physicalDevice,shaderBindingTableBuffer
				, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		//Allocate memory for the buffer
		const VkMemoryAllocateFlagsInfo flags = getDefaultAllocationFlags();
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(shaderBindingTableDeviceMemory)
			, "Shader Binding Table Memory");
#endif

		//Storing device addresses for the shader groups
		VkBufferDeviceAddressInfo info;
		info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		info.pNext = nullptr;
		info.buffer = shaderBindingTableBuffer;
		VkDeviceAddress shaderBindingTableAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &info);
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
		pData = pShaderBindingTableBuffer + rayGenerationRegion.size +rayMissRegion.size;
		//for loop to copy multiple miss shaders in case we add more
		for (uint32_t c = 0; c < hitCount; c++) {
			memcpy(pData, getHandle(handleIdx++), handleSize);
			pData += rayHitRegion.stride;
		}
		//CLeanup resources
		vkUnmapMemory(logicalDevice, shaderBindingTableDeviceMemory);
	}
	
	void RayTracer::createRayTracerImageAndImageView() {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		if (mainSwapChainFormat.expired()) {
			throw std::runtime_error("Main SwapChain Format is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		VkFormat swapChainFormat = *mainSwapChainFormat.lock();
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
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
	void RayTracer::createRayTracerDescriptorSetLayout() {
		//Creating the layout
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		if (mainSwapChainFormat.expired()) {
			throw std::runtime_error("Main SwapChain Format is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		VkFormat swapChainFormat = *mainSwapChainFormat.lock();
		//topLevelAccelerationStructure binding
		VkDescriptorSetLayoutBinding accStructureBinding;
		accStructureBinding.binding = 0;
		accStructureBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
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

		//Add Material bindings below! ---WIP---
		//Combining all the layouts
		std::array<VkDescriptorSetLayoutBinding, 4> bindings = { accStructureBinding, imageBinding,
		vertexLayoutBinding, indexLayoutBinding };
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
	void RayTracer::createRayTracerDescriptorPool() {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}
	//Create Descriptor Sets for a single model
	void RayTracer::createRayTracerDescriptorSets(VkBuffer& vertexBuffer, VkBuffer& indexBuffer) {
		//Allocate data for the descriptor sets
		//Creating the layout
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();
		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
		//Configure the sets and pass them to sets
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkDescriptorImageInfo imageInfo;
			imageInfo.imageLayout = {};
			imageInfo.imageView = rayTracerImageView;
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			
			VkWriteDescriptorSetAccelerationStructureKHR writeStuct;
			writeStuct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			writeStuct.pNext = NULL;
			writeStuct.accelerationStructureCount = 1;
			writeStuct.pAccelerationStructures = &topLevelAccelerationStructure;

			VkDescriptorBufferInfo vertexInfo;
			vertexInfo.buffer = vertexBuffer;
			vertexInfo.offset = 0;
			vertexInfo.range = VK_WHOLE_SIZE;

			VkDescriptorBufferInfo indexInfo;
			indexInfo.buffer = indexBuffer;
			indexInfo.offset = 0;
			indexInfo.range = VK_WHOLE_SIZE;

			//Assigning Descriptor infomation to bindings in layout
			std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
			
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pNext = &writeStuct;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[2].dstSet = descriptorSets[i];
			descriptorWrites[2].dstBinding = 2;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].pBufferInfo = &vertexInfo;

			descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[3].dstSet = descriptorSets[i];
			descriptorWrites[3].dstBinding = 3;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].pBufferInfo = &indexInfo;

			vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data()
				, 0, nullptr);
		}
	}
	//Call this in the resizing callback function to rebuild image on resize
	void RayTracer::updateRayTracerDescriptorSets() {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		//Relink output image in case of change in window size
		VkDescriptorImageInfo rayTraceImageDescriptorInfo;
		rayTraceImageDescriptorInfo.sampler = VK_NULL_HANDLE;
		rayTraceImageDescriptorInfo.imageView = rayTracerImageView;
		rayTraceImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkWriteDescriptorSet writeSet;
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.dstSet = descriptorSets[0];
		writeSet.dstBinding = 1;
		writeSet.dstArrayElement = 0;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeSet.descriptorCount = 1;
		writeSet.pImageInfo = &rayTraceImageDescriptorInfo;
		vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
	}
	void RayTracer::createTopLevelAccelerationStructure() {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		//Get the address to pass to the bl instance
		VkAccelerationStructureDeviceAddressInfoKHR bottomLevelAccelerationStructureDeviceAddressInfo;
		bottomLevelAccelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		bottomLevelAccelerationStructureDeviceAddressInfo.accelerationStructure = bottomLevelAccelerationStructure;
		bottomLevelAccelerationStructureDeviceAddressInfo.pNext = NULL;
		VkDeviceAddress bottomLevelAccelerationStructureAddress;
		bottomLevelAccelerationStructureAddress = pvkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &bottomLevelAccelerationStructureDeviceAddressInfo);
		VkAccelerationStructureInstanceKHR bottomLevelAccelerationStructureInstance;
		//Initialize an indenity matrix
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				bottomLevelAccelerationStructureInstance.transform.matrix[i][j] = 0.0;
				if (i == j) bottomLevelAccelerationStructureInstance.transform.matrix[i][j] = 1.0;
			}
		}
		bottomLevelAccelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
		bottomLevelAccelerationStructureInstance.accelerationStructureReference = bottomLevelAccelerationStructureAddress;
		bottomLevelAccelerationStructureInstance.instanceCustomIndex = 0;
		bottomLevelAccelerationStructureInstance.mask = 0xFF;
		bottomLevelAccelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		//Get the queueFamiies
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };
		uint32_t simultaniousIndex = findSimultaniousGraphicsAndPresentIndex(physicalDevice);
		VkBufferCreateInfo bottomLevelGeometryStructureReference;
		bottomLevelGeometryStructureReference.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bottomLevelGeometryStructureReference.flags = 0;
		bottomLevelGeometryStructureReference.size = sizeof(VkAccelerationStructureInstanceKHR);
		bottomLevelGeometryStructureReference.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		bottomLevelGeometryStructureReference.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bottomLevelGeometryStructureReference.queueFamilyIndexCount = 1;
		bottomLevelGeometryStructureReference.pQueueFamilyIndices = &simultaniousIndex;
		bottomLevelGeometryStructureReference.pNext = NULL;
		VkBuffer bottomLevelGeometryInstanceBuffer;
		if (vkCreateBuffer(logicalDevice, &bottomLevelGeometryStructureReference, nullptr, &bottomLevelGeometryInstanceBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Buffer for building Bottom Level Acc. Struct. instance cannot be made!");
		}
#ifndef NDEBUG
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(bottomLevelGeometryInstanceBuffer)
			, "Bottom Level Geo. Instance Buffer");
#endif
		//Get memory requirements for instance
		VkMemoryRequirements bottomLevelGeometryInstanceMemReq;
		vkGetBufferMemoryRequirements(logicalDevice, bottomLevelGeometryInstanceBuffer, &bottomLevelGeometryInstanceMemReq);
		//Check to see what memory our graphics card has for the buffer
		uint32_t bottomLevelGeometryInstanceMemoryTypeIndex = findBufferMemoryTypeIndex(logicalDevice, physicalDevice,
			bottomLevelGeometryInstanceBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		VkMemoryAllocateFlagsInfo defaultFlags = getDefaultAllocationFlags();
		VkMemoryAllocateInfo bottomLevelGeometryInstanceMemoryAllocateInfo;
		bottomLevelGeometryInstanceMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		bottomLevelGeometryInstanceMemoryAllocateInfo.pNext = &defaultFlags;
		bottomLevelGeometryInstanceMemoryAllocateInfo.allocationSize = bottomLevelGeometryInstanceMemReq.size;
		bottomLevelGeometryInstanceMemoryAllocateInfo.memoryTypeIndex = bottomLevelGeometryInstanceMemoryTypeIndex;

		VkDeviceMemory bottomLevelGeometryInstanceDeviceMemoryHandle;
		if (vkAllocateMemory(logicalDevice, &bottomLevelGeometryInstanceMemoryAllocateInfo
			, nullptr, &bottomLevelGeometryInstanceDeviceMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Can't allocate memory for device");
		}
		if (vkBindBufferMemory(logicalDevice,bottomLevelGeometryInstanceBuffer,bottomLevelGeometryInstanceDeviceMemoryHandle,0) != VK_SUCCESS) {
			throw std::runtime_error("Can't bind memory for device");
		}
#ifndef NDEBUG
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
			, reinterpret_cast<uint64_t>(bottomLevelGeometryInstanceDeviceMemoryHandle)
			, "Bottom Level Geometery Instance Device Memory");
#endif
		//Host Device blGeoInstance buffer creation
		//We trying to copy from GPU to CPU
		void* hostbottomLevelGeometryInstanceMemoryBuffer;
		VkResult result =
			vkMapMemory(logicalDevice, bottomLevelGeometryInstanceDeviceMemoryHandle,
				0, sizeof(VkAccelerationStructureInstanceKHR), 0,
				&hostbottomLevelGeometryInstanceMemoryBuffer);

		memcpy(hostbottomLevelGeometryInstanceMemoryBuffer,
			&bottomLevelAccelerationStructureInstance,
			sizeof(VkAccelerationStructureInstanceKHR));

		if (result != VK_SUCCESS) {
			throw std::runtime_error("Can't map memory");
		}

		vkUnmapMemory(logicalDevice, bottomLevelGeometryInstanceDeviceMemoryHandle);
		//We have the instance data, so now we are going to get the geometry data to pass into topLevelAccelerationStructure
		VkBufferDeviceAddressInfo bottomLevelGeometryInstanceDeviceAddressInfo;
		bottomLevelGeometryInstanceDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bottomLevelGeometryInstanceDeviceAddressInfo.buffer = bottomLevelGeometryInstanceBuffer;
		bottomLevelGeometryInstanceDeviceAddressInfo.pNext = NULL;
		bottomLevelAccelerationStructureAddress = pvkGetBufferDeviceAddressKHR(logicalDevice, &bottomLevelGeometryInstanceDeviceAddressInfo);
		//Geo data setup for top level 
		VkAccelerationStructureGeometryDataKHR topLevelGeometryData;
		topLevelGeometryData.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		topLevelGeometryData.instances.arrayOfPointers = VK_FALSE;
		topLevelGeometryData.instances.data.deviceAddress = bottomLevelAccelerationStructureAddress;
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(topLevelAccelerationStructureBuffer)
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_BUFFER
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
		setDebugObjectName(logicalDevice, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY
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
		if (mainCommandPool.expired()) {
			throw std::runtime_error("Commaned pool has expired or is null!\n");
		}
		VkCommandPool commandPool = *mainCommandPool.lock();
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
		if (mainGraphicsQueue.expired()) {
			throw std::runtime_error("Graphics Queue expired\n");
		}
		VkQueue graphicsQueue = *mainGraphicsQueue.lock();
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
		vkDestroyBuffer(logicalDevice, bottomLevelGeometryInstanceBuffer, NULL);
		vkDestroyBuffer(logicalDevice, topLevelAccelerationStructureScratchBuffer, NULL);
		vkFreeMemory(logicalDevice,topLevelAccelerationStructureDeviceScratchMemoryHandle, NULL);
		vkFreeMemory(logicalDevice, bottomLevelGeometryInstanceDeviceMemoryHandle, NULL);
		//Free up our one time command buffer submission
		vkDestroyFence(logicalDevice, topLevelFence, NULL);
		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}
	void RayTracer::createRayTracingPipeline() {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		if (mainDescSetLayout.expired()) {
			throw std::runtime_error("Main Desc. Set Layout Device is expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		VkDescriptorSetLayout descSetLayout = *mainDescSetLayout.lock();
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
		pushConstant.size = sizeof(PushConstantRay);
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
		if(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &rayPipelineLayout) != VK_SUCCESS){
			throw std::runtime_error("Failed to make the rt pipeline layout!");
		}
		rayTracerPipeline.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayTracerPipeline.pNext = NULL;
		rayTracerPipeline.flags = 0;
		rayTracerPipeline.stageCount = stages.size();
		rayTracerPipeline.pStages = stages.data();
		rayTracerPipeline.groupCount = raytracingShaderGroups.size();
		rayTracerPipeline.pGroups = raytracingShaderGroups.data();
		rayTracerPipeline.layout = rayPipelineLayout;
		rayTracerPipeline.maxPipelineRayRecursionDepth = 1;
		rayTracerPipeline.basePipelineHandle = VK_NULL_HANDLE;
		rayTracerPipeline.basePipelineIndex = 0;
		rayTracerPipeline.pDynamicState = NULL;
		rayTracerPipeline.pLibraryInfo = NULL;
		rayTracerPipeline.pLibraryInterface = NULL;

		if (pvkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracerPipeline, NULL, &raytracingPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to make the rt pipeline!");
		}
		//Get rid of the modules since we don't need them now
		for (auto& s : stages){
			vkDestroyShaderModule(logicalDevice, s.module, nullptr);
		}
	}
	void RayTracer::rayTrace(VkCommandBuffer& cmdBuf,std::vector<void *>& uniBufferMMap, glm::vec4 clearColor) {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("Main Logical Device is expired / null!\n");
		}
		if (mainPhysicalDevice.expired()) {
			throw std::runtime_error("Main Physical Device is expired / null!\n");
		}
		if (rayTracerFences.expired()) {
			throw std::runtime_error("In flight fences are expired / null!\n");
		}
		if (rayTracerSwapchain.expired()) {
			throw std::runtime_error("swapchain reference has expired / null!\n");
		}
		if (rayTracerImageAvailableSemaphores.expired()) {
			throw std::runtime_error("available semaphores ref has expired / null!\n");
		}
		if (rayTracerFinishedSemaphores.expired()) {
			throw std::runtime_error("finished semaphores ref has expired / null!\n");
		}
		if (rayTracerPresentQueue.expired()) {
			throw std::runtime_error("Present Queue has expired / null!\n");
		}
		if (mainLightSource.expired()) {
			throw std::runtime_error("main light source has expired / null!\n");
		}
		if (mainGraphicsQueue.expired()) {
			throw std::runtime_error("Graphics Queue has expired\n");
		}
		if (rayTracerSwapchainImages.expired()) {
			throw std::runtime_error("swachain images has expired / null!\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		VkPhysicalDevice physicalDevice = *mainPhysicalDevice.lock();
		std::vector<VkFence> fences = *rayTracerFences.lock();
		uint32_t currentFrame = *currentFrameRef.lock();
		VkSwapchainKHR swapChain = *rayTracerSwapchain.lock();
		std::vector<VkDescriptorSet> mainDescSetVector = *mainDescSets.lock();
		std::vector <VkSemaphore> availableSemaphores = *rayTracerImageAvailableSemaphores.lock();
		std::vector <VkSemaphore> finishedSemaphores = *rayTracerFinishedSemaphores.lock();
		VkQueue presentQueue = *rayTracerPresentQueue.lock();
		VkQueue graphicsQueue = *mainGraphicsQueue.lock();
		LightSource lightSource = *mainLightSource.lock();
		std::vector<VkImage> swapchainImages = *rayTracerSwapchainImages.lock();
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
		//Copied From draw frame -- update unform buffers
		//Using chrono to keep track of time independent of framerate
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		UniformBufferObject ubo{};
		//TODO: INVERT THIS MATRIX!
		//We create an indentity matrix and rotate based on the time
		ubo.model = glm::mat4(0.25f);
		ubo.model[3][3] = 1.0f;
		ubo.model[3][2] = -1.0f;
		ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.model = glm::rotate(ubo.model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.model = glm::rotate(ubo.model, time * glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//Create a camera matrix at pos 2,2,2 look at 0 0 0, with up being Z
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.view = glm::rotate(ubo.view, time * glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		//Create a perspective based projection matrix for our camera
		ubo.proj = glm::perspective(glm::radians(45.0f), widthRef / (float)heightRef, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1; //Y-coord for clip coords is inverted. This fixes that (GLM designed for openGL)
		
		ubo.proj = glm::inverse(ubo.proj);
		ubo.view = glm::inverse(ubo.view);
		ubo.model = glm::inverse(ubo.model);
							  
		//ubo.colorAdd = glm::vec4(abs(cos(time)), abs(sin(time)), abs(tan(time)), 1);
		memcpy(uniBufferMMap[currentFrame], &ubo, sizeof(ubo));
		vkResetCommandBuffer(cmdBuf, 0);

		//Building pipeline and layout
		pushConstantRay.clearColor = clearColor;
		pushConstantRay.lightPos = lightSource.pos;
		pushConstantRay.lightIntensity = lightSource.intensity;
		pushConstantRay.lightType = lightSource.type;
		//Desc sets to bind
		std::vector<VkDescriptorSet> descSets{ descriptorSets.at(currentFrame), (mainDescSetVector.at(currentFrame)) };
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
		
		//TODO: More barriers and transitions need to be made it seems. Trying to figure out why
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, raytracingPipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
			rayPipelineLayout, 0, (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
		vkCmdPushConstants(cmdBuf, rayPipelineLayout
			, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
			0, sizeof(PushConstantRay), &pushConstantRay);
		pvkCmdTraceRaysKHR(cmdBuf, &rayGenerationRegion,&rayMissRegion, &rayHitRegion, &rayCallRegion
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
		if (mainSurface.expired()) {
			throw std::runtime_error("main Surface is null or expired\n");
		}
		VkSurfaceKHR surface = *mainSurface.lock();
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
		if (mainSurface.expired()) {
			throw std::runtime_error("main Surface is null or expired\n");
		}
		VkSurfaceKHR surface = *mainSurface.lock();

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
	//Module to handle shader programs compiled into the vulkan byte code
	VkShaderModule RayTracer::createShaderModule(const std::vector<char>& code) {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("main logical device is null or expired\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();
		
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //The bytecode pointer is a uint32 and not a char, hence the cast
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule; //A thin wrapper around the byte code. Compliation + linking occurs at graphics pipeline time
	}
	void RayTracer::cleanup() {
		if (mainLogicalDevice.expired()) {
			throw std::runtime_error("main Logical Device is null or expired\n");
		}
		VkDevice logicalDevice = *mainLogicalDevice.lock();

		//Acceleration Structures
		pvkDestroyAccelerationStructureKHR(logicalDevice, topLevelAccelerationStructure, nullptr);
		pvkDestroyAccelerationStructureKHR(logicalDevice, bottomLevelAccelerationStructure, nullptr);
		vkDestroyBuffer(logicalDevice, topLevelAccelerationStructureBuffer, nullptr);
		vkFreeMemory(logicalDevice,topLevelAccelerationStructureDeviceMemory , nullptr);
		vkDestroyBuffer(logicalDevice, bottomLevelAccelerationStructureBuffer, nullptr);
		vkFreeMemory(logicalDevice, bottomLevelAccelerationStructureDeviceMemory, nullptr);
		//Ray Tracing Pipeline
		vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
		vkDestroyPipeline(logicalDevice, raytracingPipeline, nullptr);
		vkDestroyPipelineLayout(logicalDevice, rayPipelineLayout, nullptr);
		//Shader Binding Table and Ray Trace Image
		vkDestroyBuffer(logicalDevice, shaderBindingTableBuffer, nullptr);
		vkFreeMemory(logicalDevice, shaderBindingTableDeviceMemory, nullptr);
		vkDestroyImageView(logicalDevice, rayTracerImageView, nullptr);
		vkDestroyImage(logicalDevice, rayTracerImage, nullptr);
		vkFreeMemory(logicalDevice, rayTracerImageDeviceMemory, nullptr);

	}
