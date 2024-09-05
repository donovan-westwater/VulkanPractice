#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "common.h"
#include "Pipeline.h"
#include "Model.h"
#include "RayTracer.h"
#include "ResourceManager.h"


class HelloTriangleApplication {
public:
    static ResourceManager resourceManager;
    void run() {
        initWindow();
        initVulkan();
        //Setup RayTracer
        //CreateLightAndPassVarsToRayTracer();
        //DEBUG: LOOK FOR BAD / UNITALIZIED COMMAND BUFFERS or ONES WHICH WERENT COMPLETELY CLEANED!
        //rayTracer.setupRayTracer(vertexBuffer, indexBuffer, vertices.size(),materialBuffer,materialIndexBuffer);
        mainLoop();
        cleanup();
    }

private:
    bool useRayTracing = false;
    const int MAX_FRAMES_IN_FLIGHT = 2; //The amount of frames that can be processed concurrently
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    const std::string MODEL_PATH = "Models/CornellBox-Original.obj";//"Models/CrappyCornellBox_TriVersion.obj";//"Models/Guilmon.obj";
    const std::string TEXTURE_PATH = "Textures/TestTex.png";
    const std::string MATERIALS_PATH = "Materials/";//"Materials/CrappyCornellBox_TriVersion.mtl";

