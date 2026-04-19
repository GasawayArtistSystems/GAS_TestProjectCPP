using UnrealBuildTool;

public class GAS_PreProToolsEditor : ModuleRules
{
    public GAS_PreProToolsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUseUnity = false;        // 🔥 REQUIRED

        PublicDependencyModuleNames.AddRange(new string[]
        {
        "Core",
        "CoreUObject",
        "Engine",
        "GAS_PreProTools",
        "GAS_TestProjectCPP",
        "Slate",
        "SlateCore",
        "EditorSubsystem",
        "UnrealEd",
        "InputCore",
        "CinematicCamera"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        "Projects",
        "ApplicationCore",
        "DesktopPlatform",
        "EditorFramework",
        "ToolMenus",
        "Json",
        "JsonUtilities",
        "UnrealEd",
        "EditorSubsystem",
        "LevelEditor",
        "AssetRegistry",
        "AssetTools"
        });

        PrivateIncludePaths.AddRange(new string[]
        {
        ModuleDirectory + "/Private",
        ModuleDirectory + "/Private/ShotList"
        });
    }
}