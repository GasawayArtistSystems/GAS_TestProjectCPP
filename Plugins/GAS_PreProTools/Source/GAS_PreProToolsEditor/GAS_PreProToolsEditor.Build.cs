using UnrealBuildTool;

public class GAS_PreProToolsEditor : ModuleRules
{
    public GAS_PreProToolsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUseUnity = false;
        PublicIncludePaths.Add(ModuleDirectory + "/Public");

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
        "LevelSequence",
        "MovieScene",
        "MovieSceneTracks",
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
        "AssetTools",
        "LevelSequence",
        "LevelSequenceEditor",
        "MovieScene",
        "MovieSceneTracks",
        "CinematicCamera",
        "GAS_PreProTools"
        });

        PrivateIncludePaths.AddRange(new string[]
        {
        ModuleDirectory + "/Private",
        ModuleDirectory + "/Private/ShotList"
        });
    }
}