    const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
    "VK_LAYER_LUNARG_monitor"
    }; //Provides a list of required validation layers for the system
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_ray_tracing_pipeline",
      "VK_KHR_acceleration_structure",
      "VK_EXT_descriptor_indexing",
      "VK_KHR_maintenance3",
      "VK_KHR_buffer_device_address",
      "VK_KHR_deferred_host_operations"
    }; //Provides a list of required extensions for the system

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif
    RayTracer rayTracer;
    bool framebufferResized = false;
    int primativeCount = 0;

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities; //What basic surface capabilities does the swap chain have?
        std::vector<VkSurfaceFormatKHR>formats; //What surface formats do we have?
        std::vector<VkPresentModeKHR> presentModes; //What available presentation formats?
    };
    //move to common
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
    //Will disable for now
    /*
    void CreateLightAndPassVarsToRayTracer() {
        light.dir =  glm::normalize(glm::vec3(0, -1, 1));
        light.intensity = 1.0;
        light.pos = glm::vec3(0, 2, 2);
        light.type = 0;

        rayTracer.mainCommandPool = shared_commandPool;
        rayTracer.mainDescSetLayout = shared_descLayout;
        rayTracer.mainDescSets = shared_descSetList;
        rayTracer.mainGraphicsQueue = shared_graphicsQueue;
        rayTracer.mainLogicalDevice = shared_logicalDevice;
        rayTracer.mainPhysicalDevice = shared_physicalDevice;
        rayTracer.mainSurface = shared_surface;
        rayTracer.mainLightSource = shared_lightSource;
        rayTracer.heightRef = HEIGHT;
        rayTracer.widthRef = WIDTH;
        rayTracer.currentFrameRef = shared_currentFrame;
        rayTracer.mainSwapChainFormat = shared_swapChainFormat;
        rayTracer.rayTracerImageAvailableSemaphores = shared_imageAvailableSemaphores;
        rayTracer.rayTracerFinishedSemaphores = shared_finishedSemaphores;
        rayTracer.rayTracerFences = shared_fences;
        rayTracer.rayTracerPresentQueue = shared_presentQueue;
        rayTracer.rayTracerSwapchain = shared_swapchain;
        rayTracer.rayTracerSwapchainImages = shared_swapchainImages;
        rayTracer.maxPrimativeCount = primativeCount;
    }
    */
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Disable openGL API
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        //Create a callback for window resizing
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }
    //stays in main
    //Call back for when we want to resize the widow. Static GLFW doesn't know what to do with a member function version
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
        //if(app->r.isEnabled) app->r.updateRTDescriptorSets();
    }
    //stays in main
    //Check if validation layers if validation layers are available
    // These layers are important for checcking for any mistakes made in coding process
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        //Iterate through the validation layers and check if they exist
        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }
    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }
    //Debug callback function
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        
        return VK_FALSE;
    }
    //stays in main
    //Fills in the instance struct with relevant infomation
    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }
        //Optional infomation struct that is helpful to fill out
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 255);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 255);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        //Basic instance info and attaches app struct to info struct
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        //Provide vulkan the extension that allows vulkan to work with windows
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        //Ensures that vulkan gets the required extensions that allow it to work with windows and other apis
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        //In Debug mode, it will enable the validation layers used to debug the system
        //Create debug messenger for the instance creation spefically as the other debug system will be created after / destoried
        //before the device instance is created or destoried
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }
        
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
    void initVulkan() {
        //These are manditory for all graphics pipelines
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        //This is where you would start to make pipelines for different shaders 
        createGraphicsPipeline();
        //create command pool for command buffers
        createCommandPool();
        //Create color buffer
        createColorResources();
        //Create depth buffer
        createDepthResources();
        //Create framebuffers to draw the actual images with
        createFramebuffers();
        //TEXTURE LOADING WAS HERE
        //Load in the model
        loadModel();
        //Create vertex buffer for vertex shader
        createVertexBuffer();
        //Create index buffer for vertex shader
        createIndexBuffer();
        // Create material Buffer
        createMaterialBuffer();
        createMaterialIndexBuffer();
        //create ubo buffer
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        //Raytracing pipeline section

        createSyncObjects(); //create objects for syncing cpu with gpu
    }
    //stays in main
    //Figure out how many samples we can do with our device safely
    //takes in both color buffer and depth buffers into account
    VkSampleCountFlagBits getMaxUsableSampleCount() {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    //This is more like a resource function. Move it to a resource manager when that is made
    void loadModel() {
        tinyobj::attrib_t attrib; //Contains positions normals texture coords
        std::vector<tinyobj::shape_t> shapes; //seperate objects and faces
        std::vector<tinyobj::material_t> localMaterials;
        std::string warn, err;
        Model model;
        Mesh modelMesh;
        model.referenceMesh = &modelMesh;
        model.referencePipeline = &graphicsPipeline;
        if (!tinyobj::LoadObj(&attrib, &shapes, &localMaterials, &warn, &err, MODEL_PATH.c_str(),MATERIALS_PATH.c_str())) {
            throw std::runtime_error(warn + err);
        }
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        //Going to combine all the faces into to one model
        for (const auto& shape : shapes) {
            primativeCount += shape.mesh.num_face_vertices.size();
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                //array of 'vec3' represented as floats only, hence the 3 *
                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0], 
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };
                
                if(!attrib.normals.empty() && index.normal_index > 0){
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }
                //array of 'vec2' represented as floats only, hence the 3 *
                //Invert the y axis because of obj format
                if (!attrib.texcoords.empty() && index.texcoord_index > 0) {
                    vertex.texCoord = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }
                vertex.color = { 1.0f, 1.0f, 1.0f };
                
                
                //Load in the indcies and vertices
                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(modelMesh.vertices.size());
                    modelMesh.vertices.push_back(vertex);
                }
                modelMesh.indices.push_back(uniqueVertices[vertex]);
                
                //vertices.push_back(vertex);
                //indices.push_back(indices.size());
                
            }
            for (int matIndex : shape.mesh.material_ids) {
                modelMesh.materialIndices.push_back(matIndex);
            }
        }
        //Copy material infomation into vector
        for (uint32_t x = 0; x < localMaterials.size(); x++) {
            Material m;
            m.ambient = float3ToVec4(localMaterials[x].ambient);
            m.diffuse = float3ToVec4(localMaterials[x].diffuse);
            //Using IOR Is stored in ambient
            m.ambient.x = localMaterials[x].ior;
            m.ambient.y = localMaterials[x].illum;
            float clampProb = localMaterials[x].ambient[0];
            if (clampProb < 0) clampProb = 0.0;
            if (clampProb > 1.0) clampProb = 1.0;
            m.diffuse.a = clampProb;
            m.specular = float3ToVec4(localMaterials[x].specular);
            float clampedShininess = localMaterials[x].shininess;
            //Use shininess as a blending value for reflective surfaces
            if (clampedShininess < 0) clampedShininess = 0.0;
            if (clampedShininess > 1.0) clampedShininess = 1.0;
            m.specular.a = clampedShininess;
            m.emission = float3ToVec4(localMaterials[x].emission);
            modelMesh.materials.push_back(m);
        }

        meshes.push_back(modelMesh);
        models.push_back(model);
    }

    //Render loop only function - stay in main
    void createSyncObjects() {
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
    void createCommandBuffers() {
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
    void createCommandPool() {
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
    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size()); //Should be same size as the amount of images
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT,1);
        }
    }
    //Stays in main
    //Cleans up swapChain related resources
    void cleanupSwapChain() {
        //free color buffer
        vkDestroyImageView(device, colorImageView, nullptr);
        vkDestroyImage(device, colorImage, nullptr);
        vkFreeMemory(device, colorImageMemory, nullptr);
        //Free depth buffer
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    //Should create a new swap chain when the window is updated
    void recreateSwapChain() {
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
        //Pipeline Specific
        createColorResources();
        createDepthResources();
        createFramebuffers();
    }
    //stay in main
    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapChainSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities); 
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount+1; //Decide how many images we want from swapchain
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
    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }
    //stay in main
    void createLogicalDevice() {
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
        setDebugObjectName(device, VkObjectType::VK_OBJECT_TYPE_DEVICE,reinterpret_cast<uint64_t>(device)
            , "Main Logical Device");
#endif // !NDEBUG
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }
    //stay in main
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount < 1) {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }
        //Check to see if there is a GPU for Vulkan to use
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance,&deviceCount,devices.data());
        for (const auto& devcive : devices) {
            if (isDeviceSuitable(devcive)) {
                physicalDevice = devcive;
                msaaSamples = getMaxUsableSampleCount();
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }
    //stay in main
    bool isDeviceSuitable(VkPhysicalDevice device) {
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
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
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
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
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
    VkSurfaceFormatKHR chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
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
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        //If mailbox mode is supported just pick that, other pick the default mode
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    //Pick a swap extent
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
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
    //stay in main
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    //stay in main
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }
    //stay in main
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }
    //stay in main
    void setupDebugMessenger() {
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
    //Timer for animating triangle in quick and dirty way
    //float timer = 0;
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            //This is a quick and dirty way to animate the triangle. I don't think it is remotely ideal for a bunch of reasons
            /*
            vertices[0].color.r = abs(cos(timer));
            vertices[1].color.g = abs(sin(timer));
            void* data;
            vkMapMemory(device, vertexBufferMemory, 0, sizeof(vertices[0])*vertices.size(), 0, &data);
            memcpy(data, vertices.data(), (size_t)(sizeof(vertices[0]) * vertices.size()));
            vkUnmapMemory(device, vertexBufferMemory);
            */
            if (useRayTracing) {
                rayTracer.rayTrace(commandBuffers[currentFrame],uniformBuffersMapped ,glm::vec4(0, 0, 0, 1));
                currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
            }
            else drawFrame();
            //timer += 0.01f;
        }
        //Wait for drawing and presnetation operations to stop
        //Stops cleanup from trying to free up semaphores while to program is still running
        vkDeviceWaitIdle(device);
    }
    //Draw the present the frame set by the command buffer, broad plan as follows:
        //Acquire an image from the swap chain
        //execute commands that draw onto image
        //present that image to screen, reutning it to the swapchain
        //All executed asynchronously
    void drawFrame() {
        //Wait for frame to be finished drawing
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        //Make sure the chain is fresh so we know we can use it. This allows us to delay a fense reset and stop a deadlock
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        //Reset signaling so since we have retrieved the frame now
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        //Acquire the image we waited on
        //vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        updateUniformBuffers(currentFrame);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex); //Record the draw calls we want
        //submit the command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        //Specify which semaphores to wait on before execution beings and in which stages of the pipeline to wait
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        //Specify which command buffer to submit
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        //Specify which semaphores to signal once the command buffers finished execution
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
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
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; //Make sure we know which of the in flight frames we are updating
    }
    //Move to pipleine and rename to updateMainUniformBuffers
    void updateUniformBuffers(uint32_t currentFrame) {
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
        ubo.model = glm::rotate(ubo.model, time*glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        //Create a camera matrix at pos 2,2,2 look at 0 0 0, with up being Z
        ubo.view = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        //Create a perspective based projection matrix for our camera
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1; //Y-coord for clip coords is inverted. This fixes that (GLM designed for openGL)
        //ubo.colorAdd = glm::vec4(abs(cos(time)), abs(sin(time)), abs(tan(time)), 1);
        memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
    }
    //Cleaan up everything EXPLICITLY CREATED by us!
    //Create a destructor for the main resources or create a deleter to pass to shared ptrs
    //Will probably go with the destructor plan.
    void cleanup() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            //Unmapping memory
            vkUnmapMemory(device, uniformBuffersMemory[i]);

        }
        rayTracer.cleanup();
        //Former Swapchain function cleanup
        //free color buffer
        vkDestroyImageView(device, colorImageView, nullptr);
        vkDestroyImage(device, colorImage, nullptr);
        vkFreeMemory(device, colorImageMemory, nullptr);
        //Free depth buffer
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }
        //End of Swap Chain resource Cleanup
        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        //---Descriptor Set layout destruction here
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr); //Free the memory assoiated with vertex buffer
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        //--- command pool destruction here
        //for (auto framebuffer : swapChainFramebuffers) {
        //     vkDestroyFramebuffer(device, framebuffer, nullptr);
        //}
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        //for (auto imageView : swapChainImageViews) {
        //    vkDestroyImageView(device, imageView, nullptr);
        //}
        //---swapchain destruction here
        //---Logical device destruction here;
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        //---Surface Would be Destoried here
        //---Instance would be destroied here

        glfwDestroyWindow(window); //Free up memory used by window

        glfwTerminate(); //Clean up GLFW
    }
};

int main() {
    HelloTriangleApplication app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}