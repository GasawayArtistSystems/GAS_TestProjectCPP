using UnrealBuildTool;

public class GAS_PreProToolsEditor : ModuleRules
{
    public GAS_PreProToolsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GAS_PreProTools",
            "Slate",
            "SlateCore",
            "EditorSubsystem",
            "UnrealEd",
            "InputCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "ApplicationCore",
            "DesktopPlatform",
            "EditorFramework",
            "ToolMenus",
            "Json",
            "JsonUtilities"
        });

        PublicIncludePaths.AddRange(new string[]
        {
    ModuleDirectory + "/Public"
        });

        PrivateIncludePaths.AddRange(new string[]
        {
    ModuleDirectory + "/Private",
    ModuleDirectory + "/Private/ShotList"
        });

    }
}
