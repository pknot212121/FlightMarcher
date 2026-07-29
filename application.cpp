#include "application.h"


void Application::initializeBuffers()
{
    std::vector<float> pointData;
    std::vector<uint16_t> indexData;

    bool success = loadGeometry(RESOURCE_DIR "/pyramid.txt", pointData, indexData, 3);
    assert(success);

    indexData.resize((indexData.size() + 1) & ~1);
    indexCount = static_cast<uint32_t>(indexData.size());
    pointBuffer = createBuffer(device, "vertex_buffer", pointData.size() * sizeof(float), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(pointBuffer, 0, pointData.data(), pointData.size() * sizeof(float));

    indexBuffer = createBuffer(device, "index_buffer", (indexData.size() * sizeof(uint16_t) + 3) & ~3, wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0,indexData.data(), (indexData.size() * sizeof(uint16_t) + 3) & ~3);

    wgpu::Limits limits;
    device.GetLimits(&limits);
    uniformStride = ceilToNextMultiple((uint32_t)sizeof(MyUniforms), (uint32_t)limits.minUniformBufferOffsetAlignment);

    MyUniforms uniforms {
        .color = {0.0f, 1.0f, 0.4f, 1.0f},
        .scale = {1.0f, 1.0f},
        .offset = {-0.5f, 0.0f},
        .time = 0,
    };
    uniformBuffer = createBuffer(device, "uniform_buffer", uniformStride + sizeof(MyUniforms), wgpu::BufferUsage::Uniform);
    device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));
}

void Application::initializePipeline()
{
    assert(surfaceFormat);
    assert(pointBuffer);
    assert(indexBuffer);
    assert(uniformBuffer);
    auto shader = loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
    assert(shader);
    wgpu::ColorTargetState target{.format = surfaceFormat,};

    /*---- VERTEX ATTRIBUTES ----*/

    std::vector<wgpu::VertexAttribute> vertexAttribs(2);
    vertexAttribs[0] = {
        .format = wgpu::VertexFormat::Float32x3,
        .offset = 0,
        .shaderLocation = 0,
    };
    vertexAttribs[1] = {
        .format = wgpu::VertexFormat::Float32x3,
        .offset = 3 * sizeof(float),
        .shaderLocation = 1,
    };
    wgpu::VertexBufferLayout vertexBufferLayout {
        .stepMode = wgpu::VertexStepMode::Vertex,
        .arrayStride = 6 * sizeof(float),
        .attributeCount = static_cast<uint32_t>(vertexAttribs.size()),
        .attributes = vertexAttribs.data()
    };

    /*---- UNIFORMS / BIND GROUPS ----*/

    wgpu::BindGroupLayout bindGroupLayout;
    wgpu::BindGroupLayoutEntry bindingLayout {
        .binding = 0,
        .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
        .buffer = {
            .type = wgpu::BufferBindingType::Uniform,
            .hasDynamicOffset = true,
            .minBindingSize = sizeof(MyUniforms),
        }
    };

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc {
        .entryCount = 1,
        .entries = &bindingLayout,
    };
    bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);
    wgpu::PipelineLayoutDescriptor layoutDesc {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (wgpu::BindGroupLayout*)&bindGroupLayout,
    };

    auto layout = device.CreatePipelineLayout(&layoutDesc);
    wgpu::BindGroupEntry binding {
        .binding = 0,
        .buffer = uniformBuffer,
        .offset = 0,
        .size = sizeof(MyUniforms),
    };

    wgpu::BindGroupDescriptor bindGroupDesc {
        .layout = bindGroupLayout,
        .entryCount = 1,
        .entries = &binding,
    };
    bindGroup = device.CreateBindGroup(&bindGroupDesc);

    wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
    wgpu::DepthStencilState depthStencilState {
        .format = depthTextureFormat,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::Less,
        .stencilReadMask = 0,
        .stencilWriteMask = 0,
    };

    wgpu::TextureDescriptor depthTextureDesc {
        .usage = wgpu::TextureUsage::RenderAttachment,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {kWidth, kHeight, 1},
        .format = depthTextureFormat,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = (wgpu::TextureFormat*)&depthTextureFormat,
    };

    depthTexture = device.CreateTexture(&depthTextureDesc);
    depthTextureView = depthTexture.CreateView();

    /*-------------- PIPELINE ------------*/

    wgpu::FragmentState fragState {
        .module = shader,
        .entryPoint = "fs",
        .constants = nullptr,
        .targetCount = 1,
        .targets = &target,
    };

    wgpu::RenderPipelineDescriptor pipelineDesc {
        .label = "Main render pipeline",
        .layout = layout,
        .vertex = {
            .module = shader,
            .entryPoint = "vs",
            .constants = nullptr,
            .bufferCount = 1,
            .buffers = &vertexBufferLayout,
        },
        .depthStencil = &depthStencilState,
        .fragment = &fragState,
    };
    pipeline = device.CreateRenderPipeline(&pipelineDesc);
}

