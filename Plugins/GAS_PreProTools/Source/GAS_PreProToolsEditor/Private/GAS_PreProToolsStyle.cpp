#include "GAS_PreProToolsStyle.h"
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

    // Register CameraIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.CameraIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("CameraIcon_40.png")),
            FVector2D(40.0f, 40.0f)
        )
    );
    // Register NumberIcon_40.png as a brush
    StyleInstance->Set(
        "GAS.NumberIcon",
        new FSlateImageBrush(
            StyleInstance->RootToContentDir(TEXT("NumberIcon_40.png")),
            FVector2D(40.0f, 40.0f)
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
