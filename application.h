#include <memory>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <dawn/webgpu_cpp_print.h>
#include <webgpu/webgpu_glfw.h>
#include "misc.h"
#include "plane.h"

constexpr float PI = 3.14159265358979323846f;
constexpr uint32_t kWidth = 512;
constexpr uint32_t kHeight = 512;
constexpr wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;

constexpr glm::vec3 cameraPos(0.0f, 0.0f, 300.0f);
constexpr glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
constexpr glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
constexpr float fov = 1.047198f;

class Application
{
    public:
        void initializeBuffers();
        void initializePipeline();
        bool initialize();
        void mainLoop();
        void terminate();

    private:
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
};