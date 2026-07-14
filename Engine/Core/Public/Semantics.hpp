#pragma once

// copy - move semantics
#define CROWY_DECLARE_DEFAULT_COPYABLE(Type) \
    Type(const Type&) = default; \
    Type& operator=(const Type&) = default;
#define CROWY_DECLARE_DEFAULT_COPYABLE_NOEXCEPT(Type) \
    Type(const Type&) noexcept = default; \
    Type& operator=(const Type&) noexcept = default;
#define CROWY_DECLARE_DEFAULT_MOVABLE(Type) \
    Type(Type&&) = default; \
    Type& operator=(Type&&) = default;
#define CROWY_DECLARE_DEFAULT_MOVABLE_NOEXCEPT(Type) \
    Type(Type&&) noexcept = default; \
    Type& operator=(Type&&) noexcept = default;
#define CROWY_DECLARE_NON_COPYABLE(Type) \
    Type(const Type&) = delete; \
    Type& operator=(const Type&) = delete;
#define CROWY_DECLARE_NON_MOVABLE(Type) \
    Type(Type&&) = delete; \
    Type& operator=(Type&&) = delete;

#define CROWY_DECLARE_PINNED(Type) \
    CROWY_DECLARE_NON_COPYABLE(Type) \
    CROWY_DECLARE_NON_MOVABLE(Type)
#define CROWY_DECLARE_COPY_ONLY(Type) \
    CROWY_DECLARE_DEFAULT_COPYABLE(Type) \
    CROWY_DECLARE_NON_MOVABLE(Type)
#define CROWY_DECLARE_COPY_ONLY_NOEXCEPT(Type) \
    CROWY_DECLARE_DEFAULT_COPYABLE_NOEXCEPT(Type) \
    CROWY_DECLARE_NON_MOVABLE(Type)
#define CROWY_DECLARE_MOVE_ONLY(Type) \
    CROWY_DECLARE_NON_COPYABLE(Type) \
    CROWY_DECLARE_DEFAULT_MOVABLE(Type)
#define CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(Type) \
    CROWY_DECLARE_NON_COPYABLE(Type) \
    CROWY_DECLARE_DEFAULT_MOVABLE_NOEXCEPT(Type)
#define CROWY_DECLARE_TRANSFERABLE(Type) \
    CROWY_DECLARE_DEFAULT_COPYABLE(Type) \
    CROWY_DECLARE_DEFAULT_MOVABLE(Type)
#define CROWY_DECLARE_TRANSFERABLE_NOEXCEPT(Type) \
    CROWY_DECLARE_DEFAULT_COPYABLE_NOEXCEPT(Type) \
    CROWY_DECLARE_DEFAULT_MOVABLE_NOEXCEPT(Type)

#define CROWY_DECLARE_NON_ASSIGNABLE(Type) \
    Type& operator=(const Type&) = delete; \
    Type& operator=(Type&&) = delete;
#define CROWY_DECLARE_CONSTRUCT_ONLY(Type) \
    Type(const Type&) = default; \
    Type(Type&&) = default; \
    CROWY_DECLARE_NON_ASSIGNABLE(Type)
#define CROWY_DECLARE_CONSTRUCT_ONLY_NOEXCEPT(Type) \
    Type(const Type&) noexcept = default; \
    Type(Type&&) noexcept = default; \
    CROWY_DECLARE_NON_ASSIGNABLE(Type)

// interface semantics
#define CROWY_DECLARE_INTERFACE(Type) \
    Type() = default; \
    virtual ~Type() = default; \
    CROWY_DECLARE_PINNED(Type)
