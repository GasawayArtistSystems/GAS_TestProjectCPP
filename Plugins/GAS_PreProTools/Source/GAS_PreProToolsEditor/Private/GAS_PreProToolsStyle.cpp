#include "GAS_PreProToolsStyle.h"
#include "Templates/SharedPointer.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"

TSharedPtr<FSlateStyleSet> FGAS_PreProToolsStyle::StyleInstance = nullptr;

void FGAS_PreProToolsStyle::Initialize()
{
    if (StyleInstance.IsValid())
        return;

    StyleInstance = MakeShareable(new FSlateStyleSet("GAS_PreProToolsStyle"));

    // Set root to plugin's Resources directory
    FString PluginBaseDir = IPluginManager::Get().FindPlugin("GAS_PreProTools")->GetBaseDir();
    StyleInstance->SetContentRoot(PluginBaseDir / TEXT("Resources"));

    // Register Courier New font
    FString FontPath = StyleInstance->RootToContentDir(TEXT("Fonts/cour.ttf"));

    // Proper UE 5.6 constructor: takes *path + size*
    FSlateFontInfo FontInfo(*FontPath, 12);

    // (Optional but recommended)
    FontInfo.TypefaceFontName = FName("Regular");

    StyleInstance->Set("GAS.ScriptFont", FontInfo);


    FButtonStyle ToolButtonStyle = FButtonStyle()
        .SetNormal(FSlateNoResource())
        .SetHovered(FSlateNoResource())
        .SetPressed(FSlateNoResource())
        .SetDisabled(FSlateNoResource())
        .SetNormalForeground(FSlateColor::UseForeground())
        .SetHoveredForeground(FSlateColor::UseForeground())
        .SetPressedForeground(FSlateColor::UseForeground())
        .SetDisabledForeground(FSlateColor::UseForeground());
   

    StyleInstance->Set("GAS.ToolButton", ToolButtonStyle);




    // Register CameraIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.CameraIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("CameraIcon_40.png")),
            FVector2D(40.0f, 40.0f)
        )
    );
    // Register SceneNumberIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.SceneNumberIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("SceneNumberIcon_40.png")),
            FVector2D(40.0f, 40.0f)
        )
    );

    // Register SaveIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.SaveIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("SaveIcon_40.png")),
            FVector2D(40.0f, 40.0f)
        )
    );

    // Register EditIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.EditIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("EditIcon_40.png")),
            FVector2D(40.f, 40.f)
        )
    );

    // Register EditAddIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.EditAddIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("EditAddIcon_40.png")),
            FVector2D(40.f, 40.f)
        )
    );

    // Register PageBreakIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.PageBreakIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("PageBreakIcon_40.png")),
            FVector2D(40.f, 40.f)
        )
    );

    // Register ClearIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.ClearIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("ClearIcon_40.png")),
            FVector2D(40.f, 40.f)
        )
    );

    // Register ImportScript_40.png as a brush
    StyleInstance->Set(
        "GAS.ImportScript",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("ImportScript_40.png")),
            FVector2D(40.f, 40.f)
        )
    );

    // Register style set
    FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FGAS_PreProToolsStyle::Shutdown()
{
    if (StyleInstance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
        StyleInstance.Reset();
    }
}

FName FGAS_PreProToolsStyle::GetStyleSetName()
{
    return "GAS_PreProToolsStyle";
}

const ISlateStyle& FGAS_PreProToolsStyle::Get()
{
    return *StyleInstance;
}
