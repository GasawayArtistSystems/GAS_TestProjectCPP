using UnrealBuildTool;

public class GAS_PreProToolsEditor : ModuleRules
{
    public GAS_PreProToolsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "Slate",
                "SlateCore",
                "EditorSubsystem",
                "EditorFramework",
                "ToolMenus",
                "LevelEditor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
        "InputCore",
        "Projects",
        "ApplicationCore",
        "EditorStyle",
        "PropertyEditor"
            }
        );



    }
}
