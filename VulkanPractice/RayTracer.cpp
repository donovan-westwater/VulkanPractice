#include "common.h"

//Using the following link as a referencce: https://github.com/WilliamLewww/vulkan_ray_tracing_minimal_abstraction/blob/master/ray_pipeline/src/main.cpp
class RayTracer {
	const int MAX_FRAMES_IN_FLIGHT = 2; //The amount of frames that can be processed concurrently
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> raytracingShaderGroups;
	VkPipeline raytracingPipeline;
	VkPipelineLayout rayPipelineLayout;
	struct PushConstantRay
	{
		glm::vec4 clearColor;
		glm::vec3 lightPos;
		float lightIntensity;
		int lightType;
	};
	PushConstantRay pcRay;
	VkBuffer bASSBuffer;
	VkBuffer tASSBuffer;
	VkAccelerationStructureKHR blAShandle;
	VkAccelerationStructureKHR tlAShandle;
	VkDescriptorSetLayout descriptorSetLayout; //holds values to setup the descriptor sets
	std::vector<VkDescriptorSet> descriptorSets; //The actual descr sets
	VkDescriptorPool descriptorPool;
	VkBuffer sbtBuffer;
	VkStridedDeviceAddressRegionKHR rayGenRegion;
	VkStridedDeviceAddressRegionKHR rayMissRegion;
	VkStridedDeviceAddressRegionKHR rayHitRegion;
	VkStridedDeviceAddressRegionKHR rayCallRegion;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
public:
	bool isEnabled = true;
	VkDevice* mainLogicalDevice; //Logical device chosen by main
	VkPhysicalDevice* mainPhysicalDevice; //physical device chosen by main
	VkSurfaceKHR* mainSurface; //Surface allocated by main
	VkCommandPool* mainCommandPool; //Should point back to the main pool from the main pipeline
	VkQueue* mainGraphicsQueue; //Submission queue for the main pool
	VkImageView* rtColorBufferView; //Should point toward color buffer in main
	VkDescriptorSetLayout* mainDescSetLayout; //The desc set layout of the rasterization pipeline
	VkMemoryAllocateFlagsInfo getDefaultAllocationFlags() {
		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo;
			memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
			memoryAllocateFlagsInfo.pNext = NULL,
			memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
			memoryAllocateFlagsInfo.deviceMask = 0;
		return memoryAllocateFlagsInfo;
	}
	//Check to see if our GPU supports raytracing
	void initRayTracing()
	{
		// Requesting ray tracing properties
		rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		prop2.pNext = &rayTracingProperties;
		vkGetPhysicalDeviceProperties2(*mainPhysicalDevice, &prop2);
	}
	void modelToBLAS(VkBuffer &vertexBuffer, VkBuffer&indexBuffer, uint32_t nOfVerts) {

		VkBufferDeviceAddressInfo vInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		vInfo.buffer = vertexBuffer;
		VkDeviceAddress vertexAddress = vkGetBufferDeviceAddressKHR(*mainLogicalDevice, &vInfo);
		vInfo.buffer = indexBuffer;
		VkDeviceAddress indexAddress = vkGetBufferDeviceAddressKHR(*mainLogicalDevice, &vInfo);
		uint32_t maxPrimativeCount = nOfVerts / 3;

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		//Vertex buffer description
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexStride = sizeof(Vertex);
		//Index buffer description
		triangles.indexData.deviceAddress = indexAddress;
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.maxVertex = nOfVerts;

		VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeom.geometry.triangles = triangles;
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

		VkAccelerationStructureBuildSizesInfoKHR bottomLevelAccelerationBuildSizesInfo{};
		bottomLevelAccelerationBuildSizesInfo.sType =
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		bottomLevelAccelerationBuildSizesInfo.updateScratchSize = 0;
		bottomLevelAccelerationBuildSizesInfo.buildScratchSize = 0;
		bottomLevelAccelerationBuildSizesInfo.accelerationStructureSize = 0;
		std::vector<uint32_t> bottomLevelMaxPrimitiveCountList = { maxPrimativeCount };
		vkGetAccelerationStructureBuildSizesKHR(*mainLogicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
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
		//We are now allocating memory to the blAS
		VkMemoryAllocateInfo blASmemAllocateInfo;
		blASmemAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
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
		//VkAccelerationStructureKHR blAShandle = VK_NULL_HANDLE;
		if (vkCreateAccelerationStructureKHR(*mainLogicalDevice, &accInfo, nullptr, &blAShandle) != VK_SUCCESS) {
			throw std::runtime_error("Couldnt create bottom acceleration structure");
		}
		//Building Bottom level acceleartion structure
		VkAccelerationStructureDeviceAddressInfoKHR blASdeviceAddressInfo;
		blASdeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		blASdeviceAddressInfo.accelerationStructure = blAShandle;
		VkDeviceAddress blAddress;
		blAddress = vkGetAccelerationStructureDeviceAddressKHR(*mainLogicalDevice, &blASdeviceAddressInfo);
		VkBufferCreateInfo blASScratchBufferCreateInfo;
		blASScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		blASScratchBufferCreateInfo.flags = 0;
		blASScratchBufferCreateInfo.size = bottomLevelAccelerationBuildSizesInfo.buildScratchSize;
		blASScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		blASScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		blASScratchBufferCreateInfo.queueFamilyIndexCount = 1;
		blASScratchBufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VkBuffer blASScratchBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(*mainLogicalDevice, &blASScratchBufferCreateInfo, nullptr, &blASScratchBufferHandle) != VK_SUCCESS) {
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
		VkMemoryAllocateFlagsInfo defaultFlags = getDefaultAllocationFlags();
		VkMemoryAllocateInfo blASScratchMemoryAllocateInfo;
		blASScratchMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		blASScratchMemoryAllocateInfo.pNext = &defaultFlags;
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
		blASScratchBufferDeviceAddressInfo.buffer = blASScratchBufferHandle;
		VkDeviceAddress blASScratchBuffeDeviceAddress = vkGetBufferDeviceAddress(*mainLogicalDevice, &blASScratchBufferDeviceAddressInfo);
		//Building the actual geometry
		//Set where we want the data to be saved to
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
		//Memory transfer is executed using command buffers
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(*mainLogicalDevice, &allocInfo, &commandBuffer);
		//Going to be completely unabstracted do to reference code
		VkCommandBufferBeginInfo bottomLevelCommandBufferBeginInfo;
		bottomLevelCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bottomLevelCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(commandBuffer, &bottomLevelCommandBufferBeginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Ray tracing command buffer cant start!");
		}
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &bottomLevelAccelerationBuildGeometryInfoKHR, &bottomLevelAccelerationStructureBuildRangeInfos);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't end the command buffer");
		}
		//Submit and free the command buffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		//Helps use sync data between GPU and CPU
		VkFenceCreateInfo blBuildFenceInfo;
		blBuildFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
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
		vkFreeCommandBuffers(*mainLogicalDevice, *mainCommandPool, 1, &commandBuffer);
	}
	void createShaderBindingTable() {
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
		uint32_t handleSizeAligned = (handleSize + (handleAlign-1)) & !(handleAlign - 1);
		rayGenRegion.stride = (handleSizeAligned + (baseAlign - 1)) & !(baseAlign - 1);
		rayGenRegion.size = rayGenRegion.stride;
		rayMissRegion.stride = handleSizeAligned;
		rayMissRegion.size = (missCount * handleSizeAligned + (baseAlign - 1)) & !(baseAlign - 1);
		rayHitRegion.stride = handleSizeAligned;
		rayHitRegion.size = (hitCount * handleSizeAligned + (baseAlign - 1)) & !(baseAlign - 1);
		//Vector to store the handles to each shader
		uint32_t dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		auto result = vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(*mainLogicalDevice, raytracingPipeline, 0, handleCount, dataSize, handles.data());
		//Im breaking my consistentcy rules because this way is so much better and I want to demonstrate the better way to do this valdiatioN!
		assert(result == VK_SUCCESS);
		//Allocate buffer for SBT
		VkDeviceSize sbtSize = rayGenRegion.size + rayMissRegion.size + rayHitRegion.size;
		//create and bind buffer for SBT
	}
	void createRtDescriptorSetLayout() {
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
		accStructureBinding.binding = 1;
		accStructureBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		accStructureBinding.descriptorCount = 1;
		accStructureBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		accStructureBinding.pImmutableSamplers = nullptr;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { accStructureBinding, imageBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data(); //Array of bindings
		if (vkCreateDescriptorSetLayout(*mainLogicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}
	void createRtDescriptorPool() {
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
	void createRtDescriptorSets() {
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
			imageInfo.imageView = *rtColorBufferView;
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
			VkWriteDescriptorSetAccelerationStructureKHR writeStuct;
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
	void updateRTDescriptorSets() {
		//Relink output image in case of change in window size
		VkDescriptorImageInfo rayTraceImageDescriptorInfo;
		rayTraceImageDescriptorInfo.sampler = VK_NULL_HANDLE;
		rayTraceImageDescriptorInfo.imageView = *rtColorBufferView;
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
	void createTopLevelAS() {
		//Get the address to pass to the bl instance
		VkAccelerationStructureDeviceAddressInfoKHR blASdeviceAddressInfo;
		blASdeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		blASdeviceAddressInfo.accelerationStructure = blAShandle;
		VkDeviceAddress blAddress;
		blAddress = vkGetAccelerationStructureDeviceAddressKHR(*mainLogicalDevice, &blASdeviceAddressInfo);
		VkAccelerationStructureInstanceKHR blACSInstance;
		//Initialize an indenity matrix
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; i++) {
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

		VkBufferCreateInfo blGeoStructureReference;
		blGeoStructureReference.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		blGeoStructureReference.flags = 0;
		blGeoStructureReference.size = sizeof(VkAccelerationStructureInstanceKHR);
		blGeoStructureReference.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		blGeoStructureReference.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		blGeoStructureReference.queueFamilyIndexCount = 1;
		blGeoStructureReference.pQueueFamilyIndices = queueFamilyIndices;
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
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

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
		VkDeviceAddress blAddress = vkGetBufferDeviceAddressKHR(*mainLogicalDevice, &blGeoInstanceDeviceAddressInfo);
		//Geo data setup for top level 
		VkAccelerationStructureGeometryDataKHR tlGeoData;
		tlGeoData.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		tlGeoData.instances.arrayOfPointers = VK_FALSE;
		tlGeoData.instances.data.deviceAddress = blAddress;
		//top level structure being preped to be built
		VkAccelerationStructureGeometryKHR tlASGeo;
		tlASGeo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		tlASGeo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlASGeo.geometry = tlGeoData;
		tlASGeo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
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
		//How much should be allocated to the TlASGeo?
		VkAccelerationStructureBuildSizesInfoKHR tlASBuildSizesInfo;
		tlASBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		tlASBuildSizesInfo.accelerationStructureSize = 0;
		tlASBuildSizesInfo.updateScratchSize = 0;
		tlASBuildSizesInfo.buildScratchSize = 0;
		//We are only going to have 1 primative since we only have 1 top level geo
		std::vector<uint32_t> topLevelMaxPrimitiveCountList = { 1 };
		vkGetAccelerationStructureBuildSizesKHR(*mainLogicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&tlASBuildGeoInfo,
			topLevelMaxPrimitiveCountList.data(),
			&tlASBuildSizesInfo);
		VkBufferCreateInfo tlASBufferCreateInfo;
		tlASBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		tlASBufferCreateInfo.size = tlASBuildSizesInfo.accelerationStructureSize;
		tlASBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
		tlASBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		tlASBufferCreateInfo.queueFamilyIndexCount = 1;
		tlASBufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
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
		if (vkCreateAccelerationStructureKHR(*mainLogicalDevice, &tlASCreateInfo, NULL, &tlAShandle) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create the tlAS");
		}
		//Building the tlAS
		//Setup the address we are going to build the tlAS on
		//Building here means populating the structure with data and such
		VkAccelerationStructureDeviceAddressInfoKHR tlASDeviceAddressInfo;
		tlASDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		tlASDeviceAddressInfo.accelerationStructure = tlAShandle;

		VkDeviceAddress tlASDeviceAddress =
			vkGetAccelerationStructureDeviceAddressKHR(
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
			tlASScratchBufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VkBuffer tlASScratchBufferHandle = VK_NULL_HANDLE;
		if (vkCreateBuffer(
			*mainLogicalDevice, &tlASScratchBufferCreateInfo, NULL,
			&tlASScratchBufferHandle) != VK_SUCCESS) {
			throw std::runtime_error("Scratch memory buffer couldnt be built");
		}
		//Get memory req to determine what kinds of memory the GPU has
		VkMemoryRequirements tlASScratchMemoryRequirments;
		vkGetBufferMemoryRequirements(*mainLogicalDevice, tlASScratchBufferHandle, &tlASScratchMemoryRequirments);
		uint32_t topLevelAccelerationStructureMemoryTypeIndex = -1;
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
		VkMemoryAllocateFlagsInfo defaultFlags = getDefaultAllocationFlags();
		tlASScratchMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		tlASScratchMemoryAllocateInfo.pNext = &defaultFlags;
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
		VkDeviceAddress tlASScratchBufferDeviceAddress = vkGetBufferDeviceAddress(*mainLogicalDevice, &tlASScratchBufferDeviceAddressInfo);
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
		//Memory transfer is executed using command buffers
VkCommandBuffer commandBuffer;
vkAllocateCommandBuffers(*mainLogicalDevice, &allocInfo, &commandBuffer);
//Going to be completely unabstracted do to reference code
VkCommandBufferBeginInfo tlCommandBufferBeginInfo;
tlCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
tlCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
if (vkBeginCommandBuffer(commandBuffer, &tlCommandBufferBeginInfo) != VK_SUCCESS) {
	throw std::runtime_error("Ray tracing command buffer cant start!");
}
//Add the command we want to submmit, which is to finally build the TLAS!
vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlASBuildGeoInfo, &tlASSBuildRangeInfos);
//End Wrap up our command buffer and submit it to the pool to run on the gpu!
if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
	throw std::runtime_error("Ray tracing command buffer cant finish!");
}
VkSubmitInfo submitInfo{};
submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
submitInfo.commandBufferCount = 1;
submitInfo.pCommandBuffers = &commandBuffer;
//Get a fence for transfering the command buffer over
VkFenceCreateInfo tlFenceInfo;
tlFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
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
vkFreeCommandBuffers(*mainLogicalDevice, *mainCommandPool, 1, &commandBuffer);
	}
	void createRayTracingPipeline() {
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
		group.anyHitShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = VK_SHADER_UNUSED_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.intersectionShader = VK_SHADER_UNUSED_KHR;
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
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		std::vector<VkDescriptorSetLayout> rtDescSetLayout = { descriptorSetLayout,*mainDescSetLayout };
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayout.size());
		pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayout.data();
		//Finish the pipeline layout
		if(vkCreatePipelineLayout(*mainLogicalDevice, &pipelineLayoutCreateInfo, nullptr, &rayPipelineLayout) != VK_SUCCESS){
			throw std::runtime_error("Failed to make the rt pipeline layout!");
		}
		rtPipeline.layout = rayPipelineLayout;
		rtPipeline.maxPipelineRayRecursionDepth = 1;
		if (vkCreateRayTracingPipelinesKHR(*mainLogicalDevice, {}, {}, 1, &rtPipeline, nullptr, &raytracingPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to make the rt pipeline!");
		}
		//Get rid of the modules since we don't need them now
		for (auto& s : stages){
			vkDestroyShaderModule(*mainLogicalDevice, s.module, nullptr);
		}
	}
	//From Main: FIGURE OUT HOW TO REPLACE THIS AND AVOID COPYING CODE!
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
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
	static std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}
		//start at the beginning to see the file size and allocate buffer
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}
	//Module to handle shader programs compiled into the vulkan byte code
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //The bytecode pointer is a uint32 and not a char, hence the cast
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(*mainLogicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule; //A thin wrapper around the byte code. Compliation + linking occurs at graphics pipeline time
	}
	void Cleanup() {
		vkDestroyAccelerationStructureKHR(*mainLogicalDevice, tlAShandle, nullptr);
		vkDestroyAccelerationStructureKHR(*mainLogicalDevice, blAShandle, nullptr);
		//Pipeline goes here
		vkDestroyDescriptorSetLayout(*mainLogicalDevice, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(*mainLogicalDevice, descriptorPool, nullptr);
		vkDestroyPipeline(*mainLogicalDevice, raytracingPipeline, nullptr);
		vkDestroyPipelineLayout(*mainLogicalDevice, rayPipelineLayout, nullptr);
	}
};