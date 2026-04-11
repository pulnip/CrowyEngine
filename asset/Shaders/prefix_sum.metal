#include <metal_stdlib>

using namespace metal;

uint nextPow2(uint n){
    return 1 << (32 - clz(n-1));
}

void prefix_sum_up_sweep(
    threadgroup uint* shared,
    uint tid,
    uint n
){
    for(uint stride=1; stride<n; stride*=2){
        uint idx = (tid+1)*stride*2 - 1;
        if(idx < n)
            shared[idx] += shared[idx - stride];

        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}

void prefix_sum_down_sweep(
    threadgroup uint* shared,
    uint tid,
    uint n
){
    for(uint stride=n/2; stride>=1; stride/=2){
        uint idx = (tid+1)*stride*2 - 1;
        if(idx < n){
            uint tmp = shared[idx - stride];
            shared[idx - stride] = shared[idx];
            shared[idx] += tmp;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
}

kernel void cs_prefix_sum_single(
    device const uint*       data [[buffer(0)]],
    device uint*              out [[buffer(1)]],
    uint group_size [[threads_per_threadgroup]],
    uint    tid [[thread_index_in_threadgroup]]
){
    threadgroup uint shared[2048];
    uint n = group_size*2;

    shared[2*tid + 0] = data[2*tid + 0];
    shared[2*tid + 1] = data[2*tid + 1];
    threadgroup_barrier(mem_flags::mem_threadgroup);

    prefix_sum_up_sweep(shared, tid, n);

    if(tid == 0) shared[n-1] = 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    prefix_sum_down_sweep(shared, tid, n);

    out[2*tid + 0] = shared[2*tid + 0];
    out[2*tid + 1] = shared[2*tid + 1];
}

kernel void cs_prefix_sum(
    device const uint*       data [[buffer(0)]],
    device uint*              out [[buffer(1)]],
    device uint*       group_sums [[buffer(2)]],
    uint group_size [[threads_per_threadgroup]],
    uint    tid [[thread_index_in_threadgroup]],
    uint    gid [[threadgroup_position_in_grid]]
){
    threadgroup uint shared[2048];
    uint n = group_size*2;
    uint base = gid * n;

    shared[2*tid + 0] = data[base + 2*tid + 0];
    shared[2*tid + 1] = data[base + 2*tid + 1];
    threadgroup_barrier(mem_flags::mem_threadgroup);

    prefix_sum_up_sweep(shared, tid, n);

    if(tid == 0){
        group_sums[gid] = shared[n-1];
        shared[n-1] = 0;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    prefix_sum_down_sweep(shared, tid, n);

    out[base + 2*tid + 0] = shared[2*tid + 0];
    out[base + 2*tid + 1] = shared[2*tid + 1];
}

kernel void cs_add_group_sums(
    device const uint* group_sums [[buffer(0)]],
    device uint*              out [[buffer(1)]],
    uint group_size [[threads_per_threadgroup]],
    uint    tid [[thread_index_in_threadgroup]],
    uint    gid [[threadgroup_position_in_grid]]
){
    uint base = 2 * gid * group_size;
    out[base + 2*tid + 0] += group_sums[gid];
    out[base + 2*tid + 1] += group_sums[gid];
}