// See CLAUDE.md Phase 2.
#include "SolarSystemSimulation.h"
#include "Math/AstroConstants.h"

namespace
{
    // Cubic Hermite on position using both endpoint velocities: exact for
    // constant acceleration, so interpolated motion is visually indistinguishable from integration.
    FMassiveBodyState HermiteInterpolate(const FMassiveBodyState& A, const FMassiveBodyState& B, double S, double Dt)
    {
        const double S2 = S * S, S3 = S2 * S;
        const double H00 = 2 * S3 - 3 * S2 + 1, H10 = S3 - 2 * S2 + S, H01 = -2 * S3 + 3 * S2, H11 = S3 - S2;
        const double D00 = 6 * S2 - 6 * S, D10 = 3 * S2 - 4 * S + 1, D01 = -6 * S2 + 6 * S, D11 = 3 * S2 - 2 * S;

        FMassiveBodyState Out;
        Out.Mass = A.Mass;
        Out.Position = A.Position * H00 + A.Velocity * (H10 * Dt) + B.Position * H01 + B.Velocity * (H11 * Dt);
        Out.Velocity = (A.Position * D00 + A.Velocity * (D10 * Dt) + B.Position * D01 + B.Velocity * (D11 * Dt)) / Dt;
        return Out;
    }
}

void FSolarSystemSimulation::Initialize(const FBodyRegistry& InRegistry, double StartSimSeconds, double InStepSeconds)
{
    Registry = &InRegistry;
    StepSeconds = FMath::Max(1.0, InStepSeconds);
    BodyStates.SetNum(Registry->Num());

    MoonElements.SetNum(Registry->Num());
    for (int32 i = 0; i < Registry->Num(); ++i)
    {
        MoonElements[i] = Registry->Get(i).Elements;
    }

    SeedAtJ2000();
    RebuildParticles({});
    RebracketAt(0.0, BuildParticleStates(BodyStates));

    // Warm-up integration from the element epoch to the requested start, uncapped.
    const int32 SavedMax = MaxStepsPerAdvance;
    MaxStepsPerAdvance = MAX_int32;
    AdvanceTo(StartSimSeconds);
    MaxStepsPerAdvance = SavedMax;
}

void FSolarSystemSimulation::SeedAtJ2000()
{
    // Heliocentric seeding: planet elements describe each planet system's barycenter.
    const int32 Star = Registry->GetStarIndex();
    BodyStates[Star] = FBodyInstantState();

    for (int32 i = 0; i < Registry->Num(); ++i)
    {
        const FBodyDefinition& Body = Registry->Get(i);
        if (Body.BodyType != EAstroBodyType::Planet)
        {
            continue;
        }
        const FOrbitalState Barycenter = Registry->ComputeRelativeStateAt(i, 0.0);

        // Place moons relative to the planet, then shift the group so its barycenter matches.
        double SystemMass = Body.MassKg;
        FAstroVector3d MoonMomentPos, MoonMomentVel;
        for (int32 Moon : Body.ChildIndices)
        {
            const FOrbitalState Rel = Registry->ComputeRelativeStateAt(Moon, 0.0);
            BodyStates[Moon].Position = Rel.Position;
            BodyStates[Moon].Velocity = Rel.Velocity;
            const double MoonMass = Registry->Get(Moon).MassKg;
            SystemMass += MoonMass;
            MoonMomentPos += Rel.Position * MoonMass;
            MoonMomentVel += Rel.Velocity * MoonMass;
        }
        const FAstroVector3d PlanetPos = Barycenter.Position - MoonMomentPos / SystemMass;
        const FAstroVector3d PlanetVel = Barycenter.Velocity - MoonMomentVel / SystemMass;
        BodyStates[i] = FBodyInstantState{ PlanetPos, PlanetVel };
        for (int32 Moon : Body.ChildIndices)
        {
            BodyStates[Moon].Position += PlanetPos;
            BodyStates[Moon].Velocity += PlanetVel;
        }
    }

    // Heliocentric -> solar-system barycentric (zero net momentum, so nothing drifts).
    double TotalMass = 0.0;
    FAstroVector3d MomentPos, MomentVel;
    for (int32 i = 0; i < Registry->Num(); ++i)
    {
        const double Mass = Registry->Get(i).MassKg;
        TotalMass += Mass;
        MomentPos += BodyStates[i].Position * Mass;
        MomentVel += BodyStates[i].Velocity * Mass;
    }
    const FAstroVector3d CenterPos = MomentPos / TotalMass;
    const FAstroVector3d CenterVel = MomentVel / TotalMass;
    for (FBodyInstantState& State : BodyStates)
    {
        State.Position -= CenterPos;
        State.Velocity -= CenterVel;
    }
}

