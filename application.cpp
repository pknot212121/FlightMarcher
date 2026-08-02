#include "application.h"

void Application::initializeBuffers()
{
    std::vector<float> pointData;
    std::vector<uint16_t> indexData;

    bool success = loadGeometry(RESOURCE_DIR "/pyramid.txt", pointData, indexData, 6);
    assert(success);

    indexData.resize((indexData.size() + 1) & ~1);
    indexCount = static_cast<uint32_t>(indexData.size());
    pointBuffer = createBuffer(device, "vertex_buffer", pointData.size() * sizeof(float), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(pointBuffer, 0, pointData.data(), pointData.size() * sizeof(float));

    indexBuffer = createBuffer(device, "index_buffer", (indexData.size() * sizeof(uint16_t) + 3) & ~3, wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0,indexData.data(), (indexData.size() * sizeof(uint16_t) + 3) & ~3);

    wgpu::Limits limits;
    device.GetLimits(&limits);
    
    mainUniforms.init(device);

    vec3 focalPoint(0.0, 0.0, -2.0);
    float angle2 = 3.0f * PI / 4.0f;
    float focalLength = 2.0;
    float fov = 2 * glm::atan(1 / focalLength);

    mat4x4 M(1.0);
    M = glm::rotate(M, 0.0f, vec3(0.0, 0.0, 1.0));
    M = glm::translate(M, vec3(0.5, 0.0, 0.0));
    M = glm::scale(M, vec3(0.3f));

    mat4x4 V(1.0);
    V = glm::translate(V, -focalPoint);
    V = glm::rotate(V, -angle2, vec3(1.0, 0.0, 0.0));
    mat4x4 P = glm::perspective(fov, 1.0f, 0.01f, 100.0f);

    MyUniforms uniformValues {
        .projectionMatrix = P,
        .viewMatrix = V,
        .modelMatrix = M,
        .color = {0.0f, 1.0f, 0.4f, 1.0f},
        .time = 0,
    };
    mainUniforms.update(device.GetQueue(), uniformValues);
}

void Application::initializePipeline()
{
    assert(surfaceFormat);
    assert(pointBuffer);
    assert(indexBuffer);
    auto shader = loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
    assert(shader);
    wgpu::ColorTargetState target{.format = surfaceFormat,};

    DynamicVertexLayout vertexLayout({3, 3, 3});
    depthManager = std::make_unique<DepthManager>(depthTextureFormat, device, kWidth, kHeight);

    wgpu::FragmentState fragState {
        .module = shader,
        .entryPoint = "fs",
        .constants = nullptr,
        .targetCount = 1,
        .targets = &target,
    };

    wgpu::BindGroupLayout bindGroupLayout = mainUniforms.getLayout();
    wgpu::PipelineLayoutDescriptor layoutDesc {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (wgpu::BindGroupLayout*)&bindGroupLayout,
    };

    auto layout = device.CreatePipelineLayout(&layoutDesc);
    wgpu::RenderPipelineDescriptor pipelineDesc {
        .label = "Main render pipeline",
        .layout = layout,
        .vertex = {
            .module = shader,
            .entryPoint = "vs",
            .constants = nullptr,
            .bufferCount = 1,
            .buffers = vertexLayout.getLayout(),
        },
        .depthStencil = depthManager->getDepthStencilState(),
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
        mainUniforms.updateField(device.GetQueue(), offsetof(MyUniforms, time), &time, sizeof(float));
        mat4x4 M(1.0);
        M = glm::rotate(M, time, vec3(0.0, 0.0, 1.0));
        M = glm::translate(M, vec3(0.5, 0.0, 0.0));
        M = glm::scale(M, vec3(0.3f));
        mainUniforms.updateField(device.GetQueue(), offsetof(MyUniforms, modelMatrix), &M, sizeof(mat4x4));
        wgpu::RenderPassColorAttachment attachment {
            .view = backbufferView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {0.2, 0.2, 0.2, 1.},
        };

        wgpu::RenderPassDescriptor renderPass {
            .label = "Main render pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
            .depthStencilAttachment = depthManager->getDepthAttachment(),
        };

        auto pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffer(0, pointBuffer, 0, pointBuffer.GetSize());
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexBuffer.GetSize());
        mainUniforms.bind(pass);
        pass.DrawIndexed(indexCount);
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
    device.Destroy();
    indexBuffer.Destroy();
    pointBuffer.Destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
}