#include <cstring>
#include <algorithm>
#include "ModelSerializer.hpp"

namespace {
    // Align offset to 16 bytes
    inline uint32_t align16(uint32_t offset){
        return (offset + 15) & ~15;
    }

    // Write data to buffer at offset
    template<typename T>
    void writeAt(std::vector<uint8_t>& buffer, size_t offset, const T& data){
        std::memcpy(buffer.data() + offset, &data, sizeof(T));
    }

    // Read data from buffer at offset
    template<typename T>
    T readAt(const uint8_t* buffer, size_t offset){
        T data;
        std::memcpy(&data, buffer + offset, sizeof(T));
        return data;
    }

    // Submesh metadata (stored in submesh table)
    struct SubmeshTableEntry{
        uint32_t vertexOffset; // Offset in vertex array
        uint32_t vertexCount;
        uint32_t indexOffset; // Offset in index array
        uint32_t indexCount;
        uint32_t primitiveType;
        uint32_t materialSlotNameOffset; // Offset in string blob
        uint32_t materialSlotNameLength;
        uint32_t padding; // Align to 32 bytes
    };
    static_assert(sizeof(SubmeshTableEntry) == 32);
}

namespace Crowy
{
    /// Magic number for Crowy mesh files
    constexpr char MESH_FILE_MAGIC[8] = "CRMESH\x01";
    constexpr uint32_t MESH_FILE_VERSION = 1;

    // Binary format for efficient loading
    struct MeshFileHeader {
        char magic[8] = {'C', 'R', 'M', 'E', 'S', 'H', '\x01', '\0'};
        uint32_t version = MESH_FILE_VERSION;
        uint32_t headerSize = sizeof(MeshFileHeader);

        // Section offsets (from file start)
        uint32_t submeshTableOffset = 0;
        uint32_t vertexDataOffset = 0;
        uint32_t indexDataOffset = 0;
        uint32_t materialTableOffset = 0;
        uint32_t stringBlobOffset = 0;

        // Section sizes
        uint32_t submeshCount = 0;
        uint32_t totalVertexCount = 0;
        uint32_t totalIndexCount = 0;
        uint32_t materialCount = 0;
        uint32_t stringBlobSize = 0;

        // Metadata
        AxisInfo axisInfo;
        AABB bounds;
    };
    static_assert(sizeof(MeshFileHeader) % 16 == 0, "Header should be 16-byte aligned");

    std::vector<uint8_t> serializeModel(const ModelData& model){
        // Calculate sizes
        const uint32_t submeshCount = model.submeshCount();
        const uint32_t totalVertices = model.totalVertexCount();
        const uint32_t totalIndices = model.totalIndexCount();

        // Calculate string blob size (material slot names)
        uint32_t stringBlobSize = 0;
        for (const auto& submesh : model.submeshes) {
            stringBlobSize += static_cast<uint32_t>(submesh.materialSlotName.size() + 1);  // +1 for null terminator
        }

        // Calculate offsets
        const uint32_t headerSize = sizeof(MeshFileHeader);
        const uint32_t submeshTableOffset = align16(headerSize);
        const uint32_t vertexDataOffset = align16(submeshTableOffset + submeshCount * sizeof(SubmeshTableEntry));
        const uint32_t indexDataOffset = align16(vertexDataOffset + totalVertices * sizeof(Vertex));
        const uint32_t stringBlobOffset = align16(indexDataOffset + totalIndices * sizeof(uint32_t));
        const uint32_t totalSize = align16(stringBlobOffset + stringBlobSize);

        // Allocate buffer
        std::vector<uint8_t> buffer(totalSize, 0);

        // Write header
        MeshFileHeader header{};
        std::memcpy(header.magic, MESH_FILE_MAGIC, 8);
        header.version = MESH_FILE_VERSION;
        header.headerSize = sizeof(MeshFileHeader);
        header.submeshTableOffset = submeshTableOffset;
        header.vertexDataOffset = vertexDataOffset;
        header.indexDataOffset = indexDataOffset;
        header.materialTableOffset = 0;  // Not implemented yet
        header.stringBlobOffset = stringBlobOffset;
        header.submeshCount = submeshCount;
        header.totalVertexCount = totalVertices;
        header.totalIndexCount = totalIndices;
        header.materialCount = 0;  // Not implemented yet
        header.stringBlobSize = stringBlobSize;
        header.axisInfo = model.axisInfo;
        header.bounds = model.bounds;

        writeAt(buffer, 0, header);

        // Write submesh table + vertex data + index data + string blob
        uint32_t currentVertexOffset = 0;
        uint32_t currentIndexOffset = 0;
        uint32_t currentStringOffset = 0;

        for(size_t i = 0; i < model.submeshes.size(); ++i){
            const auto& submesh = model.submeshes[i];

            // Write submesh table entry
            SubmeshTableEntry entry{};
            entry.vertexOffset = currentVertexOffset;
            entry.vertexCount = submesh.vertexCount();
            entry.indexOffset = currentIndexOffset;
            entry.indexCount = submesh.indexCount();
            entry.primitiveType = static_cast<uint32_t>(submesh.primitiveType);
            entry.materialSlotNameOffset = currentStringOffset;
            entry.materialSlotNameLength = static_cast<uint32_t>(submesh.materialSlotName.size());

            const size_t entryOffset = submeshTableOffset + i * sizeof(SubmeshTableEntry);
            writeAt(buffer, entryOffset, entry);

            // Write vertices
            const size_t vertexWriteOffset = vertexDataOffset + currentVertexOffset * sizeof(Vertex);
            std::memcpy(buffer.data() + vertexWriteOffset,
                        submesh.vertices.data(),
                        submesh.vertices.size() * sizeof(Vertex));

            // Write indices
            const size_t indexWriteOffset = indexDataOffset + currentIndexOffset * sizeof(uint32_t);
            std::memcpy(buffer.data() + indexWriteOffset,
                        submesh.indices.data(),
                        submesh.indices.size() * sizeof(uint32_t));

            // Write material slot name to string blob
            const size_t stringWriteOffset = stringBlobOffset + currentStringOffset;
            std::memcpy(buffer.data() + stringWriteOffset,
                        submesh.materialSlotName.c_str(),
                        submesh.materialSlotName.size() + 1);  // Include null terminator

            // Update offsets
            currentVertexOffset += submesh.vertexCount();
            currentIndexOffset += submesh.indexCount();
            currentStringOffset += static_cast<uint32_t>(submesh.materialSlotName.size() + 1);
        }

        return buffer;
    }

