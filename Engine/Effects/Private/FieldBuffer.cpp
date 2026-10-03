#include "FieldBuffer.hpp"

#include <format>
#include <limits>
#include <stdexcept>

#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    namespace
    {
        // never initialData: a buffer created with it can no longer be
        // acquired from Undefined
        RHIBufferRAII createField(
            RHIDevice& device,
            u32 count,
            u32 stride,
            StrView name
        ) {
            const auto bytes = static_cast<u64>(count) * stride;
            if(count == 0 || stride == 0 || stride % 4 != 0 ||
               bytes > std::numeric_limits<u32>::max()) {
                throw std::invalid_argument(
                    std::format(
                        "field '{}': {} elements of {} bytes; a field holds "
                        "at least one element of whole 32-bit words, in at "
                        "most 4 GiB",
                        name,
                        count,
                        stride
                    )
                );
            }

            return device.CreateBuffer(
                RHIBufferCreateDesc{
                    .size = static_cast<u32>(bytes),
                    .memory = RHIMemoryType::GPUOnly,
                    .shaderWrite = true
                },
                name
            );
        }
    }

    FieldBuffer::~FieldBuffer() = default;

    FieldBuffer::FieldBuffer(
        RHIDevice& device,
        u32 count,
        u32 stride,
        StrView name,
        RHIResourceUsage reader
    )
        : buffer(createField(device, count, stride, name)),
          count(count),
          stride(stride),
          reader(reader) {}

    RHIBufferBarrier FieldBuffer::AcquireForWrite() const {
        if(Fresh()) {
            return MakeBarrier(
                *buffer,
                RHIResourceUsage::Undefined,
                RHIResourceUsage::StorageCompute
            );
        }

        return MakeCrossSubmissionBarrier(
            *buffer,
            resting,
            RHIResourceUsage::StorageCompute
        );
    }

    RHIBufferBarrier FieldBuffer::Release() {
        resting = reader;

        return MakeBarrier(*buffer, RHIResourceUsage::StorageCompute, reader);
    }

    u64 FieldBuffer::Writable() {
        return buffer->GetWritableID(stride);
    }

    u64 FieldBuffer::Readable() {
        return buffer->GetReadableID(stride);
    }
}
