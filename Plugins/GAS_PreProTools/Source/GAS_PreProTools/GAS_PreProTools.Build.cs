using UnrealBuildTool;
using System.IO;

public class GAS_PreProTools : ModuleRules
{
    public GAS_PreProTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "JsonUtilities",

            // Cameras
            "CinematicCamera",

            // Sequencer / Level Sequence
            "LevelSequence",
            "MovieScene",
            "MovieSceneTracks"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "Slate",
            "SlateCore",
            "ApplicationCore",
            "Engine",

            // Sequencer runtime modules (linking)
            "LevelSequence",
            "MovieScene",
            "MovieSceneTracks"
        });

        PublicIncludePaths.AddRange(
            new string[]
            {
                Path.Combine(ModuleDirectory, "Public"),
                Path.Combine(ModuleDirectory, "Public/Utils")
            }
        );
    }
}