using UnrealBuildTool;

public class AstroRendering : ModuleRules
{
    public AstroRendering(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "HeadMountedDisplay",
            "RenderCore",
            "RHI",
            "Niagara",
            "ProceduralMeshComponent",
            "AstroCore",
            "AstroBodies",
            "AstroActivation"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Layer rule: this module may only depend on modules in its own
        // architecture layer or below (see CLAUDE.md). Do not add a
        // dependency that points upward in the layer stack.
    }
}
