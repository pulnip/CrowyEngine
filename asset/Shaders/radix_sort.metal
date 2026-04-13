#include <metal_stdlib>

using namespace metal;

kernel void cs_histogram(
    device const uint*       keys [[buffer(0)]],
    device uint*        histogram [[buffer(1)]],
    constant uint&     bit_offset [[buffer(2)]],
    constant uint&          count [[buffer(3)]],
    constant uint&     num_groups [[buffer(4)]],
    uint group_size [[threads_per_threadgroup]],
    uint    tid [[thread_index_in_threadgroup]],
    uint    gid [[threadgroup_position_in_grid]]
){
    threadgroup uint local_hist[16];

    if(tid < 16) local_hist[tid] = 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    uint idx = group_size*gid + tid;
    if(idx < count){
        uint digit = (keys[idx] >> bit_offset) & 0xF;
        atomic_fetch_add_explicit(
            (threadgroup atomic_uint*)&local_hist[digit],
            1,
            memory_order_relaxed
        );
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    if(tid < 16)
        histogram[num_groups*tid + gid] = local_hist[tid];
}

kernel void cs_scatter(
    device const uint*     keys_in [[buffer(0)]],
    device const uint*     vals_in [[buffer(1)]],
    device uint*          keys_out [[buffer(2)]],
    device uint*          vals_out [[buffer(3)]],
    device const uint* prefix_sums [[buffer(4)]],
    constant uint&      bit_offset [[buffer(5)]],
    constant uint&           count [[buffer(6)]],
    constant uint&      num_groups [[buffer(7)]],
    uint  group_size [[threads_per_threadgroup]],
    uint     tid [[thread_index_in_threadgroup]],
    uint     gid [[threadgroup_position_in_grid]]
){
    uint idx = group_size*gid + tid;
    if(idx >= count) return;

    uint key = keys_in[idx];
    uint digit = (key >> bit_offset) & 0xF;

    uint base = prefix_sums[num_groups*digit + gid];

    uint local_offset = 0;
    uint block_start = group_size*gid;
    for(uint i=block_start; i<idx; ++i){
        if(((keys_in[i] >> bit_offset) & 0xF) == digit)
            ++local_offset;
    }

    keys_out[base + local_offset] = key;
    vals_out[base + local_offset] = vals_in[idx];
}