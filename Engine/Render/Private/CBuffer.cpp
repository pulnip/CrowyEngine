#include "CBuffer.hpp"
#include "RHIBuffer.hpp"

namespace Crowy
{
    std::optional<CBuffer::ConstFieldProxy> CBuffer::at(std::string_view name) const{
        auto it = fieldIndex.find(name);
        if(it == fieldIndex.end())
            return std::nullopt;

        auto& [_, field] = fields[it->second];
        return ConstFieldProxy{
            .type = field.type,
            .ptr = buffer->data(field.offset, size_of(field.type))
        };
    }

    CBuffer::ConstFieldView CBuffer::fieldView(const Field& field) const{
        return ConstFieldView{
            .name = field.name,
            .field = {
                .type = field.meta.type,
                .ptr = buffer->data(
                    field.meta.offset,
                    size_of(field.meta.type)
                )
            }
        };
    }

    CBuffer::FieldView CBuffer::fieldView(const Field& field){
        return FieldView{
            .name = field.name,
            .field = {
                .type = field.meta.type,
                .ptr = buffer->data(
                    field.meta.offset,
                    size_of(field.meta.type)
                )
            }
        };
    }

}