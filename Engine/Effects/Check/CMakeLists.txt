# steps fields on the device and holds them against what the CPU says
crowy_declare_check(FieldCheck
SOURCES
    FieldCheck.cpp
DEPENDS
    Crowy::Effects
)

# runs a particle effect for hundreds of steps and replays every slot's
# births and ages on the CPU
crowy_declare_check(ParticleCheck
SOURCES
    ParticleCheck.cpp
DEPENDS
    Crowy::Effects
)
