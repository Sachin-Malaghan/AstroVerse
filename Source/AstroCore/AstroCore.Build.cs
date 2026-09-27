using UnrealBuildTool;

public class AstroCore : ModuleRules
{
    public AstroCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Layer rule: this module may only depend on modules in its own
        // architecture layer or below (see CLAUDE.md). Do not add a
        // dependency that points upward in the layer stack.
    }
}
