// The built-in first write of a fresh field: every 32-bit word to zero.

struct ClearPush {
    DescriptorHandle<RWStructuredBuffer<uint>> words;
    uint count;
    // the threads dispatched; a field over D3D12's one-dimension limit takes
    // several words a thread
    uint stride;
};

[[vk::push_constant]]
ConstantBuffer<ClearPush> clearPush;

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_clear(uint id: SV_DispatchThreadID) {
    for(uint word = id; word < clearPush.count; word += clearPush.stride)
        clearPush.words[word] = 0u;
}
