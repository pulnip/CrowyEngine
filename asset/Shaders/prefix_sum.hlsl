static const uint GROUP_SIZE = 1024;

ByteAddressBuffer   data       : register(t0);
RWByteAddressBuffer io_buf     : register(u0);
RWByteAddressBuffer group_sums : register(u1);

groupshared uint shared_buf[2 * GROUP_SIZE];

void up_sweep(uint tid, uint n){
    for(uint stride=1; stride<n; stride*=2){
        uint idx = (tid+1)*stride*2 - 1;
        if(idx < n)
            shared_buf[idx] += shared_buf[idx - stride];

        GroupMemoryBarrierWithGroupSync();
    }
}

void down_sweep(uint tid, uint n){
    for(uint stride=n/2; stride>=1; stride/=2){
        uint idx = (tid+1)*stride*2 - 1;
        if(idx < n){
            uint tmp = shared_buf[idx - stride];
            shared_buf[idx - stride] = shared_buf[idx];
            shared_buf[idx] += tmp;
        }
        GroupMemoryBarrierWithGroupSync();
    }
}

[numthreads(GROUP_SIZE, 1, 1)]
void cs_prefix_sum_single(uint tid : SV_GroupThreadID){
    uint n = GROUP_SIZE*2;

    if(tid < GROUP_SIZE){
        shared_buf[2*tid + 0] = io_buf.Load((2*tid + 0) * 4);
        shared_buf[2*tid + 1] = io_buf.Load((2*tid + 1) * 4);
    }
    GroupMemoryBarrierWithGroupSync();

    up_sweep(tid, n);

    if(tid == 0) shared_buf[n-1] = 0;
    GroupMemoryBarrierWithGroupSync();

    down_sweep(tid, n);

    if(tid < GROUP_SIZE){
        io_buf.Store((2*tid + 0) * 4, shared_buf[2*tid + 0]);
        io_buf.Store((2*tid + 1) * 4, shared_buf[2*tid + 1]);
    }
}

[numthreads(GROUP_SIZE, 1, 1)]
void cs_prefix_sum(
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID
){
    uint n = GROUP_SIZE*2;
    uint base = gid * n;

    if(tid < GROUP_SIZE){
        shared_buf[2*tid + 0] = data.Load((base + 2*tid + 0) * 4);
        shared_buf[2*tid + 1] = data.Load((base + 2*tid + 1) * 4);
    }
    GroupMemoryBarrierWithGroupSync();

    up_sweep(tid, n);

    if(tid == 0){
        group_sums.Store(gid * 4, shared_buf[n-1]);
        shared_buf[n-1] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    down_sweep(tid, n);

    if(tid < GROUP_SIZE){
        io_buf.Store((base + 2*tid + 0) * 4, shared_buf[2*tid + 0]);
        io_buf.Store((base + 2*tid + 1) * 4, shared_buf[2*tid + 1]);
    }
}

[numthreads(GROUP_SIZE, 1, 1)]
void cs_add_group_sums(
    uint tid : SV_GroupThreadID,
    uint gid : SV_GroupID
){
    uint base = 2 * gid * GROUP_SIZE;
    uint gs = group_sums.Load(gid * 4);

    if(tid < GROUP_SIZE){
        uint v0 = io_buf.Load((base + 2*tid + 0) * 4);
        uint v1 = io_buf.Load((base + 2*tid + 1) * 4);
        io_buf.Store((base + 2*tid + 0) * 4, v0 + gs);
        io_buf.Store((base + 2*tid + 1) * 4, v1 + gs);
    }
}
