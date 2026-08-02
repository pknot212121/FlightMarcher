#include <webgpu/webgpu_cpp.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <webgpu/webgpu.h>
#include <vector>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <initializer_list>
#include <stdexcept>


class DynamicVertexLayout 
{
    public:
        DynamicVertexLayout(std::initializer_list<uint32_t> sizes)
        {
            uint32_t currentOffset = 0;
            uint32_t location = 0;
            attributes.reserve(sizes.size());

            for (uint32_t count : sizes)
            {
                wgpu::VertexFormat format;
                switch (count)
                {
                    case 1: format = wgpu::VertexFormat::Float32; break;
                    case 2: format = wgpu::VertexFormat::Float32x2; break;
                    case 3: format = wgpu::VertexFormat::Float32x3; break;
                    case 4: format = wgpu::VertexFormat::Float32x4; break;
                    default:
                        throw std::invalid_argument("Invalid size of vertex attribute (only 1-4)!");
                }
                attributes.push_back(wgpu::VertexAttribute {
                    .format = format,
                    .offset = currentOffset,
                    .shaderLocation = location++
                });
                currentOffset += count * sizeof(float);
            }
            layout = wgpu::VertexBufferLayout {
                .stepMode = wgpu::VertexStepMode::Vertex,
                .arrayStride = currentOffset,
                .attributeCount = static_cast<uint32_t>(attributes.size()),
                .attributes = attributes.data()
            };
        }

        const wgpu::VertexBufferLayout* getLayout() const
        {
            return &layout;
        }
    private:
        std::vector<wgpu::VertexAttribute> attributes;
        wgpu::VertexBufferLayout layout;
};

class DepthManager
{
    public:
        DepthManager(wgpu::TextureFormat depthTextureFormat, wgpu::Device device, uint32_t kWidth, uint32_t kHeight)
        {
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

            depthAttachment = {
                .view = depthTextureView,
                .depthLoadOp = wgpu::LoadOp::Clear,
                .depthStoreOp = wgpu::StoreOp::Store,
                .depthClearValue = 1.0f,
                .stencilLoadOp = wgpu::LoadOp::Undefined,
                .stencilStoreOp = wgpu::StoreOp::Undefined,
                .stencilClearValue = 0,
            };
        }
        const wgpu::RenderPassDepthStencilAttachment* getDepthAttachment() const
        {
            return &depthAttachment;
        }
    private:
        wgpu::RenderPassDepthStencilAttachment depthAttachment;
        wgpu::TextureView depthTextureView;
        wgpu::Texture depthTexture;
};


inline bool loadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData, int dimensions)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    pointData.clear();
    indexData.clear();

    enum class Section {
        None,
        Points,
        Indices,
    };
    Section currentSection = Section::None;

    float value;
    uint16_t index;
    std::string line;
    while (!file.eof()) {
        getline(file, line);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "[points]") {
            currentSection = Section::Points; 
        }
        else if (line == "[indices]") {
            currentSection = Section::Indices;
        }
        else if (line[0] == '#' || line.empty()) {
            
        }
        else if (currentSection == Section::Points) {
            std::istringstream iss(line);
            for (int i = 0; i < dimensions + 3; ++i) {
                iss >> value;
                pointData.push_back(value);
            }
        }
        else if (currentSection == Section::Indices) {
            std::istringstream iss(line);
            for (int i = 0; i < 3; ++i) {
                iss >> index;
                indexData.push_back(index);
            }
        }
    }
    return true;
}

inline wgpu::Buffer createBuffer(const wgpu::Device& device, std::string_view label, uint64_t size_in_bytes, wgpu::BufferUsage usage)
{
    wgpu::BufferDescriptor desc{
        .label = label,
        .usage = usage | wgpu::BufferUsage::CopyDst,
        .size = size_in_bytes,
    };
    return device.CreateBuffer(&desc);
}

inline wgpu::ShaderModule createShaderModule(const wgpu::Device& device, std::string_view label, std::string_view src)
{
    wgpu::ShaderSourceWGSL wgslDesc;
    wgslDesc.code = src;

    wgpu::ShaderModuleDescriptor desc{
        .nextInChain = &wgslDesc,
        .label = label,
    };

    return device.CreateShaderModule(&desc);
}

inline uint32_t ceilToNextMultiple(uint32_t value, uint32_t step)
{
    uint32_t divideAndCeil = value / step + (value % step == 0 ? 0 : 1);
    return step * divideAndCeil;
}

inline wgpu::ShaderModule loadShaderModule(const std::filesystem::path& path, wgpu::Device device)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string shaderSource(size, ' ');
    file.seekg(0);
    file.read(shaderSource.data(), size);
    return createShaderModule(device, "shader from file", shaderSource);
}

inline void glfwError [[noreturn]] (int code, const char* message)
{
    std::cerr << "GLFW error: " << code << ":" << message;
    assert(false);
}

inline void adapterRequest(wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message, wgpu::Adapter* data)
{
    if (status != wgpu::RequestAdapterStatus::Success) {
        std::cout << "Adapter request failed: " << std::string_view(message);
        exit(1);
    }
    *data = adapter;
}

inline void deviceLost([[maybe_unused]] const wgpu::Device& device, wgpu::DeviceLostReason reason, struct wgpu::StringView message)
{
    if (message == std::string_view("A valid external Instance reference no longer exists.")) {
        return;
    }
    std::cerr << "device lost: \n";
    if (message.length > 0) {
        std::cout << ": " << std::string_view(message);
    }
    std::cout << std::endl;
}

inline void uncapturedError [[noreturn]] ([[maybe_unused]] const wgpu::Device& device, wgpu::ErrorType type, struct wgpu::StringView message)
{
    std::cout << "uncaptured error: \n";
    if (message.length > 0)
        std::cerr << ": {}" << std::string_view(message);
    std::cout << std::endl;
    assert(false);
}