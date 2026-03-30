#include <metal_stdlib>

using namespace metal;

kernel void cs_vector_multiply(
    device const float* A [[buffer(0)]],
    device const float* B [[buffer(1)]],
    device float*     out [[buffer(2)]],
    uint tid [[thread_position_in_grid]]
){
    out[tid] = A[tid] * B[tid];
}