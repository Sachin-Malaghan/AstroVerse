using UnrealBuildTool;
using System.Collections.Generic;

public class AstroVerseTarget : TargetRules
{
    public AstroVerseTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] {
            "AstroCore", "AstroBodies", "AstroTime", "AstroActivation",
            "AstroRendering", "AstroTravel", "AstroGalaxy",
            "AstroInput", "AstroUI", "AstroApp"
        });
    }
}
