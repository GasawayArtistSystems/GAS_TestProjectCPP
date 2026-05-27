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
        "InputCore",
        "GAS_TestProjectCPP",
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

        // Editor systems
        "UnrealEd",
        "EditorSubsystem",
        "LevelEditor",
        "AssetRegistry",
        "AssetTools",

        // Sequencer
        "LevelSequence",
        "MovieScene",
        "MovieSceneTracks",
        "Sequencer",
        "LevelSequenceEditor",

        // Movie Render Queue
        "MovieRenderPipelineCore",
        "MovieRenderPipelineEditor",
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
