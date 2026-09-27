using UnrealBuildTool;
using System.Collections.Generic;

public class AstroVerseEditorTarget : TargetRules
{
    public AstroVerseEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] {
            "AstroCore", "AstroBodies", "AstroTime", "AstroActivation",
            "AstroRendering", "AstroTravel", "AstroGalaxy",
            "AstroInput", "AstroUI", "AstroApp"
        });
    }
}
