struct ResourceData {
    DescriptorHandle<RWTexture2D> texture;
    uint cellWidth;
    uint cellHeight;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

[shader("compute")]
[numthreads(32, 32, 1)]
void cs_main(uint2 tid: SV_DispatchThreadID) {
    RWTexture2D texture = resource.texture;
    uint cellWidth = resource.cellWidth;
    uint cellHeight = resource.cellHeight;

    uint width;
    uint height;
    texture.GetDimensions(width, height);

    if (tid.x >= width || tid.y >= height) return;

    bool isWhite = ((tid.x / cellWidth) + (tid.y / cellHeight)) % 2 == 0;
    texture[tid] = isWhite ?
        float4(1, 1, 1, 1) :
        float4(0, 0, 0, 1);
}