void FSolarSystemSimulation::RebuildParticles(const TArray<int32>& NBodyMoons)
{
    Particles.Reset();
    ParticleIndexByBody.Init(INDEX_NONE, Registry->Num());
    for (int32 i = 0; i < Registry->Num(); ++i)
    {
        if (Registry->Get(i).BodyType != EAstroBodyType::Moon || NBodyMoons.Contains(i))
        {
            ParticleIndexByBody[i] = Particles.Num();
            Particles.Add(FParticle{ i, {} });
        }
    }
    for (int32 i = 0; i < Registry->Num(); ++i)
    {
        const FBodyDefinition& Body = Registry->Get(i);
        if (Body.BodyType == EAstroBodyType::Moon && ParticleIndexByBody[i] == INDEX_NONE)
        {
            Particles[ParticleIndexByBody[Body.ParentIndex]].DormantMoons.Add(i);
        }
    }
}

std::vector<FMassiveBodyState> FSolarSystemSimulation::BuildParticleStates(const TArray<FBodyInstantState>& FromBodyStates) const
{
    // Planet-system particles sit at the mass-weighted barycenter of the planet and its dormant moons.
    std::vector<FMassiveBodyState> Out;
    Out.reserve(Particles.Num());
    for (const FParticle& Particle : Particles)
    {
        FMassiveBodyState State;
        State.Mass = ParticleMass(Particle);
        const double OwnMass = Registry->Get(Particle.BodyIndex).MassKg;
        FAstroVector3d WeightedPos = FromBodyStates[Particle.BodyIndex].Position * OwnMass;
        FAstroVector3d WeightedVel = FromBodyStates[Particle.BodyIndex].Velocity * OwnMass;
        for (int32 Moon : Particle.DormantMoons)
        {
            const double MoonMass = Registry->Get(Moon).MassKg;
            WeightedPos += FromBodyStates[Moon].Position * MoonMass;
            WeightedVel += FromBodyStates[Moon].Velocity * MoonMass;
        }
        State.Position = WeightedPos / State.Mass;
        State.Velocity = WeightedVel / State.Mass;
        Out.push_back(State);
    }
    return Out;
}

double FSolarSystemSimulation::ParticleMass(const FParticle& Particle) const
{
    double Mass = Registry->Get(Particle.BodyIndex).MassKg;
    for (int32 Moon : Particle.DormantMoons)
    {
        Mass += Registry->Get(Moon).MassKg;
    }
    return Mass;
}

void FSolarSystemSimulation::LoadIntoIntegrator(const std::vector<FMassiveBodyState>& Snapshot, double Time)
{
    Integrator.SetStates(Snapshot);
    IntegratorTime = Time;
}

void FSolarSystemSimulation::StepIntegrator(double Delta)
{
    Integrator.Step(Delta);
    IntegratorTime += Delta;
}

void FSolarSystemSimulation::RebracketAt(double Time, const std::vector<FMassiveBodyState>& StateAtTime)
{
    SnapshotA = StateAtTime;
    TimeA = Time;
    LoadIntoIntegrator(SnapshotA, TimeA);
    StepIntegrator(StepSeconds);
    SnapshotB = Integrator.GetStates();
    CurrentTime = Time;
    UpdateBodyStates(SnapshotA, Time);
}

void FSolarSystemSimulation::AdvanceTo(double TargetSimSeconds)
{
    bAnalyticFallbackLastAdvance = false;
    const double TimeB = TimeA + StepSeconds;
    const double StepsNeeded = TargetSimSeconds > TimeB ? (TargetSimSeconds - TimeB) / StepSeconds
                             : TargetSimSeconds < TimeA ? (TimeA - TargetSimSeconds) / StepSeconds
                             : 0.0;
    if (StepsNeeded > MaxStepsPerAdvance)
    {
        JumpAnalytically(TargetSimSeconds);
        return;
    }

    while (TargetSimSeconds > TimeA + StepSeconds)
    {
        const double NewTimeA = TimeA + StepSeconds;
        if (IntegratorTime != NewTimeA)
        {
            LoadIntoIntegrator(SnapshotB, NewTimeA);
        }
        SnapshotA = SnapshotB;
        TimeA = NewTimeA;
        StepIntegrator(StepSeconds);
        SnapshotB = Integrator.GetStates();
    }
    while (TargetSimSeconds < TimeA)
    {
        if (IntegratorTime != TimeA)
        {
            LoadIntoIntegrator(SnapshotA, TimeA);
        }
        SnapshotB = SnapshotA;
        StepIntegrator(-StepSeconds);
        SnapshotA = Integrator.GetStates();
        TimeA -= StepSeconds;
    }

    CurrentTime = TargetSimSeconds;
    UpdateBodyStates(InterpolateParticles(TargetSimSeconds), TargetSimSeconds);
}

