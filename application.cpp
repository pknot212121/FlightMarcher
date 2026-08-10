#include "application.h"

void Application::initializeBuffers()
{
    std::vector<VertexAttributes> vertexData;
    bool success = loadGeometryFromObj(RESOURCE_DIR "/plane2.obj", vertexData);
    assert(success);
    vertexBuffer = createBuffer(device, "vertex_buffer", vertexData.size() * sizeof(VertexAttributes), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(VertexAttributes));
    vertexCount = static_cast<int32_t>(vertexData.size());

    for (int i = 0; i < 100; i++)
    {
        Airplane plane{};
        vec2 latLon {(i % 10) * 15.0f - 60.0f, (i / 10) * 36.0f - 180.0f};
        plane.setFlightData(latLon, 5.0f, (i * 25) % 360);
        plane.setScale(1.5f);
        planes.push_back(plane);
    }
    instanceBuffer = createBuffer(device, "instance", planes.size() * sizeof(mat4), wgpu::BufferUsage::Storage);
    uniformBuffer = createBuffer(device, "uniform buffer", sizeof(MyUniforms), wgpu::BufferUsage::Uniform);

    wgpu::Limits limits;
    device.GetLimits(&limits);

    mat4x4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, CAMERA_UP);
    mat4x4 P = glm::perspective(FOV, 1.0f, 0.01f, 1000.0f);
    projectionMatrix = P;
    MyUniforms uniformValues {
        .projectionMatrix = P,
        .viewMatrix = V,
    };

    device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniformValues, sizeof(MyUniforms));
}

void Application::initializePipeline()
{
    assert(surfaceFormat != wgpu::TextureFormat::Undefined);
    assert(vertexBuffer);
    auto shader = loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
    assert(shader);
    wgpu::ColorTargetState target{.format = surfaceFormat,};

    DynamicVertexLayout vertexLayout({3, 3, 3});
    depthManager = std::make_unique<DepthManager>(DEPTH_TEXTURE_FORMAT, device, WIN_WIDTH, WIN_HEIGHT);

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

void Application::processInput()
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += CAMERA_SPEED * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= CAMERA_SPEED * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, CAMERA_UP)) * CAMERA_SPEED;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, CAMERA_UP)) * CAMERA_SPEED;
}

void Application::handleMouse(vec2 pos)
{
    if (firstClick)
    {
        lastXY = pos;
        firstClick = false;
    }

    vec2 offset = lastXY - pos;
    lastXY = pos;
    offset *= SENSITIVITY;
    
    quat qYaw = glm::angleAxis(glm::radians(-offset.x), vec3(0.0f, 1.0f, 0.0f));
    vec3 right = glm::normalize(glm::cross(cameraFront, CAMERA_UP));
    quat qPitch = glm::angleAxis(glm::radians(offset.y), right);

    quat totalRotation = qPitch * qYaw;
    cameraFront = glm::normalize(totalRotation * cameraFront);
}

void Application::renderFrame()
{
    if (!device || !pipeline)
    {
        return;
    }

    glfwPollEvents();
    processInput();
#ifndef __EMSCRIPTEN__
    device.Tick(); 
#endif

    mat4x4 V = glm::lookAt(cameraPos, cameraPos + cameraFront, CAMERA_UP);
    MyUniforms uniformValues {
        .projectionMatrix = projectionMatrix,
        .viewMatrix = V,
    };
    device.GetQueue().WriteBuffer(uniformBuffer, 0, &uniformValues, sizeof(MyUniforms));

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
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);

    auto backbufferView = surfaceTexture.texture.CreateView();

    wgpu::RenderPassColorAttachment attachment {
        .view = backbufferView,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = {0.2, 0.2, 0.2, 1.},
    };

    wgpu::RenderPassDescriptor renderPass {
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

#ifndef __EMSCRIPTEN__
    surface.Present();
#endif
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
    window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Triangle", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Unable to create GLFW Window";
        return false;
    }
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos)
    {
        auto app = static_cast<Application*>(glfwGetWindowUserPointer(w));
        if (app)
            app->handleMouse({xpos, ypos});
    });

    instance = wgpu::CreateInstance();

#ifdef __EMSCRIPTEN__
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = "#canvas";
    wgpu::SurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &canvasDesc;
    surface = instance.CreateSurface(&surfaceDesc);

    wgpu::RequestAdapterOptions adapterOpts {
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .compatibleSurface = surface,
    };

    instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::AllowSpontaneous,
        [this](wgpu::RequestAdapterStatus status, wgpu::Adapter ad, wgpu::StringView message) {
            if (status != wgpu::RequestAdapterStatus::Success) {
                std::cerr << "Failed to request adapter: " << (message.data ? message.data : "") << std::endl;
                return;
            }
            this->adapter = ad;

            wgpu::DeviceDescriptor deviceDesc{};
            deviceDesc.label = "Primary device";

            this->adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
                [this](wgpu::RequestDeviceStatus status, wgpu::Device dev, wgpu::StringView message) {
                    if (status != wgpu::RequestDeviceStatus::Success) {
                        std::cerr << "Failed to request device: " << (message.data ? message.data : "") << std::endl;
                        return;
                    }
                    this->device = dev;

                    wgpu::SurfaceCapabilities capabilities;
                    this->surface.GetCapabilities(this->adapter, &capabilities);
                    this->surfaceFormat = capabilities.formats[0];

                    wgpu::SurfaceConfiguration config {
                        .device = this->device,
                        .format = this->surfaceFormat,
                        .width = WIN_WIDTH,
                        .height = WIN_HEIGHT,
                        .presentMode = wgpu::PresentMode::Fifo,
                    };
                    this->surface.Configure(&config);

                    this->initializeBuffers();
                    this->initializePipeline();
                }
            );
        }
    );
#else
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
        .width = WIN_WIDTH,
        .height = WIN_HEIGHT,
        .presentMode = wgpu::PresentMode::Fifo,
    };
    surface.Configure(&config);

    initializeBuffers();
    initializePipeline();
#endif

    return true;
}

void Application::mainLoop()
{
    #ifndef __EMSCRIPTEN__
    assert(device);
    assert(pipeline);
    #endif

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* arg) {
        static_cast<Application*>(arg)->renderFrame();
    }, this, 0, true);
    #else
    while (!glfwWindowShouldClose(window))
    {
        renderFrame();
    }
    #endif
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