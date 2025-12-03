using UnrealBuildTool;

public class GAS_TestProjectCPP : ModuleRules
{
    public GAS_TestProjectCPP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "Slate",
            "SlateCore",

            // ADD THIS — your plugin module
            "GAS_PreProTools"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "JsonUtilities"
        });
    }
}
