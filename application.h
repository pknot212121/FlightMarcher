#include <memory>
#include <GLFW/glfw3.h>
#include "misc.h"
#include "plane.h"
#include <webgpu/webgpu_cpp.h>
#include <glm/gtc/quaternion.hpp>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>

constexpr float PI = 3.14159265358979323846f;
constexpr uint32_t WIN_WIDTH = 512;
constexpr uint32_t WIN_HEIGHT = 512;
constexpr wgpu::TextureFormat DEPTH_TEXTURE_FORMAT = wgpu::TextureFormat::Depth24Plus;

constexpr float CAMERA_SPEED = 3.0f;
constexpr vec3 CAMERA_UP {0.0f, 1.0f, 0.0f};
constexpr float FOV = 1.047198f;
constexpr uint32_t MAX_PLANES = 30000;
constexpr float SENSITIVITY = 0.1f;

using glm::quat;

class Application
{
    public:
        void initializeBuffers();
        void initializePipeline();
        bool initializeGLFW();
        bool initialize();
        void renderFrame();
        void fetchPlanesOnDemand();
        void onResize(uint32_t width, uint32_t height);
    private:
        void processInput();
        void handleMouse(vec2 pos);
        void updatePlanes(const float* data, int sizeInBytes);
        struct MyUniforms
        {
            mat4x4 projectionMatrix;
            mat4x4 viewMatrix;
        };
        static_assert(sizeof(MyUniforms) % 16 == 0);

        GLFWwindow *window;
        wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
        wgpu::Instance instance;
        wgpu::Surface surface;
        wgpu::Adapter adapter;
        wgpu::Device device;
        wgpu::RenderPipeline pipeline;
        int32_t vertexCount = 0;
        std::unique_ptr<DepthManager> depthManager;
        BindGroupManager mainBindGroup;
        
        wgpu::Buffer vertexBuffer;
        wgpu::Buffer instanceBuffer;
        wgpu::Buffer uniformBuffer;
        
        uint16_t planesCount = 0;
        Airplane planes[MAX_PLANES];
        glm::mat4 instanceData[MAX_PLANES];
        mat4x4 projectionMatrix;

        vec3 cameraPos {0.0f, 0.0f, 200.0f};
        vec3 cameraFront {0.0f, 0.0f, -1.0f};
        vec2 yawPitch {-90.0f, 0.0f};
        vec2 lastXY = {WIN_WIDTH / 2.0f, WIN_HEIGHT / 2.0f};
        bool firstClick = true;
};