using UnrealBuildTool;

public class AstroApp : ModuleRules
{
    public AstroApp(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "AstroCore",
            "AstroBodies",
            "AstroTime",
            "AstroActivation",
            "AstroRendering",
            "AstroTravel",
            "AstroGalaxy",
            "AstroInput",
            "AstroUI"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Layer rule: this module may only depend on modules in its own
        // architecture layer or below (see CLAUDE.md). Do not add a
        // dependency that points upward in the layer stack.
    }
}
