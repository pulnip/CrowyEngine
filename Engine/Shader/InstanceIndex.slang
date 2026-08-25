#pragma once

// SV_InstanceID is zero-based per draw on D3D. Slang's Metal target instead
// maps it straight to [[instance_id]], which counts from base_instance - the
// SPIR-V target has subtracted the base since slang 6.0, but that policy was
// never ported to Metal. Every shader that reads SV_InstanceID in a draw whose
// baseInstance is not zero goes through this helper to get the D3D semantics
// on every backend. InstanceIdSpike is the regression check for this contract.
uint zeroBasedInstanceID(uint instanceID, uint baseInstance){
    __target_switch{
    case metal: return instanceID - baseInstance;
    default: return instanceID;
    }
}
