using UnrealBuildTool;
using System.IO;

public class GAS_PreProTools : ModuleRules
{
    public GAS_PreProTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUseUnity = false; // 🔥 ADD THIS

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });
    }
}