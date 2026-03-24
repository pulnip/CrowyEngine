RWStructuredBuffer<float> A : register(t0);
RWStructuredBuffer<float> B : register(t1);
RWStructuredBuffer<float> C : register(u0);

[numthreads(256, 1, 1)]
void cs_vector_addition(uint tid : SV_DispatchThreadID){
    C[tid] = A[tid] + B[tid];
}