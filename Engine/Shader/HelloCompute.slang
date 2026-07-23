struct ResourceData {
    DescriptorHandle<StructuredBuffer<float>> lhs;
    DescriptorHandle<StructuredBuffer<float>> rhs;
    DescriptorHandle<RWStructuredBuffer<float>> out;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

[shader("compute")]
[numthreads(1024, 1, 1)]
void cs_main(uint tid: SV_DispatchThreadID) {
    resource.out[tid] = resource.lhs[tid] + resource.rhs[tid];
}