#pragma once

// copy - move semantics
#define SMOL_DECLARE_DEFAULT_COPYABLE(Type) \
    Type(const Type&) = default; \
    Type& operator=(const Type&) = default;
#define SMOL_DECLARE_DEFAULT_COPYABLE_NOEXCEPT(Type) \
    Type(const Type&) noexcept = default; \
    Type& operator=(const Type&) noexcept = default;
#define SMOL_DECLARE_DEFAULT_MOVABLE(Type) \
    Type(Type&&) = default; \
    Type& operator=(Type&&) = default;
#define SMOL_DECLARE_DEFAULT_MOVABLE_NOEXCEPT(Type) \
    Type(Type&&) noexcept = default; \
    Type& operator=(Type&&) noexcept = default;
#define SMOL_DECLARE_NON_COPYABLE(Type) \
    Type(const Type&) = delete; \
    Type& operator=(const Type&) = delete;
#define SMOL_DECLARE_NON_MOVABLE(Type) \
    Type(Type&&) = delete; \
    Type& operator=(Type&&) = delete;

#define SMOL_DECLARE_PINNED(Type) \
    SMOL_DECLARE_NON_COPYABLE(Type) \
    SMOL_DECLARE_NON_MOVABLE(Type)
#define SMOL_DECLARE_COPY_ONLY(Type) \
    SMOL_DECLARE_DEFAULT_COPYABLE(Type) \
    SMOL_DECLARE_NON_MOVABLE(Type)
#define SMOL_DECLARE_COPY_ONLY_NOEXCEPT(Type) \
    SMOL_DECLARE_DEFAULT_COPYABLE_NOEXCEPT(Type) \
    SMOL_DECLARE_NON_MOVABLE(Type)
#define SMOL_DECLARE_MOVE_ONLY(Type) \
    SMOL_DECLARE_NON_COPYABLE(Type) \
    SMOL_DECLARE_DEFAULT_MOVABLE(Type)
#define SMOL_DECLARE_MOVE_ONLY_NOEXCEPT(Type) \
    SMOL_DECLARE_NON_COPYABLE(Type) \
    SMOL_DECLARE_DEFAULT_MOVABLE_NOEXCEPT(Type)
#define SMOL_DECLARE_TRANSFERABLE(Type) \
    SMOL_DECLARE_DEFAULT_COPYABLE(Type) \
    SMOL_DECLARE_DEFAULT_MOVABLE(Type)
#define SMOL_DECLARE_TRANSFERABLE_NOEXCEPT(Type) \
    SMOL_DECLARE_DEFAULT_COPYABLE_NOEXCEPT(Type) \
    SMOL_DECLARE_DEFAULT_MOVABLE_NOEXCEPT(Type)

#define SMOL_DECLARE_NON_ASSIGNABLE(Type) \
    Type& operator=(const Type&) = delete; \
    Type& operator=(Type&&) = delete;
#define SMOL_DECLARE_CONSTRUCT_ONLY(Type) \
    Type(const Type&) = default; \
    Type(Type&&) = default; \
    SMOL_DECLARE_NON_ASSIGNABLE(Type)
#define SMOL_DECLARE_CONSTRUCT_ONLY_NOEXCEPT(Type) \
    Type(const Type&) noexcept = default; \
    Type(Type&&) noexcept = default; \
    SMOL_DECLARE_NON_ASSIGNABLE(Type)

// interface semantics
#define SMOL_DECLARE_INTERFACE(Type) \
    Type() = default; \
    virtual ~Type() = default; \
    SMOL_DECLARE_PINNED(Type)