    std::optional<ModelData> deserializeModel(std::span<const uint8_t> data){
        // Validate minimum size
        if(data.size() < sizeof(MeshFileHeader)){
            return std::nullopt;
        }

        // Read and validate header
        MeshFileHeader header;
        std::memcpy(&header, data.data(), sizeof(MeshFileHeader));

        // Check magic number
        if(std::memcmp(header.magic, MESH_FILE_MAGIC, 8) != 0){
            return std::nullopt;
        }

        // Check version
        if(header.version != MESH_FILE_VERSION){
            return std::nullopt;
        }

        // Validate offsets
        if(header.submeshTableOffset + header.submeshCount * sizeof(SubmeshTableEntry) > data.size() ||
            header.vertexDataOffset + header.totalVertexCount * sizeof(Vertex) > data.size() ||
            header.indexDataOffset + header.totalIndexCount * sizeof(uint32_t) > data.size() ||
            header.stringBlobOffset + header.stringBlobSize > data.size()){
            return std::nullopt;
        }

        // Create model
        ModelData model;
        model.axisInfo = header.axisInfo;
        model.bounds = header.bounds;

        const uint8_t* bufferPtr = data.data();

        // Read submeshes
        for(uint32_t i = 0; i < header.submeshCount; ++i){
            const size_t entryOffset = header.submeshTableOffset + i * sizeof(SubmeshTableEntry);
            SubmeshTableEntry entry = readAt<SubmeshTableEntry>(bufferPtr, entryOffset);

            SubmeshData submesh;

            // Read vertices
            submesh.vertices.resize(entry.vertexCount);
            const size_t vertexReadOffset = header.vertexDataOffset + entry.vertexOffset * sizeof(Vertex);
            std::memcpy(submesh.vertices.data(),
                        bufferPtr + vertexReadOffset,
                        entry.vertexCount * sizeof(Vertex));

            // Read indices
            submesh.indices.resize(entry.indexCount);
            const size_t indexReadOffset = header.indexDataOffset + entry.indexOffset * sizeof(uint32_t);
            std::memcpy(submesh.indices.data(),
                        bufferPtr + indexReadOffset,
                        entry.indexCount * sizeof(uint32_t));

            // Read primitive type
            submesh.primitiveType = static_cast<RHIPrimitiveTopology>(entry.primitiveType);

            // Read material slot name from string blob
            const size_t stringReadOffset = header.stringBlobOffset + entry.materialSlotNameOffset;
            const char* namePtr = reinterpret_cast<const char*>(bufferPtr + stringReadOffset);
            submesh.materialSlotName = std::string(namePtr, entry.materialSlotNameLength);

            model.submeshes.push_back(std::move(submesh));
        }

        return model;
    }
}