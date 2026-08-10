#include <memory>
#include <GLFW/glfw3.h>
#include "misc.h"
#include "plane.h"
#include <webgpu/webgpu_cpp.h>
#include <glm/gtc/quaternion.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#include <webgpu/webgpu_glfw.h>
#include <dawn/webgpu_cpp_print.h>
#endif

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
        bool initialize();
        void mainLoop();
        void renderFrame();
        void terminate();

    private:
        void processInput();
        void handleMouse(vec2 pos);
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
        wgpu::Buffer vertexBuffer;
        int32_t vertexCount = 0;
        std::unique_ptr<DepthManager> depthManager;
        BindGroupManager mainBindGroup;
        std::vector<Airplane> planes;
        wgpu::Buffer instanceBuffer;
        wgpu::Buffer uniformBuffer;
        vec3 cameraPos {0.0f, 0.0f, 200.0f};
        vec3 cameraFront {0.0f, 0.0f, -1.0f};
        mat4x4 projectionMatrix;
        vec2 yawPitch {-90.0f, 0.0f};
        vec2 lastXY = {WIN_WIDTH / 2.0f, WIN_HEIGHT / 2.0f};
        bool firstClick = true;
};