std::vector<FMassiveBodyState> FSolarSystemSimulation::InterpolateParticles(double Time) const
{
    const double S = FMath::Clamp((Time - TimeA) / StepSeconds, 0.0, 1.0);
    std::vector<FMassiveBodyState> Out(SnapshotA.size());
    for (size_t i = 0; i < SnapshotA.size(); ++i)
    {
        Out[i] = HermiteInterpolate(SnapshotA[i], SnapshotB[i], S, StepSeconds);
    }
    return Out;
}

void FSolarSystemSimulation::JumpAnalytically(double TargetSimSeconds)
{
    bAnalyticFallbackLastAdvance = true;

    // Each non-star particle follows its osculating conic about the star from
    // its current state; the star is then placed to keep the barycenter fixed.
    const std::vector<FMassiveBodyState> Now = InterpolateParticles(CurrentTime);
    const int32 StarParticle = ParticleIndexByBody[Registry->GetStarIndex()];
    const FMassiveBodyState& StarNow = Now[StarParticle];
    const double StarGM = Registry->Get(Registry->GetStarIndex()).GM;
    const double DeltaTime = TargetSimSeconds - CurrentTime;

    std::vector<FMassiveBodyState> Next = Now;
    FAstroVector3d MomentPos, MomentVel;
    double OtherMass = 0.0;
    for (size_t i = 0; i < Now.size(); ++i)
    {
        if (static_cast<int32>(i) == StarParticle)
        {
            continue;
        }
        const double Mu = StarGM + AstroConstants::GravitationalConstant * Now[i].Mass;
        const FOrbitalState Rel{ Now[i].Position - StarNow.Position, Now[i].Velocity - StarNow.Velocity };
        const FKeplerElements Osculating = KeplerOrbit::ElementsFromState(Rel, Mu, 0.0);
        const FOrbitalState Future = Osculating.Eccentricity < 1.0 ? KeplerOrbit::StateAtTime(Osculating, Mu, DeltaTime) : Rel;
        Next[i].Position = Future.Position;
        Next[i].Velocity = Future.Velocity;
        MomentPos += Future.Position * Now[i].Mass;
        MomentVel += Future.Velocity * Now[i].Mass;
        OtherMass += Now[i].Mass;
    }
    // Heliocentric -> barycentric.
    const double TotalMass = OtherMass + StarNow.Mass;
    const FAstroVector3d CenterPos = MomentPos / TotalMass;
    const FAstroVector3d CenterVel = MomentVel / TotalMass;
    Next[StarParticle].Position = FAstroVector3d();
    Next[StarParticle].Velocity = FAstroVector3d();
    for (FMassiveBodyState& State : Next)
    {
        State.Position -= CenterPos;
        State.Velocity -= CenterVel;
    }

    RebracketAt(TargetSimSeconds, Next);
}

void FSolarSystemSimulation::UpdateBodyStates(const std::vector<FMassiveBodyState>& ParticleStates, double Time)
{
    for (int32 p = 0; p < Particles.Num(); ++p)
    {
        const FParticle& Particle = Particles[p];
        const FMassiveBodyState& State = ParticleStates[p];
        if (Particle.DormantMoons.Num() == 0)
        {
            BodyStates[Particle.BodyIndex] = FBodyInstantState{ State.Position, State.Velocity };
            continue;
        }

        // Unfold the planet-system barycenter into the planet and its analytic moons.
        FAstroVector3d MomentPos, MomentVel;
        TArray<FOrbitalState, TInlineAllocator<8>> MoonRel;
        for (int32 Moon : Particle.DormantMoons)
        {
            const FOrbitalState Rel = DormantMoonRelativeState(Moon, Time);
            MoonRel.Add(Rel);
            const double MoonMass = Registry->Get(Moon).MassKg;
            MomentPos += Rel.Position * MoonMass;
            MomentVel += Rel.Velocity * MoonMass;
        }
        const FAstroVector3d PlanetPos = State.Position - MomentPos / State.Mass;
        const FAstroVector3d PlanetVel = State.Velocity - MomentVel / State.Mass;
        BodyStates[Particle.BodyIndex] = FBodyInstantState{ PlanetPos, PlanetVel };
        for (int32 m = 0; m < Particle.DormantMoons.Num(); ++m)
        {
            BodyStates[Particle.DormantMoons[m]] = FBodyInstantState{ PlanetPos + MoonRel[m].Position, PlanetVel + MoonRel[m].Velocity };
        }
    }
}