bool Application::initialize()
{
    glfwSetErrorCallback(glfwError);
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW.";
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(kWidth, kHeight, "Triangle", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Unable to create GLFW Window";
        return false;
    }

    instance = wgpu::CreateInstance();
    surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);

    wgpu::RequestAdapterOptions adapterOpts {
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .compatibleSurface = surface,
    };
    instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::AllowSpontaneous, adapterRequest, &adapter);

    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.label = "Primary device";
    deviceDesc.SetDeviceLostCallback(wgpu::CallbackMode::AllowProcessEvents, deviceLost);
    deviceDesc.SetUncapturedErrorCallback(uncapturedError);
    device = adapter.CreateDevice(&deviceDesc);

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    surfaceFormat = capabilities.formats[0];
    wgpu::SurfaceConfiguration config {
        .device = device,
        .format = surfaceFormat,
        .width = kWidth,
        .height = kHeight,
        .presentMode = wgpu::PresentMode::Fifo,
    };
    surface.Configure(&config);

    initializeBuffers();
    initializePipeline();
    return true;
}

void Application::mainLoop()
{
    assert(device);
    assert(uniformBuffer);
    assert(bindGroup);
    assert(pipeline);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        device.Tick();
        auto encoder = device.CreateCommandEncoder();
        encoder.SetLabel("Main command encoder");
        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);

        auto backbufferView = surfaceTexture.texture.CreateView();
        backbufferView.SetLabel("Back buffer Texture View");
        float time = static_cast<float>(glfwGetTime());
        device.GetQueue().WriteBuffer(uniformBuffer, offsetof(MyUniforms,time), &time, sizeof(float));
        wgpu::RenderPassColorAttachment attachment {
            .view = backbufferView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {0., 0., 0., 1.},
        };

        wgpu::RenderPassDepthStencilAttachment depthAttachment {
            .view = depthTextureView,
            .depthLoadOp = wgpu::LoadOp::Clear,
            .depthStoreOp = wgpu::StoreOp::Store,
            .depthClearValue = 1.0f,
            .stencilLoadOp = wgpu::LoadOp::Undefined,
            .stencilStoreOp = wgpu::StoreOp::Undefined,
            .stencilClearValue = 0,
        };

        wgpu::RenderPassDescriptor renderPass {
            .label = "Main render pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
            .depthStencilAttachment = &depthAttachment,
        };

        auto pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        uint32_t dynamicOffset = 0;
        pass.SetVertexBuffer(0, pointBuffer, 0, pointBuffer.GetSize());
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexBuffer.GetSize());

        dynamicOffset = 0 * uniformStride;
        pass.SetBindGroup(0, bindGroup, 1, &dynamicOffset);
        pass.DrawIndexed(indexCount);

        // dynamicOffset = 1 * uniformStride;
        // pass.SetBindGroup(0, bindGroup, 1, &dynamicOffset);
        // pass.DrawIndexed(indexCount);

        pass.End();
        auto commands = encoder.Finish();
        device.GetQueue().Submit(1, &commands);
        surface.Present();
    }
}

void Application::terminate()
{
    surface.Unconfigure();
    surface = nullptr;
    depthTexture.Destroy();
    device.Destroy();
    indexBuffer.Destroy();
    pointBuffer.Destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
}