// The step every particle effect runs; an effect's file includes it last,
// after its spawn(slot, generation) and update(inout p, slot).

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_step(uint slot: SV_DispatchThreadID) {
    if(slot >= effect.count)
        return;

    RWStructuredBuffer<Particle> particles = effect.particlesRW;
    if(effect.step == 0u) {
        // born at once they would rise as one wave: each starts at its own age
        Particle born = spawn(slot, 0u);
        born.ageSteps =
            effectHash(effect.seed, slot, 0u, ParticleStaggerDraw) %
            max(born.lifeSteps, 1u);
        particles[slot] = born;
        return;
    }

    Particle p = particles[slot];
    p.ageSteps += 1u;
    if(p.ageSteps >= p.lifeSteps) {
        particles[slot] = spawn(slot, p.generation + 1u);
        return;
    }

    update(p, slot);
    particles[slot] = p;
}
