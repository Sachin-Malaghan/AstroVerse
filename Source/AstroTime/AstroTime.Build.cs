using UnrealBuildTool;

public class AstroTime : ModuleRules
{
    public AstroTime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "AstroCore"
        });

        // Live clock: network UTC via the HTTP "Date" header (engine module, not a layer change).
        PrivateDependencyModuleNames.AddRange(new string[] {
            "HTTP"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Layer rule: this module may only depend on modules in its own
        // architecture layer or below (see CLAUDE.md). Do not add a
        // dependency that points upward in the layer stack.
    }
}
