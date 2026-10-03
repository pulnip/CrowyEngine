// Randomness keyed by what it belongs to, never by time, so an effect's
// frame N is the same in every run. Engine/Effects's EffectRandom.hpp
// mirrors it bit for bit.

// PCG's output permutation over 32 bits (Jarzynski and Olano, 2020)
uint pcgHash(uint value) {
    uint state = value * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// one draw of an effect's particle: its seed, slot, generation and the
// draw's index within that particle's life
uint effectHash(uint seed, uint slot, uint generation, uint k) {
    return pcgHash(seed ^ pcgHash(slot ^ pcgHash(generation ^ pcgHash(k))));
}

// effectHash's top 24 bits as a uniform in [0, 1)
float effectRandom(uint seed, uint slot, uint generation, uint k) {
    return float(effectHash(seed, slot, generation, k) >> 8u) *
           (1.0 / 16777216.0);
}
