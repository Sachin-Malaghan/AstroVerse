using UnrealBuildTool;

public class AstroUI : ModuleRules
{
    public AstroUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "Slate",
            "SlateCore",
            "DeveloperSettings",
            "AstroCore",
            "AstroTime",
            "AstroBodies",
            "AstroActivation",
            "AstroTravel",
            "AstroGalaxy",
            "AstroRendering",  // Presentation layer (below Application): site overlay data
            "AstroInput"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Layer rule: this module may only depend on modules in its own
        // architecture layer or below (see CLAUDE.md). Do not add a
        // dependency that points upward in the layer stack.
    }
}
