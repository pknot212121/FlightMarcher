#include "application.h"

void Application::initializeBuffers()
{
    std::vector<VertexAttributes> vertexData;
    bool success = loadGeometryFromObj(RESOURCE_DIR "/pyramid.obj", vertexData);
    assert(success);
    vertexBuffer = createBuffer(device, "vertex_buffer", vertexData.size() * sizeof(VertexAttributes), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(VertexAttributes));
    vertexCount = static_cast<int32_t>(vertexData.size());

    for (int i = 0; i < 100; i++)
    {
        Airplane plane{};
        vec2 latLon {(i % 10) * 15.0f - 60.0f, (i / 10) * 36.0f - 180.0f};
        plane.setFlightData(latLon, 5.0f, (i * 25) % 360);
        plane.setScale(15.0f);
        planes.push_back(plane);
    }
    instanceBuffer = createBuffer(device, "instance", planes.size() * sizeof(mat4), wgpu::BufferUsage::Storage);
    uniformBuffer = createBuffer(device, "uniform buffer", sizeof(MyUniforms), wgpu::BufferUsage::Uniform);

    wgpu::Limits limits;
    device.GetLimits(&limits);

    mat4x4 V = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    mat4x4 P = glm::perspective(fov, 1.0f, 0.01f, 1000.0f);

    MyUniforms uniformValues {
        .projectionMatrix = P,
        .viewMatrix = V,
    };

    device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniformValues, sizeof(MyUniforms));
}

void Application::initializePipeline()
{
    assert(surfaceFormat);
    assert(vertexBuffer);
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

    mainBindGroup.addBuffer(0, uniformBuffer, sizeof(MyUniforms), wgpu::BufferBindingType::Uniform);
    mainBindGroup.addBuffer(1, instanceBuffer, planes.size() * sizeof(mat4), wgpu::BufferBindingType::ReadOnlyStorage, wgpu::ShaderStage::Vertex);
    mainBindGroup.build(device);
    wgpu::BindGroupLayout bindGroupLayout = mainBindGroup.getLayout();
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

        for (auto& plane : planes)
        {
            plane.fly({0.0f, 0.05f});
        }

        std::vector<glm::mat4> instanceData;
        instanceData.reserve(planes.size());
        for (auto& plane : planes)
        {
            instanceData.push_back(plane.getModelMatrix());
        }

        device.GetQueue().WriteBuffer(
            instanceBuffer, 0, 
            instanceData.data(), 
            instanceData.size() * sizeof(glm::mat4)
        );

        auto encoder = device.CreateCommandEncoder();
        encoder.SetLabel("Main command encoder");
        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);

        auto backbufferView = surfaceTexture.texture.CreateView();
        backbufferView.SetLabel("Back buffer Texture View");
        float time = static_cast<float>(glfwGetTime());
        
        
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
        pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
        mainBindGroup.bind(pass, 0);
        pass.Draw(vertexCount, static_cast<uint32_t>(planes.size()), 0, 0);
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
    vertexBuffer.Destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
}