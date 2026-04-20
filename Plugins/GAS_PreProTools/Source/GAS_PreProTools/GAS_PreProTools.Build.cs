using UnrealBuildTool;
using System.IO;

public class GAS_PreProTools : ModuleRules
{
    public GAS_PreProTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
        "Core",
        "CoreUObject",
        "Engine",
        "Json",
        "JsonUtilities",
        "LevelSequence",
        "MovieScene",
        "MovieSceneTracks",
        "CinematicCamera"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        "Projects",
        "LevelSequence",
        "MovieScene",
        "CinematicCamera"
        });
    }
}