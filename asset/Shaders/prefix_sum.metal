#include <metal_stdlib>

using namespace metal;

uint nextPow2(uint n){
    return 1 << (32 - clz(n-1));
}

kernel void cs_prefix_sum(
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

    // up-sweep
    for(uint stride=1; stride<n; stride*=2){
        uint idx = (tid+1)*stride*2 - 1;
        if(idx < n)
            shared[idx] += shared[idx - stride];

        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    if(tid == 0) shared[n-1] = 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    // down-sweep
    for(uint stride=n/2; stride>=1; stride/=2){
        uint idx = (tid+1)*stride*2 - 1;
        if(idx < n){
            uint tmp = shared[idx - stride];
            shared[idx - stride] = shared[idx];
            shared[idx] += tmp;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    out[2*tid + 0] = shared[2*tid + 0];
    out[2*tid + 1] = shared[2*tid + 1];
}