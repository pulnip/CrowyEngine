// FieldCheck's kernels: one push struct for every entry, which Metal needs.

#include "Random.slang"

struct CheckPush {
    DescriptorHandle<RWStructuredBuffer<uint>> words;
    uint count;
    uint value;
};

[[vk::push_constant]]
ConstantBuffer<CheckPush> check;

// every word to the value
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_fill(uint id: SV_DispatchThreadID) {
    if(id >= check.count)
        return;

    check.words[id] = check.value;
}

// every word plus the value
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_add(uint id: SV_DispatchThreadID) {
    if(id >= check.count)
        return;

    check.words[id] = check.words[id] + check.value;
}

// word i is the draw an effect seeded with the value makes for slot i
[shader("compute")]
[numthreads(64, 1, 1)]
void cs_hash(uint id: SV_DispatchThreadID) {
    if(id >= check.count)
        return;

    check.words[id] = effectHash(check.value, id, 0, 0);
}
