#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GalaxyView.h"
#include "AstroScaleDomainSubsystem.generated.h"
// Solar System and Galaxy are separate scale-domains, never one coordinate space
// (~1e10 km vs ~1e18 km). This owns which one is presented and the deliberate,
// faded origin re-anchoring between them. The solar-system simulation keeps running
// while the galaxy is shown (Dormant bodies never stop). See CLAUDE.md Phase 7 / 10.

UENUM(BlueprintType)
enum class EAstroScaleDomain : uint8
{
    SolarSystem,
    Galaxy
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScaleDomainChanged, EAstroScaleDomain, NewDomain);

UCLASS()
class ASTROGALAXY_API UAstroScaleDomainSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    static UAstroScaleDomainSubsystem* Get(const UObject* WorldContext);

    // Starts a faded transition; ignored if already there or mid-transition.
    UFUNCTION(BlueprintCallable, Category = "Astro|ScaleDomain")
    void RequestDomain(EAstroScaleDomain Target);

    UFUNCTION(BlueprintPure, Category = "Astro|ScaleDomain")
    EAstroScaleDomain GetDomain() const { return Domain; }

    UFUNCTION(BlueprintPure, Category = "Astro|ScaleDomain")
    bool IsTransitioning() const { return Phase != EPhase::Idle; }

    UPROPERTY(BlueprintAssignable, Category = "Astro|ScaleDomain")
    FOnScaleDomainChanged OnDomainChanged;

    UPROPERTY(EditAnywhere, Category = "Astro|ScaleDomain")
    float FadeSeconds = 0.6f;

    FGalaxyView& GetGalaxyView() { return GalaxyView; }

private:
    enum class EPhase : uint8 { Idle, FadingOut, FadingIn };

    void Switch();
    void SetSolarSystemVisible(bool bVisible);
    void StartFade(float From, float To);

    EAstroScaleDomain Domain = EAstroScaleDomain::SolarSystem;
    EAstroScaleDomain PendingDomain = EAstroScaleDomain::SolarSystem;
    EPhase Phase = EPhase::Idle;
    float PhaseTime = 0.0f;
    FGalaxyView GalaxyView;
};
