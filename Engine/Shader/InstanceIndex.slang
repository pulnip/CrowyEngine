#pragma once

// Slang's Metal target maps SV_InstanceID to [[instance_id]], which counts
// from base_instance; this restores D3D's zero-based value (InstanceIdSpike)
uint zeroBasedInstanceID(uint instanceID, uint baseInstance){
    __target_switch{
    case metal: return instanceID - baseInstance;
    default: return instanceID;
    }
}
