#include <cassert>
#include <cstdint>
#include <memory>
#include <stdint.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <dawn/webgpu_cpp_print.h>
#include <webgpu/webgpu_glfw.h>
#include "misc.h"
#include <array>
#include <glm/glm.hpp>

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
            std::array<float, 4> color;
            std::array<float, 2> scale;
            std::array<float, 2> offset;
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
        wgpu::Buffer indexBuffer;
        uint32_t indexCount;
        std::unique_ptr<DepthManager> depthManager;
        UniformGroup<MyUniforms> mainUniforms;
};