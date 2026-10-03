// The built-in first write of a fresh field: every 32-bit word to a value.

struct ClearPush {
    DescriptorHandle<RWStructuredBuffer<uint>> words;
    uint count;
    uint value;
};

[[vk::push_constant]]
ConstantBuffer<ClearPush> clearPush;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_clear(uint id: SV_DispatchThreadID) {
    if(id >= clearPush.count)
        return;

    clearPush.words[id] = clearPush.value;
}
