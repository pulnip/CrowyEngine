import Metal
import QuartzCore
import Foundation

class MetalBuffer{
    var usage: RHIBufferUsageFlags = BUF_None
}

class MetalDevice{
    var device: MTLDevice
    var commandQueue: MTLCommandQueue

    // Default sampler for texture sampling
    var defaultSampler: MTLSamplerState

    init(){
        device = MTLCreateSystemDefaultDevice()!
        commandQueue = device.makeCommandQueue()!

        let samplerDesc = MTLSamplerDescriptor()
        samplerDesc.minFilter = .linear
        samplerDesc.magFilter = .linear
        samplerDesc.mipFilter = .linear
        samplerDesc.sAddressMode = .repeat
        samplerDesc.tAddressMode = .repeat
        samplerDesc.rAddressMode = .repeat
        samplerDesc.maxAnisotropy = 16

        defaultSampler = device.makeSamplerState(descriptor: samplerDesc)!
    }
}

@_cdecl("MetalDevice_create")
func MetalDevice_create(
) -> MetalDevicePtr? {
    autoreleasepool{
        let metalDevice = MetalDevice()

        return Unmanaged.passRetained(metalDevice).toOpaque()
    }
}

@_cdecl("MetalDevice_destroy")
func MetalDevice_destroy(_ devicePtr: MetalDevicePtr){
    autoreleasepool{
        let _ = Unmanaged<MetalDevice>
            .fromOpaque(devicePtr).takeRetainedValue()
    }
}