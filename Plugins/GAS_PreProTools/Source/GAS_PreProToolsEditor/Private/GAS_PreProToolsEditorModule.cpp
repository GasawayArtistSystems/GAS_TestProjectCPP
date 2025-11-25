#include "GAS_PreProToolsEditorModule.h"
#include "GAS_PreProToolsEditorCommands.h"

#include "SGAS_TestWindow.h"
#include "SGAS_ScriptTab.h"

#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "GAS_PreProToolsStyle.h"


IMPLEMENT_MODULE(FGAS_PreProToolsEditorModule, GAS_PreProToolsEditor)

void FGAS_PreProToolsEditorModule::StartupModule()
{
    UE_LOG(LogTemp, Warning, TEXT("GAS_PreProToolsEditor: StartupModule"));

    RegisterTabSpawner();
    FGAS_PreProToolsStyle::Initialize();


    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this,
            &FGAS_PreProToolsEditorModule::RegisterMenus
        )
    );
}

void FGAS_PreProToolsEditorModule::ShutdownModule()
{
    UE_LOG(LogTemp, Warning, TEXT("GAS_PreProToolsEditor: ShutdownModule"));

    UnregisterTabSpawner();
    FGAS_PreProToolsStyle::Shutdown();


    UToolMenus::UnregisterOwner(this);
}

void FGAS_PreProToolsEditorModule::RegisterMenus()
{
    // Extend the Window menu
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");

    // Create a new section called "GAS Tools"
    FToolMenuSection& Section = Menu->AddSection(
        "GASPreProToolsSection",
        FText::FromString("GAS Tools")
    );

    // Main Test Window
    Section.AddMenuEntry(
        "OpenTestWindow",
        FText::FromString("Main GAS Window"),
        FText::FromString("Opens the GAS Pre Pro main window."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(FName("GAS_TestWindow"));
            }))
    );

    // Standalone Script Tab
    Section.AddMenuEntry(
        "OpenScriptTab",
        FText::FromString("Script Tab"),
        FText::FromString("Opens the GAS Pre Pro Script tab."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(FName("GAS_ScriptTab"));
            }))
    );
}

void FGAS_PreProToolsEditorModule::RegisterTabSpawner()
{
    // Main window tab
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        "GAS_TestWindow",
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
            {
                return SNew(SDockTab)
                    .TabRole(ETabRole::NomadTab)
                    [
                        SNew(SGAS_TestWindow)
                    ];
            })
    )
        .SetDisplayName(FText::FromString("GAS Pre Pro Tools"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    // Standalone Script tab
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        "GAS_ScriptTab",
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
            {
                return SNew(SDockTab)
                    .TabRole(ETabRole::NomadTab)
                    [
                        SNew(SGAS_ScriptTab)
                    ];
            })
    )
        .SetDisplayName(FText::FromString("GAS Script"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
}


void FGAS_PreProToolsEditorModule::UnregisterTabSpawner()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("GAS_TestWindow");
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("GAS_ScriptTab");
}