void FSolarSystemSimulation::SetStepSeconds(double NewStepSeconds)
{
    NewStepSeconds = FMath::Max(1.0, NewStepSeconds);
    if (NewStepSeconds == StepSeconds)
    {
        return;
    }
    const std::vector<FMassiveBodyState> Now = InterpolateParticles(CurrentTime);
    StepSeconds = NewStepSeconds;
    RebracketAt(CurrentTime, Now);
}

void FSolarSystemSimulation::SetMoonUsesNBody(int32 MoonIndex, bool bUseNBody)
{
    if (!Registry || Registry->Get(MoonIndex).BodyType != EAstroBodyType::Moon || DoesBodyUseNBody(MoonIndex) == bUseNBody)
    {
        return;
    }

    // Snapshot every body at the current time, rebuild the particle set, and re-bracket.
    const TArray<FBodyInstantState> Current = BodyStates;
    if (!bUseNBody)
    {
        // Demotion: fit a conic to the moon's current parent-relative state so it
        // continues from exactly where N-body left it, instead of snapping back to the table orbit.
        const FBodyDefinition& MoonDef = Registry->Get(MoonIndex);
        const FAstroMatrix3d ToOrbitFrame = MoonDef.OrbitFrameToEcliptic.Transposed();
        const FOrbitalState RelEcliptic{
            Current[MoonIndex].Position - Current[MoonDef.ParentIndex].Position,
            Current[MoonIndex].Velocity - Current[MoonDef.ParentIndex].Velocity };
        const FOrbitalState RelLocal{ ToOrbitFrame * RelEcliptic.Position, ToOrbitFrame * RelEcliptic.Velocity };
        MoonElements[MoonIndex] = KeplerOrbit::ElementsFromState(RelLocal, Registry->GetOrbitMu(MoonIndex), CurrentTime);
    }
    TArray<int32> NBodyMoons;
    for (const FParticle& Particle : Particles)
    {
        if (Registry->Get(Particle.BodyIndex).BodyType == EAstroBodyType::Moon && Particle.BodyIndex != MoonIndex)
        {
            NBodyMoons.Add(Particle.BodyIndex);
        }
    }
    if (bUseNBody)
    {
        NBodyMoons.Add(MoonIndex);
    }

    RebuildParticles(NBodyMoons);
    RebracketAt(CurrentTime, BuildParticleStates(Current));
}

FOrbitalState FSolarSystemSimulation::DormantMoonRelativeState(int32 MoonIndex, double Time) const
{
    const FBodyDefinition& Moon = Registry->Get(MoonIndex);
    const FOrbitalState Local = KeplerOrbit::StateAtTime(MoonElements[MoonIndex], Registry->GetOrbitMu(MoonIndex), Time);
    return FOrbitalState{ Moon.OrbitFrameToEcliptic * Local.Position, Moon.OrbitFrameToEcliptic * Local.Velocity };
}

bool FSolarSystemSimulation::DoesBodyUseNBody(int32 BodyIndex) const
{
    return ParticleIndexByBody.IsValidIndex(BodyIndex) && ParticleIndexByBody[BodyIndex] != INDEX_NONE;
}

double FSolarSystemSimulation::ComputeTotalEnergy() const
{
    double Kinetic = 0.0, Potential = 0.0;
    for (int32 i = 0; i < BodyStates.Num(); ++i)
    {
        const double Mi = Registry->Get(i).MassKg;
        Kinetic += 0.5 * Mi * BodyStates[i].Velocity.LengthSquared();
        for (int32 j = i + 1; j < BodyStates.Num(); ++j)
        {
            Potential -= AstroConstants::GravitationalConstant * Mi * Registry->Get(j).MassKg
                       / (BodyStates[j].Position - BodyStates[i].Position).Length();
        }
    }
    return Kinetic + Potential;
}
