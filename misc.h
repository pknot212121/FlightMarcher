#include <numeric>
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
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

using glm::mat4x4;
using glm::vec4;
using glm::vec3;

namespace fs = std::filesystem;

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

            depthStencilState = {
                .format = depthTextureFormat,
                .depthWriteEnabled = true,
                .depthCompare = wgpu::CompareFunction::Less,
                .stencilReadMask = 0,
                .stencilWriteMask = 0,
            };
        }
        const wgpu::RenderPassDepthStencilAttachment* getDepthAttachment() const
        {
            return &depthAttachment;
        }
        const wgpu::DepthStencilState* getDepthStencilState() const
        {
            return &depthStencilState;
        }
    private:
        wgpu::RenderPassDepthStencilAttachment depthAttachment;
        wgpu::TextureView depthTextureView;
        wgpu::Texture depthTexture;
        wgpu::DepthStencilState depthStencilState;
};

class BindGroupManager
{
    public:
        struct BufferEntry
        {
            uint32_t binding;
            wgpu::Buffer buffer;
            uint64_t size;
            wgpu::BufferBindingType type;
            wgpu::ShaderStage visibility;
        };

        BindGroupManager& addBuffer(
            uint32_t binding,
            wgpu::Buffer buffer,
            uint64_t size,
            wgpu::BufferBindingType type,
            wgpu::ShaderStage visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment)
        {
            entries.push_back({binding, buffer, size, type, visibility});
            return *this;
        }

        void build(wgpu::Device device)
        {
            std::vector<wgpu::BindGroupLayoutEntry> layoutEntries;
            std::vector<wgpu::BindGroupEntry> groupEntries;
            layoutEntries.reserve(entries.size());
            groupEntries.reserve(entries.size());

            for (const auto& entry : entries)
            {
                layoutEntries.push_back(wgpu::BindGroupLayoutEntry {
                    .binding = entry.binding,
                    .visibility = entry.visibility,
                    .buffer = {
                        .type = entry.type,
                        .minBindingSize = entry.size,
                    }
                });
                groupEntries.push_back(wgpu::BindGroupEntry {
                    .binding = entry.binding,
                    .buffer = entry.buffer,
                    .offset = 0,
                    .size = entry.size,
                });
            }
            wgpu::BindGroupLayoutDescriptor layoutDesc {
                .entryCount = static_cast<uint32_t>(layoutEntries.size()),
                .entries = layoutEntries.data(),
            };
            layout = device.CreateBindGroupLayout(&layoutDesc);
            
            wgpu::BindGroupDescriptor groupDesc {
                .layout = layout,
                .entryCount = static_cast<uint32_t>(groupEntries.size()),
                .entries = groupEntries.data(),
            };
            bindGroup = device.CreateBindGroup(&groupDesc);
        }

        void bind(wgpu::RenderPassEncoder pass, uint32_t groupIndex) const 
        {
            pass.SetBindGroup(groupIndex, bindGroup);
        }

        wgpu::BindGroupLayout getLayout() const {return layout;}
        wgpu::BindGroup getBindGroup() const {return bindGroup;}
        
    private:
        std::vector<BufferEntry>(entries);
        wgpu::BindGroupLayout layout;
        wgpu::BindGroup bindGroup;
};

struct VertexAttributes
{
    vec3 position;
    vec3 normal;
    vec3 color;
};

inline bool loadGeometryFromObj(const fs::path& path, std::vector<VertexAttributes>& vertexData) {
    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.triangulate = true;

    readerConfig.mtl_search_path = path.parent_path().string();

    if (!reader.ParseFromFile(path.string(), readerConfig))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader Error: " << reader.Error() << std::endl;
        }
        return false;
    }

    if (!reader.Warning().empty())
    {
        std::cout << "TinyObjReader Warning: " << reader.Warning() << std::endl;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();
    
    size_t offset = std::accumulate(shapes.begin(), shapes.end(), 0, []
        (size_t total, auto& shape){return total + shape.mesh.indices.size();});
    vertexData.resize(offset);

    offset = 0;
    for (const auto& shape : shapes)
    {
        for (size_t i = 0; i < shape.mesh.indices.size(); i++)
        {
            const tinyobj::index_t& idx = shape.mesh.indices[i];

            vertexData[offset + i].position = {
                attrib.vertices[3 * idx.vertex_index + 0],
                -attrib.vertices[3 * idx.vertex_index + 2],
                attrib.vertices[3 * idx.vertex_index + 1]
            };

            if (idx.normal_index >= 0)
            {
                vertexData[offset + i].normal = {
                    attrib.normals[3 * idx.normal_index + 0],
                    -attrib.normals[3 * idx.normal_index + 2],
                    attrib.normals[3 * idx.normal_index + 1]
                };
            }
            else
            {
                vertexData[offset + i].normal = {0.0f, 1.0f, 0.0f};
            }

            vec3 finalColor {1.0f};
            size_t faceIndex = i / 3;
            if (faceIndex < shape.mesh.material_ids.size())
            {
                int matId = shape.mesh.material_ids[faceIndex];
                if (matId >= 0 && matId < static_cast<int>(materials.size()))
                {
                    finalColor = {
                        materials[matId].diffuse[0],
                        materials[matId].diffuse[1],
                        materials[matId].diffuse[2],
                    };
                }
            }
            else if (!attrib.colors.empty())
            {
                finalColor = {
                    attrib.colors[3 * idx.vertex_index + 0],
                    attrib.colors[3 * idx.vertex_index + 1],
                    attrib.colors[3 * idx.vertex_index + 2]
                };
            }
            vertexData[offset + i].color = finalColor;
        }
        offset += shape.mesh.indices.size();
    }
    return true;
}

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