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
	void RayTracer::setupRayTracer(VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t nOfVerts) {
		initRayTracing();
		modelToBLAS(vertexBuffer, indexBuffer, nOfVerts);
		createTopLevelAS();
		createRTImageAndImageView();
		createRtDescriptorSetLayout();
		createRtDescriptorPool();
		createRtDescriptorSets();
		createRayTracingPipeline();
		createShaderBindingTable();
	}
	//Check to see if our GPU supports raytracing
	void RayTracer::initRayTracing()
	{
		//Setup function pointers for ray trace functions
		pvkGetBufferDeviceAddressKHR =
			(PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkGetBufferDeviceAddressKHR");
		pvkCreateRayTracingPipelinesKHR =
			(PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkCreateRayTracingPipelinesKHR");
		pvkGetAccelerationStructureBuildSizesKHR =
			(PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
		pvkCreateAccelerationStructureKHR =
			(PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkCreateAccelerationStructureKHR");
		pvkDestroyAccelerationStructureKHR =
			(PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkDestroyAccelerationStructureKHR");
		pvkGetAccelerationStructureDeviceAddressKHR =
			(PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkGetAccelerationStructureDeviceAddressKHR");
		pvkCmdBuildAccelerationStructuresKHR =
			(PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkCmdBuildAccelerationStructuresKHR");
		pvkGetRayTracingShaderGroupHandlesKHR =
			(PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(
				*mainLogicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");
		pvkCmdTraceRaysKHR =
			(PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(*mainLogicalDevice,
				"vkCmdTraceRaysKHR");

		// Requesting ray tracing properties
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		prop2.pNext = &rayTracingProperties;
		vkGetPhysicalDeviceProperties2(*mainPhysicalDevice, &prop2);
	}//BUG: I think the BLAS scratch buffers are setup wrong. there is a bASSBuffer and buffer handle. Investigate
	void RayTracer::modelToBLAS(VkBuffer &vertexBuffer, VkBuffer&indexBuffer, uint32_t nOfVerts) {

		VkBufferDeviceAddressInfo vInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		vInfo.pNext = NULL;
		vInfo.buffer = vertexBuffer;
		VkDeviceAddress vertexAddress = pvkGetBufferDeviceAddressKHR(*mainLogicalDevice, &vInfo);
		vInfo.buffer = indexBuffer;
		VkDeviceAddress indexAddress = pvkGetBufferDeviceAddressKHR(*mainLogicalDevice, &vInfo);
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
		pvkGetAccelerationStructureBuildSizesKHR(*mainLogicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&bottomLevelAccelerationBuildGeometryInfoKHR,
			bottomLevelMaxPrimitiveCountList.data(),
			&bottomLevelAccelerationBuildSizesInfo);
		//Create buffer to store acceleration structure
		QueueFamilyIndices indices = findQueueFamilies(*mainPhysicalDevice);
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
			if (vkCreateBuffer(*mainLogicalDevice, &bottomLevelAccelerationStructureBufferCreateInfo, nullptr, &bASSBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create buffer for bASS");
			}
		//Get the memory requirments for the accleartion struct
		VkMemoryRequirements blASMemoryRequirements;
		vkGetBufferMemoryRequirements(*mainLogicalDevice, bASSBuffer, &blASMemoryRequirements);
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(*mainPhysicalDevice, &memProperties);
		//Look to see if our graphics card and our blAS has a local bit for our buffer
		uint32_t bottomLevelAccelerationStructureMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {

			if ((blASMemoryRequirements.memoryTypeBits &
				(1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

				bottomLevelAccelerationStructureMemoryTypeIndex = x;
				break;
			}
		}
		VkMemoryAllocateFlagsInfo defaultFlagsBLAS = getDefaultAllocationFlags();
		//We are now allocating memory to the blAS
		VkMemoryAllocateInfo blASmemAllocateInfo;
		blASmemAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		blASmemAllocateInfo.pNext = NULL;//&defaultFlagsBLAS;
		blASmemAllocateInfo.allocationSize = blASMemoryRequirements.size;
		blASmemAllocateInfo.memoryTypeIndex = bottomLevelAccelerationStructureMemoryTypeIndex;
		VkDeviceMemory blASDeviceMemoryHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(*mainLogicalDevice, &blASmemAllocateInfo, nullptr, &blASDeviceMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt allocate memory for buffer!");
		}
		//Bind the allocated memory
		if (vkBindBufferMemory(*mainLogicalDevice, bASSBuffer,blASDeviceMemoryHandle,0) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt bind memory to buffer!");
		}
		//Create acc structure
		VkAccelerationStructureCreateInfoKHR accInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		accInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accInfo.createFlags = 0;
		accInfo.buffer = bASSBuffer;
		accInfo.offset = 0;
		accInfo.size = bottomLevelAccelerationBuildSizesInfo.accelerationStructureSize;
		accInfo.deviceAddress = 0;
		accInfo.pNext = NULL;
		//VkAccelerationStructureKHR blAShandle = VK_NULL_HANDLE;
		if (pvkCreateAccelerationStructureKHR(*mainLogicalDevice, &accInfo, nullptr, &blAShandle) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt create bottom acceleration structure");
		}
		//Building Bottom level acceleartion structure
		VkAccelerationStructureDeviceAddressInfoKHR blASdeviceAddressInfo;
		blASdeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		blASdeviceAddressInfo.pNext = NULL;
		blASdeviceAddressInfo.accelerationStructure = blAShandle;
		VkDeviceAddress blAddress;
		blAddress = pvkGetAccelerationStructureDeviceAddressKHR(*mainLogicalDevice, &blASdeviceAddressInfo);
		VkBufferCreateInfo blASScratchBufferCreateInfo;
		blASScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		blASScratchBufferCreateInfo.flags = 0;
		blASScratchBufferCreateInfo.size = bottomLevelAccelerationBuildSizesInfo.buildScratchSize;
		blASScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		blASScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		blASScratchBufferCreateInfo.queueFamilyIndexCount = 1;
		blASScratchBufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		blASScratchBufferCreateInfo.pNext = NULL;

		VkBuffer blASScratchBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(*mainLogicalDevice, &blASScratchBufferCreateInfo, NULL, &blASScratchBufferHandle) != VK_SUCCESS) {
			throw std::runtime_error("Buffer for building blAS cannot be made!");
		}
		VkMemoryRequirements blASScratchMemoryReq;
		vkGetBufferMemoryRequirements(*mainLogicalDevice, blASScratchBufferHandle, &blASScratchMemoryReq);
		//Check to see what memory our graphics card has for the buffer
		uint32_t bottomLevelAccelerationStructureScratchMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {

			if ((blASScratchMemoryReq
				.memoryTypeBits &
				(1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

				bottomLevelAccelerationStructureScratchMemoryTypeIndex = x;
				break;
			}
		}
		
		VkMemoryAllocateInfo blASScratchMemoryAllocateInfo;
		blASScratchMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		blASScratchMemoryAllocateInfo.pNext = &defaultFlagsBLAS;
		blASScratchMemoryAllocateInfo.allocationSize = blASMemoryRequirements.size;
		blASScratchMemoryAllocateInfo.memoryTypeIndex = bottomLevelAccelerationStructureScratchMemoryTypeIndex;
		VkDeviceMemory blASDeviceScratchMemoryHandle;
		if (vkAllocateMemory(*mainLogicalDevice, &blASScratchMemoryAllocateInfo, nullptr, &blASDeviceMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Cannot allocate scratch memory!");
		}
		if (vkBindBufferMemory(*mainLogicalDevice, blASScratchBufferHandle, blASDeviceMemoryHandle, 0) != VK_SUCCESS) {
			throw std::runtime_error("Cannot bind scratch memory!");
		}
		VkBufferDeviceAddressInfo blASScratchBufferDeviceAddressInfo;
		blASScratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		blASScratchBufferDeviceAddressInfo.pNext = NULL;
		blASScratchBufferDeviceAddressInfo.buffer = blASScratchBufferHandle;
		VkDeviceAddress blASScratchBuffeDeviceAddress = pvkGetBufferDeviceAddressKHR(*mainLogicalDevice, &blASScratchBufferDeviceAddressInfo);
		//Building the actual geometry
		//Set where we want the data to be saved to
		bottomLevelAccelerationBuildGeometryInfoKHR.pNext = NULL;
		bottomLevelAccelerationBuildGeometryInfoKHR.dstAccelerationStructure = blAShandle;
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
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = *mainCommandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.pNext = NULL;
		//Memory transfer is executed using command buffers
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(*mainLogicalDevice, &allocInfo, &commandBuffer);
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
		VkFenceCreateInfo blBuildFenceInfo;
		blBuildFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		blBuildFenceInfo.pNext = NULL;
		blBuildFenceInfo.flags = 0;
		VkFence bottomLevelFence;
		if (vkCreateFence(*mainLogicalDevice, &blBuildFenceInfo, nullptr, &bottomLevelFence) != VK_SUCCESS) {
			throw std::runtime_error("Fence failed to be created!");
		}
		if (vkQueueSubmit(*mainGraphicsQueue, 1, &submitInfo, bottomLevelFence) != VK_SUCCESS) {
			throw std::runtime_error("Can't submit queue");
		}
		VkResult r = vkWaitForFences(*mainLogicalDevice, 1, &bottomLevelFence, true, UINT32_MAX);
		if (r != VK_SUCCESS && r != VK_TIMEOUT) {
			throw std::runtime_error("Failed to wait for fences");
		}
		vkDestroyFence(*mainLogicalDevice, bottomLevelFence, NULL);
		vkFreeCommandBuffers(*mainLogicalDevice, *mainCommandPool, 1, &commandBuffer);
	}
	void RayTracer::createShaderBindingTable() {
		// So I can't query the GPU since I dont have the device feature for it. Will use physical device queryig instead
		//Maps which shaders we should call for different entrypoints
		//Setting up buffer offsets to store the shader handles in
		//32bit for RG, 16 for miss,padd out another 16, and finally 16 for hit
		uint32_t missCount = 1;
		uint32_t hitCount = 1;
		auto handleCount = 1 + missCount + hitCount;
		//Can't guarintee that alignment matches group size or handle so we should round up to nearest bit
		uint32_t handleSize = rayTracingProperties.shaderGroupHandleSize;
		uint32_t handleAlign = rayTracingProperties.shaderGroupHandleAlignment;
		uint32_t baseAlign = rayTracingProperties.shaderGroupBaseAlignment;

		//Aligns handles using handle and base alignments, offset by the size
		//See this page for formula explaination: https://en.wikipedia.org/wiki/Data_structure_alignment
		uint32_t handleSizeAligned = (handleSize + (handleAlign-1)) & ~(handleAlign - 1);
		rayGenRegion.stride = (handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
		rayGenRegion.size = rayGenRegion.stride;
		rayMissRegion.stride = handleSizeAligned;
		rayMissRegion.size = (missCount * handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
		rayHitRegion.stride = handleSizeAligned;
		rayHitRegion.size = (hitCount * handleSizeAligned + (baseAlign - 1)) & ~(baseAlign - 1);
		//Vector to store the handles to each shader
		uint32_t dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		auto result = pvkGetRayTracingShaderGroupHandlesKHR(*mainLogicalDevice, raytracingPipeline, 0, handleCount, dataSize, handles.data());
		//Im breaking my consistentcy rules because this way is so much better and I want to demonstrate the better way to do this valdiatioN!
		assert(result == VK_SUCCESS);
		
		//Allocate buffer for SBT
		uint32_t queueFamilyIndex = findSimultGraphicsAndPresentIndex(*mainPhysicalDevice);
		VkDeviceSize sbtSize = rayGenRegion.size + rayMissRegion.size + rayHitRegion.size;
		//create and bind buffer for SBT
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sbtSize; //size of our buffer
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR; //what kind of buffer is this?
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 1;
		bufferInfo.pQueueFamilyIndices = &queueFamilyIndex;
		if (vkCreateBuffer(*mainLogicalDevice, &bufferInfo, nullptr, &sbtBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create Shader Binding Table buffer!");
		}
		//Query the memory requirements to make sure we have enough space to allocate for the vertex buffer
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(*mainLogicalDevice, sbtBuffer, &memRequirements);
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(*mainPhysicalDevice, &memProperties);
		//Check to see what memory our graphics card has for the buffer
		uint32_t sbtMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {

			if ((memRequirements
				.memoryTypeBits &
				(1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

				sbtMemoryTypeIndex = x;
				break;
			}
		}
		//Allocate memory for the buffer
		VkMemoryAllocateFlagsInfo flags = getDefaultAllocationFlags();
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = &flags;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = sbtMemoryTypeIndex;
		VkDeviceMemory sbtDeviceMemHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(*mainLogicalDevice, &allocInfo, nullptr, &sbtDeviceMemHandle) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}
		//Bind the allocated memory to the sbt buffer
		if (vkBindBufferMemory(*mainLogicalDevice, sbtBuffer, sbtDeviceMemHandle, 0)) {
			throw std::runtime_error("Failed to allocate memory to sbt buffer");
		}
		//Storing device addresses for the shader groups
		VkBufferDeviceAddressInfo info;
		info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		info.pNext = nullptr;
		info.buffer = sbtBuffer;
		VkDeviceAddress sbtAddress = pvkGetBufferDeviceAddressKHR(*mainLogicalDevice, &info);
		rayGenRegion.deviceAddress = sbtAddress;
		rayMissRegion.deviceAddress = sbtAddress + rayGenRegion.size;
		rayHitRegion.deviceAddress = sbtAddress + rayGenRegion.size + rayMissRegion.size;
		//uint32_t handleSize = handles.size();
		auto getHandle = [&](int i) { return handles.data() + (uint32_t)i * handleSize; };
		//Map the SBT buffer and write in the handles
		void* hostSBTMemoryBuffer;
		if (vkMapMemory(*mainLogicalDevice, sbtDeviceMemHandle, 0, sbtSize, 0, &hostSBTMemoryBuffer)
			!= VK_SUCCESS) {
			throw std::runtime_error("Failed to map memory sbt to buffer");
		}
		auto* pSBTBuffer = reinterpret_cast<uint8_t*>(hostSBTMemoryBuffer);
		uint8_t* pData = nullptr;
		uint32_t handleIdx = 0;
		//Copy data over for Raygen
		pData = pSBTBuffer;
		memcpy(pData, getHandle(handleIdx++), handleSize);
		//Point base of pointer to miss shader(s)
		pData = pSBTBuffer + rayGenRegion.size;
		//for loop to copy multiple miss shaders in case we add more
		for (uint32_t c = 0; c < missCount; c++) {
			memcpy(pData, getHandle(handleIdx++), handleSize);
			pData += rayMissRegion.stride;
		}
		//Point base of pointer to miss shader(s)
		pData = pSBTBuffer + rayGenRegion.size +rayMissRegion.size;
		//for loop to copy multiple miss shaders in case we add more
		for (uint32_t c = 0; c < hitCount; c++) {
			memcpy(pData, getHandle(handleIdx++), handleSize);
			pData += rayHitRegion.stride;
		}
		//CLeanup resources
		vkUnmapMemory(*mainLogicalDevice, sbtDeviceMemHandle);
		//vkDestroyBuffer(*mainLogicalDevice, sbtBuffer, nullptr);
		//vkFreeMemory(*mainLogicalDevice, sbtDeviceMemHandle, nullptr);
	}
	
	void RayTracer::createRTImageAndImageView() {
		QueueFamilyIndices indices = findQueueFamilies(*mainPhysicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };
		//Settings for the image
		VkImageCreateInfo imageInfo{};
		imageInfo.pNext = NULL;
		imageInfo.queueFamilyIndexCount = 1;
		imageInfo.pQueueFamilyIndices = queueFamilyIndices;
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(widthRef);
		imageInfo.extent.height = static_cast<uint32_t>(heightRef);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		//We need same format for the texels as the pixels in the buffer
		imageInfo.format = *mainSwapChainFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //discard textuals first transition
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT 
			| VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional

		if (vkCreateImage(*mainLogicalDevice, &imageInfo, nullptr, &rtImage) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}
		//allocating memory to the image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(*mainLogicalDevice, rtImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(*mainPhysicalDevice, &memProperties);
		//Check to see what memory our graphics card has for the buffer
		uint32_t rayTraceImageMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {
			if ((memRequirements.memoryTypeBits & (1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

				rayTraceImageMemoryTypeIndex = x;
				break;
			}
		}
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = rayTraceImageMemoryTypeIndex;

		if (vkAllocateMemory(*mainLogicalDevice, &allocInfo, nullptr, &rtImageDeviceMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}
		//Bind the image to the allocated memory
		vkBindImageMemory(*mainLogicalDevice, rtImage, rtImageDeviceMemory, 0);
		//rtImage View
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = NULL;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = rtImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = *mainSwapChainFormat;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY };
		imageViewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		if (vkCreateImageView(*mainLogicalDevice, &imageViewCreateInfo, nullptr, &rtImageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create ray tracing image view!");
		}
	}
	void RayTracer::createRtDescriptorSetLayout() {
		//Creating the layout
		//TLAS binding
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

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { accStructureBinding, imageBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.pNext = NULL;
		layoutInfo.flags = 0;
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data(); //Array of bindings
		if (vkCreateDescriptorSetLayout(*mainLogicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}
	void RayTracer::createRtDescriptorPool() {
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
		if (vkCreateDescriptorPool(*mainLogicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}
	void RayTracer::createRtDescriptorSets() {
		//Allocate data for the descriptor sets
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();
		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (vkAllocateDescriptorSets(*mainLogicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
		//Configure the sets and pass them to sets
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VkWriteDescriptorSetAccelerationStructureKHR descASInfo;
			descASInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			descASInfo.accelerationStructureCount = 1;
			descASInfo.pAccelerationStructures = &tlAShandle;

			VkDescriptorImageInfo imageInfo;
			imageInfo.imageLayout = {};
			imageInfo.imageView = rtImageView;
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
			VkWriteDescriptorSetAccelerationStructureKHR writeStuct;
			writeStuct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			writeStuct.pNext = NULL;
			writeStuct.accelerationStructureCount = 1;
			writeStuct.pAccelerationStructures = &tlAShandle;

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

			vkUpdateDescriptorSets(*mainLogicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data()
				, 0, nullptr);
		}
	}
	//Call this in the resizing callback function to rebuild image on resize
	void RayTracer::updateRTDescriptorSets() {
		//Relink output image in case of change in window size
		VkDescriptorImageInfo rayTraceImageDescriptorInfo;
		rayTraceImageDescriptorInfo.sampler = VK_NULL_HANDLE;
		rayTraceImageDescriptorInfo.imageView = rtImageView;
		rayTraceImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkWriteDescriptorSet writeSet;
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.dstSet = descriptorSets[0];
		writeSet.dstBinding = 1;
		writeSet.dstArrayElement = 0;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeSet.descriptorCount = 1;
		writeSet.pImageInfo = &rayTraceImageDescriptorInfo;
		vkUpdateDescriptorSets(*mainLogicalDevice, 1, &writeSet, 0, nullptr);
	}
	void RayTracer::createTopLevelAS() {
		//Get the address to pass to the bl instance
		VkAccelerationStructureDeviceAddressInfoKHR blASdeviceAddressInfo;
		blASdeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		blASdeviceAddressInfo.accelerationStructure = blAShandle;
		blASdeviceAddressInfo.pNext = NULL;
		VkDeviceAddress blAddress;
		blAddress = pvkGetAccelerationStructureDeviceAddressKHR(*mainLogicalDevice, &blASdeviceAddressInfo);
		VkAccelerationStructureInstanceKHR blACSInstance;
		//Initialize an indenity matrix
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				blACSInstance.transform.matrix[i][j] = 0.0;
				if (i == j) blACSInstance.transform.matrix[i][j] = 1.0;
			}
		}
		blACSInstance.instanceShaderBindingTableRecordOffset = 0;
		blACSInstance.accelerationStructureReference = blAddress;
		blACSInstance.instanceCustomIndex = 0;
		blACSInstance.mask = 0xFF;
		blACSInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		//Get the queueFamiies
		QueueFamilyIndices indices = findQueueFamilies(*mainPhysicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),indices.presentFamily.value() };
		uint32_t simultIndex = findSimultGraphicsAndPresentIndex(*mainPhysicalDevice);
		VkBufferCreateInfo blGeoStructureReference;
		blGeoStructureReference.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		blGeoStructureReference.flags = 0;
		blGeoStructureReference.size = sizeof(VkAccelerationStructureInstanceKHR);
		blGeoStructureReference.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		blGeoStructureReference.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		blGeoStructureReference.queueFamilyIndexCount = 1;
		blGeoStructureReference.pQueueFamilyIndices = &simultIndex;
		blGeoStructureReference.pNext = NULL;
		VkBuffer blGeoInstanceBuffer;
		if (vkCreateBuffer(*mainLogicalDevice, &blGeoStructureReference, nullptr, &blGeoInstanceBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Buffer for building blAS instance cannot be made!");
		}
		//Get memory requirements for instance
		VkMemoryRequirements blGeoInstanceMemReq;
		vkGetBufferMemoryRequirements(*mainLogicalDevice, blGeoInstanceBuffer, &blGeoInstanceMemReq);
		//Get memory requirements for hardware
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(*mainPhysicalDevice, &memProperties);
		//Check to see what memory our graphics card has for the buffer
		uint32_t blGeoInstanceMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {

			if ((blGeoInstanceMemReq
				.memoryTypeBits &
				(1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

				blGeoInstanceMemoryTypeIndex = x;
				break;
			}
		}
		VkMemoryAllocateFlagsInfo defaultFlags = getDefaultAllocationFlags();
		VkMemoryAllocateInfo blGeometryInstanceMemoryAllocateInfo;
		blGeometryInstanceMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		blGeometryInstanceMemoryAllocateInfo.pNext = &defaultFlags;
		blGeometryInstanceMemoryAllocateInfo.allocationSize = blGeoInstanceMemReq.size;
		blGeometryInstanceMemoryAllocateInfo.memoryTypeIndex = blGeoInstanceMemoryTypeIndex;

		VkDeviceMemory bottomLevelGeometryInstanceDeviceMemoryHandle;
		if (vkAllocateMemory(*mainLogicalDevice, &blGeometryInstanceMemoryAllocateInfo
			, nullptr, &bottomLevelGeometryInstanceDeviceMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Can't allocate memory for device");
		}
		if (vkBindBufferMemory(*mainLogicalDevice,blGeoInstanceBuffer,bottomLevelGeometryInstanceDeviceMemoryHandle,0) != VK_SUCCESS) {
			throw std::runtime_error("Can't bind memory for device");
		}
		//Host Device blGeoInstance buffer creation
		//We trying to copy from GPU to CPU
		void* hostbottomLevelGeometryInstanceMemoryBuffer;
		VkResult result =
			vkMapMemory(*mainLogicalDevice, bottomLevelGeometryInstanceDeviceMemoryHandle,
				0, sizeof(VkAccelerationStructureInstanceKHR), 0,
				&hostbottomLevelGeometryInstanceMemoryBuffer);

		memcpy(hostbottomLevelGeometryInstanceMemoryBuffer,
			&blACSInstance,
			sizeof(VkAccelerationStructureInstanceKHR));

		if (result != VK_SUCCESS) {
			throw std::runtime_error("Can't map memory");
		}

		vkUnmapMemory(*mainLogicalDevice, bottomLevelGeometryInstanceDeviceMemoryHandle);
		//We have the instance data, so now we are going to get the geometry data to pass into tlAS
		VkBufferDeviceAddressInfo blGeoInstanceDeviceAddressInfo;
		blGeoInstanceDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		blGeoInstanceDeviceAddressInfo.buffer = blGeoInstanceBuffer;
		blGeoInstanceDeviceAddressInfo.pNext = NULL;
		blAddress = pvkGetBufferDeviceAddressKHR(*mainLogicalDevice, &blGeoInstanceDeviceAddressInfo);
		//Geo data setup for top level 
		VkAccelerationStructureGeometryDataKHR tlGeoData;
		tlGeoData.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		tlGeoData.instances.arrayOfPointers = VK_FALSE;
		tlGeoData.instances.data.deviceAddress = blAddress;
		tlGeoData.instances.pNext = NULL;
		//top level structure being preped to be built
		VkAccelerationStructureGeometryKHR tlASGeo;
		tlASGeo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		tlASGeo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlASGeo.geometry = tlGeoData;
		tlASGeo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		tlASGeo.pNext = NULL;
		//Settings used to build the actual geo
		VkAccelerationStructureBuildGeometryInfoKHR tlASBuildGeoInfo;
		tlASBuildGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		tlASBuildGeoInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlASBuildGeoInfo.flags = 0;
		tlASBuildGeoInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		tlASBuildGeoInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		tlASBuildGeoInfo.dstAccelerationStructure = VK_NULL_HANDLE;
		tlASBuildGeoInfo.geometryCount = 1;
		tlASBuildGeoInfo.pGeometries = &tlASGeo;
		tlASBuildGeoInfo.ppGeometries = NULL;
		tlASBuildGeoInfo.scratchData.deviceAddress = 0;
		tlASBuildGeoInfo.pNext = NULL;
		//How much should be allocated to the TlASGeo?
		VkAccelerationStructureBuildSizesInfoKHR tlASBuildSizesInfo;
		tlASBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		tlASBuildSizesInfo.accelerationStructureSize = 0;
		tlASBuildSizesInfo.updateScratchSize = 0;
		tlASBuildSizesInfo.buildScratchSize = 0;
		tlASBuildSizesInfo.pNext = NULL;
		//We are only going to have 1 primative since we only have 1 top level geo
		std::vector<uint32_t> topLevelMaxPrimitiveCountList = { 1 };
		pvkGetAccelerationStructureBuildSizesKHR(*mainLogicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&tlASBuildGeoInfo,
			topLevelMaxPrimitiveCountList.data(),
			&tlASBuildSizesInfo);
		VkBufferCreateInfo tlASBufferCreateInfo;
		tlASBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		tlASBufferCreateInfo.size = tlASBuildSizesInfo.accelerationStructureSize;
		tlASBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
		tlASBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		tlASBufferCreateInfo.queueFamilyIndexCount = 1;
		tlASBufferCreateInfo.pQueueFamilyIndices = &simultIndex;
		tlASBufferCreateInfo.pNext = NULL;
		tlASBufferCreateInfo.flags = 0;
		//VkBuffer tlASBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(*mainLogicalDevice, &tlASBufferCreateInfo, nullptr, &tASSBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Buffer for tlAS cannot be made!");
		}
		//Check to see what memory our graphics card has for the buffer
		VkMemoryRequirements tlASMemoryRequirements;
		vkGetBufferMemoryRequirements(
			*mainLogicalDevice, tASSBuffer,
			&tlASMemoryRequirements);

		uint32_t topLevelAccelerationStructureMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {

			if ((tlASMemoryRequirements.memoryTypeBits &
				(1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

				topLevelAccelerationStructureMemoryTypeIndex = x;
				break;
			}
		}
		//Allocate memory to buffer the tlAS will be stored on
		VkMemoryAllocateInfo tlASMemoryAllocateInfo;
		tlASMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		tlASMemoryAllocateInfo.allocationSize = tlASMemoryRequirements.size;
		tlASMemoryAllocateInfo.memoryTypeIndex = topLevelAccelerationStructureMemoryTypeIndex;
		tlASMemoryAllocateInfo.pNext = NULL;

		VkDeviceMemory tlASDeviceMemoryHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(*mainLogicalDevice, &tlASMemoryAllocateInfo
			, NULL, &tlASDeviceMemoryHandle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate memory for the tlAS buffer");
		}
		if (vkBindBufferMemory(*mainLogicalDevice, tASSBuffer,tlASDeviceMemoryHandle,0) != VK_SUCCESS) {
			throw std::runtime_error("Failed to bind the memory to the buffer from the device");
		}
		//The settings for the tlAS
		VkAccelerationStructureCreateInfoKHR tlASCreateInfo;
		tlASCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		tlASCreateInfo.createFlags = 0;
		tlASCreateInfo.buffer = tASSBuffer;
		tlASCreateInfo.offset = 0;
		tlASCreateInfo.size = tlASBuildSizesInfo.accelerationStructureSize;
		tlASCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlASCreateInfo.deviceAddress = 0;
		tlASCreateInfo.pNext = NULL;
		if (pvkCreateAccelerationStructureKHR(*mainLogicalDevice, &tlASCreateInfo, NULL, &tlAShandle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create the tlAS");
		}
		//Building the tlAS
		//Setup the address we are going to build the tlAS on
		//Building here means populating the structure with data and such
		VkAccelerationStructureDeviceAddressInfoKHR tlASDeviceAddressInfo;
		tlASDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		tlASDeviceAddressInfo.accelerationStructure = tlAShandle;
		tlASDeviceAddressInfo.pNext = NULL;

		VkDeviceAddress tlASDeviceAddress =
			pvkGetAccelerationStructureDeviceAddressKHR(
				*mainLogicalDevice, &tlASDeviceAddressInfo);
		//We are going to make a temporary buffer to help store info related to building the TLAS
		VkBufferCreateInfo tlASScratchBufferCreateInfo;
		tlASScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			tlASScratchBufferCreateInfo.pNext = NULL;
			tlASScratchBufferCreateInfo.flags = 0;
			tlASScratchBufferCreateInfo.size = tlASBuildSizesInfo.buildScratchSize;
			tlASScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			tlASScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			tlASScratchBufferCreateInfo.queueFamilyIndexCount = 1;
			tlASScratchBufferCreateInfo.pQueueFamilyIndices = &simultIndex;

		VkBuffer tlASScratchBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(
			*mainLogicalDevice, &tlASScratchBufferCreateInfo, NULL,
			&tlASScratchBufferHandle) != VK_SUCCESS) {
			throw std::runtime_error("Scratch memory buffer couldnt be built");
		}
		//Get memory req to determine what kinds of memory the GPU has
		VkMemoryRequirements tlASScratchMemoryRequirments;
		vkGetBufferMemoryRequirements(*mainLogicalDevice, tlASScratchBufferHandle, &tlASScratchMemoryRequirments);
		topLevelAccelerationStructureMemoryTypeIndex = -1;
		for (uint32_t x = 0; x < memProperties.memoryTypeCount;
			x++) {

			if ((tlASScratchMemoryRequirments.memoryTypeBits &
				(1 << x)) &&
				(memProperties.memoryTypes[x].propertyFlags &
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

				topLevelAccelerationStructureMemoryTypeIndex = x;
				break;
			}
		}
		//Time to allocate memory to the buffer we are going to build on
		VkMemoryAllocateInfo tlASScratchMemoryAllocateInfo;
		VkMemoryAllocateFlagsInfo defaultFlagsScratch = getDefaultAllocationFlags();
		tlASScratchMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		tlASScratchMemoryAllocateInfo.pNext = &defaultFlagsScratch;
		tlASScratchMemoryAllocateInfo.allocationSize = tlASScratchMemoryRequirments.size;
		tlASScratchMemoryAllocateInfo.memoryTypeIndex = topLevelAccelerationStructureMemoryTypeIndex;
		VkDeviceMemory tlASDeviceScratchMemoryHandle = VK_NULL_HANDLE;
		if (vkAllocateMemory(*mainLogicalDevice, &tlASScratchMemoryAllocateInfo, NULL, &tlASDeviceScratchMemoryHandle)
			!= VK_SUCCESS) {
			throw std::runtime_error("Couldn't allocate memory to scratch buffer for tlAS");
		}
		//Need to bind the allocated memory to the scratch buffer so we know where to build it
		if (vkBindBufferMemory(*mainLogicalDevice, tlASScratchBufferHandle, tlASDeviceScratchMemoryHandle, 0)
			!= VK_SUCCESS) {
			throw std::runtime_error("Couldn't bind memeory for hte scratch buffer");
		}
		//We need to get the device address of the scratch buffer so we can direct future code to the correct
		//place to build the topl level geometry!
		//This gets us the info used to get the actual addresss
		VkBufferDeviceAddressInfo tlASScratchBufferDeviceAddressInfo;
		tlASScratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		tlASScratchBufferDeviceAddressInfo.pNext = NULL;
		tlASScratchBufferDeviceAddressInfo.buffer = tlASScratchBufferHandle;
		//Time to actually get the device address and use it to tell the scratch buffer where to build the tlAS
		VkDeviceAddress tlASScratchBufferDeviceAddress = pvkGetBufferDeviceAddressKHR(*mainLogicalDevice, &tlASScratchBufferDeviceAddressInfo);
		tlASBuildGeoInfo.dstAccelerationStructure = tlAShandle;
		tlASBuildGeoInfo.scratchData.deviceAddress = tlASScratchBufferDeviceAddress;
		//We need to tell the pipeline what offsets to expect for the geometry 
		VkAccelerationStructureBuildRangeInfoKHR tlASSBuildRangeInfo;
		tlASSBuildRangeInfo.firstVertex = 0;
		tlASSBuildRangeInfo.primitiveCount = 1;
		tlASSBuildRangeInfo.primitiveOffset = 0;
		tlASSBuildRangeInfo.transformOffset = 0;
		//We only have an array of 1 since there is only 1 primative here
		VkAccelerationStructureBuildRangeInfoKHR* tlASSBuildRangeInfos = &tlASSBuildRangeInfo;
		//We need to now create a command buffer and submit our memory transfer so we can build the TLAS
		//Allocate memory for command buffer
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = *mainCommandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.pNext = NULL;
		//Memory transfer is executed using command buffers
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(*mainLogicalDevice, &allocInfo, &commandBuffer);
		//Going to be completely unabstracted do to reference code
		VkCommandBufferBeginInfo tlCommandBufferBeginInfo;
		tlCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		tlCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		tlCommandBufferBeginInfo.pNext = NULL;
		tlCommandBufferBeginInfo.pInheritanceInfo = NULL;
		if (vkBeginCommandBuffer(commandBuffer, &tlCommandBufferBeginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Ray tracing command buffer cant start!");
		}
		//Add the command we want to submmit, which is to finally build the TLAS!
		pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlASBuildGeoInfo, &tlASSBuildRangeInfos);
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
		VkFenceCreateInfo tlFenceInfo;
		tlFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		tlFenceInfo.pNext = NULL;
		tlFenceInfo.flags = 0;
		VkFence tlFence;
		if (vkCreateFence(*mainLogicalDevice, &tlFenceInfo, nullptr, &tlFence) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't make the fence for the TLAS!");
		}
		//Submit the command buffer and check on the fences
		if (vkQueueSubmit(*mainGraphicsQueue, 1, &submitInfo, tlFence) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't queue command buffer!");
		}
		VkResult r = vkWaitForFences(*mainLogicalDevice, 1, &tlFence, true, UINT32_MAX);
		if (r != VK_SUCCESS && r != VK_TIMEOUT) {
			throw std::runtime_error("Failed to wait for fences");
		}
		//Free up our one time command buffer submission
		vkDestroyFence(*mainLogicalDevice, tlFence, NULL);
		vkFreeCommandBuffers(*mainLogicalDevice, *mainCommandPool, 1, &commandBuffer);
}
	void RayTracer::createRayTracingPipeline() {
		VkRayTracingPipelineCreateInfoKHR rtPipeline;
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
		std::vector<VkDescriptorSetLayout> rtDescSetLayout = { descriptorSetLayout,*mainDescSetLayout };
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayout.size());
		pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayout.data();
		pipelineLayoutCreateInfo.flags = 0;
		pipelineLayoutCreateInfo.pNext = NULL;
		//Finish the pipeline layout
		if(vkCreatePipelineLayout(*mainLogicalDevice, &pipelineLayoutCreateInfo, nullptr, &rayPipelineLayout) != VK_SUCCESS){
			throw std::runtime_error("Failed to make the rt pipeline layout!");
		}
		rtPipeline.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rtPipeline.pNext = NULL;
		rtPipeline.flags = 0;
		rtPipeline.stageCount = stages.size();
		rtPipeline.pStages = stages.data();
		rtPipeline.groupCount = raytracingShaderGroups.size();
		rtPipeline.pGroups = raytracingShaderGroups.data();
		rtPipeline.layout = rayPipelineLayout;
		rtPipeline.maxPipelineRayRecursionDepth = 1;
		rtPipeline.basePipelineHandle = VK_NULL_HANDLE;
		rtPipeline.basePipelineIndex = 0;
		rtPipeline.pDynamicState = NULL;
		rtPipeline.pLibraryInfo = NULL;
		rtPipeline.pLibraryInterface = NULL;

		if (pvkCreateRayTracingPipelinesKHR(*mainLogicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rtPipeline, NULL, &raytracingPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to make the rt pipeline!");
		}
		//Get rid of the modules since we don't need them now
		for (auto& s : stages){
			vkDestroyShaderModule(*mainLogicalDevice, s.module, nullptr);
		}
	}
	void RayTracer::raytrace(VkCommandBuffer& cmdBuf,std::vector<void *>& uniBufferMMap, glm::vec4 clearColor) {
		
		//submit queue
		//Wait for frame to be finished drawing
		VkResult fenceResult = vkWaitForFences(*mainLogicalDevice, 1, &((* rayFences)[*currentFrameRef]), VK_TRUE, UINT64_MAX);
		if (fenceResult != VK_SUCCESS && fenceResult != VK_TIMEOUT) {
			std::cout << "Ray trace function failure!\n";
			throw std::runtime_error("failed to wait for fences!");
		}
		uint32_t imageIndex;
		//Make sure the chain is fresh so we know we can use it. This allows us to delay a fense reset and stop a deadlock
		VkResult result = vkAcquireNextImageKHR(*mainLogicalDevice, *raySwapchain, UINT64_MAX, (* rayImageAvailableSemaphores)[*currentFrameRef], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			//recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		vkResetFences(*mainLogicalDevice, 1, &((*rayFences)[*currentFrameRef]));
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
		memcpy(uniBufferMMap[*currentFrameRef], &ubo, sizeof(ubo));
		vkResetCommandBuffer(cmdBuf, 0);

		//Building pipeline and layout
		pcRay.clearColor = clearColor;
		pcRay.lightPos = rastSource->pos;
		pcRay.lightIntensity = rastSource->intensity;
		pcRay.lightType = rastSource->type;
		//Desc sets to bind
		std::vector<VkDescriptorSet> descSets{ descriptorSets[*currentFrameRef], (mainDescSets->at(*currentFrameRef)) };
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional Controls how command buffer will be used
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(cmdBuf, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
		QueueFamilyIndices indices = findQueueFamilies(*mainPhysicalDevice);
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
		rayTraceBarrier.image = rtImage; //specify image effected by rayTraceBarrier
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
			0, sizeof(PushConstantRay), &pcRay);
		pvkCmdTraceRaysKHR(cmdBuf, &rayGenRegion,&rayMissRegion, &rayHitRegion, &rayCallRegion
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
		swapchainCopyMemoryBarrier.image = (*raySwapchainImages)[*currentFrameRef];
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
		rayTraceCopyMemoryBarrier.image = rtImage;
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

		vkCmdCopyImage(cmdBuf, rtImage,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(*raySwapchainImages)[*currentFrameRef],
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
		swapchainPresentMemoryBarrier.image = (*raySwapchainImages)[*currentFrameRef];
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
			rayTraceWriteMemoryBarrier.image = rtImage,
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
		VkSemaphore waitSemaphores[] = { (*rayImageAvailableSemaphores)[*currentFrameRef] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		//Specify which command buffer to submit
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;
		//Specify which semaphores to signal once the command buffers finished execution
		VkSemaphore signalSemaphores[] = { (*rayFinishedSemaphores)[*currentFrameRef] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		result = vkQueueSubmit(*mainGraphicsQueue, 1, &submitInfo, (*rayFences)[*currentFrameRef]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}
		//Specify which semaphores to wait on
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		//specify the swap chain to present images to and the index for each chain
		VkSwapchainKHR swapChains[] = { *raySwapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		//Presents the triangle we submitted to the queue
		result = vkQueuePresentKHR(*rayPresentQueue, &presentInfo);
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
	//From Main: FIGURE OUT HOW TO REPLACE THIS AND AVOID COPYING CODE!
	QueueFamilyIndices RayTracer::findQueueFamilies(VkPhysicalDevice device) {
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
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, *mainSurface, &presentSupport);
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
	uint32_t RayTracer::findSimultGraphicsAndPresentIndex(VkPhysicalDevice phyDevice) {
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
				vkGetPhysicalDeviceSurfaceSupportKHR(phyDevice, i, *mainSurface, &presentSupport);
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
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //The bytecode pointer is a uint32 and not a char, hence the cast
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(*(this->mainLogicalDevice), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule; //A thin wrapper around the byte code. Compliation + linking occurs at graphics pipeline time
	}
	void RayTracer::Cleanup() {
		//Acceleration Structures
		pvkDestroyAccelerationStructureKHR(*mainLogicalDevice, tlAShandle, nullptr);
		pvkDestroyAccelerationStructureKHR(*mainLogicalDevice, blAShandle, nullptr);
		vkDestroyBuffer(*mainLogicalDevice, tASSBuffer, nullptr);
		vkDestroyBuffer(*mainLogicalDevice, bASSBuffer, nullptr);
		//Ray Tracing Pipeline
		vkDestroyDescriptorSetLayout(*mainLogicalDevice, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(*mainLogicalDevice, descriptorPool, nullptr);
		vkDestroyPipeline(*mainLogicalDevice, raytracingPipeline, nullptr);
		vkDestroyPipelineLayout(*mainLogicalDevice, rayPipelineLayout, nullptr);
		//Shader Binding Table and Ray Trace Image
		vkDestroyBuffer(*mainLogicalDevice, sbtBuffer, nullptr);
		vkDestroyImageView(*mainLogicalDevice, rtImageView, nullptr);
		vkDestroyImage(*mainLogicalDevice, rtImage, nullptr);
		vkFreeMemory(*mainLogicalDevice, rtImageDeviceMemory, nullptr);

	}
