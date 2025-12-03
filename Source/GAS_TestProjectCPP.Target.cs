using UnrealBuildTool;
using System.Collections.Generic;

public class GAS_TestProjectCPPTarget : TargetRules
{
    public GAS_TestProjectCPPTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;

        // Must match what UE5.6 expects
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;

        ExtraModuleNames.Add("GAS_TestProjectCPP");
    }
}
