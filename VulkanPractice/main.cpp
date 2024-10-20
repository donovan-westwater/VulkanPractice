#include "common.h"
#include "ResourceManager.h"
#include "Pipeline.h"
#include "Texture.h"
#include "Mesh.h"
#include "Model.h"
#include "RayTracer.h"

class GameApplication {
public:
    void run() {
        //Create Resource manager
        ResourceManager::manager = new ResourceManager();
        initWindow();
        initVulkan();
        //Setup RayTracer
        //CreateLightAndPassVarsToRayTracer();
        //DEBUG: LOOK FOR BAD / UNITALIZIED COMMAND BUFFERS or ONES WHICH WERENT COMPLETELY CLEANED!
        //rayTracer.setupRayTracer(vertexBuffer, indexBuffer, vertices.size(),materialBuffer,materialIndexBuffer);
        mainLoop();
        cleanup();
        //Delete manager when we are done
        delete ResourceManager::manager;
    }

private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    
    RayTracer rayTracer;
    bool framebufferResized = false;


    
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
        ResourceManager::manager->window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(ResourceManager::manager->window, this);
        //Create a callback for window resizing
        glfwSetFramebufferSizeCallback(ResourceManager::manager->window, framebufferResizeCallback);
    }
    //stays in main
    //Call back for when we want to resize the widow. Static GLFW doesn't know what to do with a member function version
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<GameApplication*>(glfwGetWindowUserPointer(window));
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
        for (const char* layerName : ResourceManager::manager->validationLayers) {
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

        if (ResourceManager::manager->enableValidationLayers) {
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
        if (ResourceManager::manager->enableValidationLayers && !checkValidationLayerSupport()) {
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
        if (ResourceManager::manager->enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(ResourceManager::manager->validationLayers.size());
            createInfo.ppEnabledLayerNames = ResourceManager::manager->validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }
        
        VkResult result = vkCreateInstance(&createInfo, nullptr, &ResourceManager::manager->instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
    void initVulkan() {
        //These are manditory for all graphics pipelines
        createInstance();
        setupDebugMessenger();
        ResourceManager::manager->initVulkan();
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
        if (!ResourceManager::manager->enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(ResourceManager::manager->instance, &createInfo, nullptr, &ResourceManager::manager->debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
    //Timer for animating triangle in quick and dirty way
    //float timer = 0;
    void mainLoop() {
        ResourceManager::manager->lastElapsedTime = std::clock();
        ResourceManager::manager->deltaTime = ResourceManager::manager->lastElapsedTime;
        while (!glfwWindowShouldClose(ResourceManager::manager->window)) {
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
            if (ResourceManager::manager->useRayTracing) {
                rayTracer.rayTrace(ResourceManager::manager->commandBuffers[ResourceManager::manager->currentFrame], ResourceManager::manager->pipelineList[0].uniformBuffersMapped ,glm::vec4(0, 0, 0, 1));
                ResourceManager::manager->currentFrame = (ResourceManager::manager->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
            }
            else drawRasterizationFrame();
            //timer += 0.01f;
            float newtime = clock();
            float oldtime = ResourceManager::manager->lastElapsedTime;
            ResourceManager::manager->deltaTime = (newtime - oldtime)/CLOCKS_PER_SEC;
            ResourceManager::manager->lastElapsedTime = newtime;
        }
        //Wait for drawing and presnetation operations to stop
        //Stops cleanup from trying to free up semaphores while to program is still running
        vkDeviceWaitIdle(ResourceManager::manager->device);
    }
    //Draw the present the frame set by the command buffer, broad plan as follows:
    //Acquire an image from the swap chain
    //execute commands that draw onto image
    //present that image to screen, reutning it to the swapchain
    //All executed asynchronously
    void drawRasterizationFrame() {
        //Wait for frame to be finished drawing
        vkWaitForFences(ResourceManager::manager->device, 1, &ResourceManager::manager->inFlightFences[ResourceManager::manager->currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        //Make sure the chain is fresh so we know we can use it. This allows us to delay a fense reset and stop a deadlock
        VkResult result = vkAcquireNextImageKHR(ResourceManager::manager->device, ResourceManager::manager->swapChain, UINT64_MAX, ResourceManager::manager->imageAvailableSemaphores[ResourceManager::manager->currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ResourceManager::manager->recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        //Reset signaling so since we have retrieved the frame now
        vkResetFences(ResourceManager::manager->device, 1, &ResourceManager::manager->inFlightFences[ResourceManager::manager->currentFrame]);
        //Acquire the image we waited on
        //vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        ResourceManager::manager->mainCamera.updateCamera();
        vkResetCommandBuffer(ResourceManager::manager->commandBuffers[ResourceManager::manager->currentFrame], 0);
        ResourceManager::manager->beginMainRenderPass(ResourceManager::manager->commandBuffers[ResourceManager::manager->currentFrame], imageIndex);
        //record the draw calls onto the command buffer for rendering.
        for (int i = 0; i < ResourceManager::manager->modelList.size(); i++) {
            Model* m = &ResourceManager::manager->modelList[i];
            Pipeline *refPipeline = &ResourceManager::manager->pipelineList[m->referencePipelineIndex];
            m->testUpdate();
            refPipeline->updateMainUniformBuffers(ResourceManager::manager->currentFrame,m);
            refPipeline->recordDrawCallCommandBuffer(ResourceManager::manager->commandBuffers[ResourceManager::manager->currentFrame],
                *m,imageIndex); //Record the draw calls we want
        }
        ResourceManager::manager->endMainRenderPass(ResourceManager::manager->commandBuffers[ResourceManager::manager->currentFrame]);

        //submit the command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        //Specify which semaphores to wait on before execution beings and in which stages of the pipeline to wait
        VkSemaphore waitSemaphores[] = { ResourceManager::manager->imageAvailableSemaphores[ResourceManager::manager->currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        //Specify which command buffer to submit
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ResourceManager::manager->commandBuffers[ResourceManager::manager->currentFrame];
        //Specify which semaphores to signal once the command buffers finished execution
        VkSemaphore signalSemaphores[] = { ResourceManager::manager->renderFinishedSemaphores[ResourceManager::manager->currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(ResourceManager::manager->graphicsQueue, 1, &submitInfo, ResourceManager::manager->inFlightFences[ResourceManager::manager->currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
        //Specify which semaphores to wait on
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        //specify the swap chain to present images to and the index for each chain
        VkSwapchainKHR swapChains[] = { ResourceManager::manager->swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        //Presents the triangle we submitted to the queue
        result = vkQueuePresentKHR(ResourceManager::manager->presentQueue, &presentInfo);
        //If the swapchain is out dated, then we need to recreate it!
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            ResourceManager::manager->recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        ResourceManager::manager->currentFrame = (ResourceManager::manager->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; //Make sure we know which of the in flight frames we are updating
    }
    
    //Cleaan up everything EXPLICITLY CREATED by us!
    //Create a destructor for the main resources or create a deleter to pass to shared ptrs
    //Will probably go with the destructor plan.
    void cleanup() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            //Unmapping memory
            vkUnmapMemory(ResourceManager::manager->device
                , ResourceManager::manager->pipelineList[0].uniformBuffersMemory[i]);

        }
        rayTracer.cleanup();
        ResourceManager::manager->resourceCleanUp();
        if (ResourceManager::manager->enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(ResourceManager::manager->instance
                , ResourceManager::manager->debugMessenger, nullptr);
        }
    }
};

int main() {
    GameApplication app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}