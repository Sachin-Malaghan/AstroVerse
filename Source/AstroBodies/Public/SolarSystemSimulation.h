#pragma once
#include "CoreMinimal.h"
#include "BodyRegistry.h"
#include "Physics/NBodyIntegrator.h"
// Engine-agnostic solar-system state over time. The Sun and every planet
// system are N-body particles on a fixed timestep; render-time states are
// Hermite-interpolated between the two bracketing physics steps. Moons are
// Dormant (analytic conic about their parent) unless promoted to N-body.
// Knows nothing about wall-clock time: callers pass the target sim time,
// which comes from AstroTime::UTimeController. See CLAUDE.md Phase 2.

struct FBodyInstantState
{
    FAstroVector3d Position; // m, J2000 ecliptic, solar-system barycentric
    FAstroVector3d Velocity; // m/s
};

class ASTROBODIES_API FSolarSystemSimulation
{
public:
    // Seeds from the registry's mean elements at J2000, then integrates to StartSimSeconds.
    void Initialize(const FBodyRegistry& InRegistry, double StartSimSeconds, double StepSeconds);

    // Moves the whole system to TargetSimSeconds (forward or backward).
    void AdvanceTo(double TargetSimSeconds);

    double GetSimSeconds() const { return CurrentTime; }
    const FBodyInstantState& GetBodyState(int32 BodyIndex) const { return BodyStates[BodyIndex]; }
    const TArray<FBodyInstantState>& GetBodyStates() const { return BodyStates; }
    const FBodyRegistry* GetRegistry() const { return Registry; }

    // Fixed physics timestep. Changing it re-brackets at the current time.
    void SetStepSeconds(double NewStepSeconds);
    double GetStepSeconds() const { return StepSeconds; }

    // If one AdvanceTo would need more steps than this, the planets are carried
    // analytically (osculating conics) for that jump instead. Dormant-tier
    // behavior: they never stop moving, they just lose mutual perturbations.
    void SetMaxStepsPerAdvance(int32 MaxSteps) { MaxStepsPerAdvance = FMath::Max(1, MaxSteps); }
    bool UsedAnalyticFallbackLastAdvance() const { return bAnalyticFallbackLastAdvance; }

    // Promotes a moon to its own N-body particle (true) or back to an analytic conic (false).
    void SetMoonUsesNBody(int32 MoonIndex, bool bUseNBody);
    bool DoesBodyUseNBody(int32 BodyIndex) const;

    double ComputeTotalEnergy() const;

private:
    struct FParticle
    {
        int32 BodyIndex = INDEX_NONE;       // Sun, planet (system barycenter), or promoted moon
        TArray<int32> DormantMoons;         // planet particles only: moons folded into this particle
    };

    void RebuildParticles(const TArray<int32>& NBodyMoons);
    std::vector<FMassiveBodyState> BuildParticleStates(const TArray<FBodyInstantState>& FromBodyStates) const;
    void SeedAtJ2000();
    void LoadIntoIntegrator(const std::vector<FMassiveBodyState>& Snapshot, double Time);
    void StepIntegrator(double Delta);
    void RebracketAt(double Time, const std::vector<FMassiveBodyState>& StateAtTime);
    void JumpAnalytically(double TargetSimSeconds);
    std::vector<FMassiveBodyState> InterpolateParticles(double Time) const;
    void UpdateBodyStates(const std::vector<FMassiveBodyState>& ParticleStates, double Time);
    double ParticleMass(const FParticle& Particle) const;
    FOrbitalState DormantMoonRelativeState(int32 MoonIndex, double Time) const;

    // Current conic for each Dormant moon: the table's mean elements until the
    // moon is demoted from N-body, then refit to its state at that instant.
    TArray<FKeplerElements> MoonElements;

    const FBodyRegistry* Registry = nullptr;
    FNBodyIntegrator Integrator;
    TArray<FParticle> Particles;
    TArray<int32> ParticleIndexByBody; // INDEX_NONE for dormant moons
    TArray<FBodyInstantState> BodyStates;

    // Physics steps bracketing CurrentTime: SnapshotA at TimeA, SnapshotB at TimeA + StepSeconds.
    std::vector<FMassiveBodyState> SnapshotA;
    std::vector<FMassiveBodyState> SnapshotB;
    double TimeA = 0.0;
    double IntegratorTime = 0.0;
    double CurrentTime = 0.0;
    double StepSeconds = 3600.0;
    int32 MaxStepsPerAdvance = 4000;
    bool bAnalyticFallbackLastAdvance = false;
};
