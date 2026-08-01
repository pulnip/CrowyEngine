#pragma once

// defined in Global RootSignature, as static sampler
[[vk::binding(0, 0)]]
SamplerState linearWrap : register(s0);
[[vk::binding(1, 0)]]
SamplerState linearClamp : register(s1);
[[vk::binding(2, 0)]]
SamplerState linearMirror : register(s2);
[[vk::binding(3, 0)]]
SamplerState linearBorder : register(s3);

[[vk::binding(4, 0)]]
SamplerState nearestWrap : register(s4);
[[vk::binding(5, 0)]]
SamplerState nearestClamp : register(s5);
[[vk::binding(6, 0)]]
SamplerState nearestMirror : register(s6);
[[vk::binding(7, 0)]]
SamplerState nearestBorder : register(s7);
