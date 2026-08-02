#include <memory>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <dawn/webgpu_cpp_print.h>
#include <webgpu/webgpu_glfw.h>
#include "misc.h"

constexpr float PI = 3.14159265358979323846f;
constexpr uint32_t kWidth = 512;
constexpr uint32_t kHeight = 512;
constexpr wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;

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
            mat4x4 modelMatrix;
            vec4 color;
            float time;
            float _pad[3];
        };
        static_assert(sizeof(MyUniforms) % 16 == 0);

        GLFWwindow *window;
        wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
        wgpu::Instance instance;
        wgpu::Surface surface;
        wgpu::Adapter adapter;
        wgpu::Device device;
        wgpu::RenderPipeline pipeline;
        wgpu::Buffer pointBuffer;
        int32_t vertexCount = 0;
        std::unique_ptr<DepthManager> depthManager;
        UniformGroup<MyUniforms> mainUniforms;
};