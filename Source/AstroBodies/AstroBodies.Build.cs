using UnrealBuildTool;

public class AstroBodies : ModuleRules
{
    public AstroBodies(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "AstroCore",
            "AstroTime"  // same (Simulation) layer: bodies advance off the god-mode clock
        });

        PrivateDependencyModuleNames.AddRange(new string[] { "ImageWrapper" });

        // Layer rule: this module may only depend on modules in its own
        // architecture layer or below (see CLAUDE.md). Do not add a
        // dependency that points upward in the layer stack.
    }
}
