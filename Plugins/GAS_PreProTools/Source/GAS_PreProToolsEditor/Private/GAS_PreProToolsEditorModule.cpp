#include "GAS_PreProToolsEditorModule.h"
#include "GAS_PreProToolsEditorCommands.h"
#include "GAS_SetDiscovery.h"
#include "GAS_PreProToolsStyle.h"
#include "SGAS_SetBrowser.h"
#include "SGAS_TestWindow.h"
#include "SGAS_ScriptTab.h"
#include "GASPreProLog.h"

#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"

#include "ToolMenus.h"
#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "GAS_BlockingAnchorActor.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/World.h"
#include "Engine/Selection.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "FileHelpers.h"



IMPLEMENT_MODULE(FGAS_PreProToolsEditorModule, GAS_PreProToolsEditor)

static const FName GAS_SetBrowserTabName(TEXT("GAS_SetBrowser"));


void FGAS_PreProToolsEditorModule::StartupModule()
{
    UE_LOG(LogGASPrePro, Warning, TEXT("GAS_PreProToolsEditor: StartupModule"));

    RegisterTabSpawner();
    FGAS_PreProToolsStyle::Initialize();

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        RegisterMenus();
    }
    else
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(
                this,
                &FGAS_PreProToolsEditorModule::RegisterMenus
            )
        );
    }

    FEditorDelegates::OnMapOpened.AddRaw(
        this,
        &FGAS_PreProToolsEditorModule::OnMapOpened
    );

    FWorldDelegates::OnPostWorldInitialization.AddRaw(
        this,
        &FGAS_PreProToolsEditorModule::OnWorldActorsReady
    );

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        GAS_SetBrowserTabName,
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            {
                return SNew(SDockTab)
                    .TabRole(ETabRole::NomadTab)
                    [
                        SNew(SGAS_SetBrowser)
                    ];
            })
    )
        .SetDisplayName(FText::FromString("GAS Sets"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

}


void FGAS_PreProToolsEditorModule::ShutdownModule()
{
    UE_LOG(LogGASPrePro, Warning, TEXT("GAS_PreProToolsEditor: ShutdownModule"));

    FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);

    FEditorDelegates::OnMapOpened.RemoveAll(this);

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

    Section.AddMenuEntry(
        "OpenGASSetBrowser",
        FText::FromString("GAS Sets"),
        FText::FromString("Open the GAS Set Browser"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(
                    FName("GAS_SetBrowser")
                );
            }))
    );

}

void FGAS_PreProToolsEditorModule::RegisterTabSpawner()
{
    // Main window tab
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        "GAS_TestWindow",
        FOnSpawnTab::CreateRaw(this, &FGAS_PreProToolsEditorModule::SpawnMainToolTab)
    )
        .SetDisplayName(FText::FromString("GAS Pre Pro Tools"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);


    // Script Tab (PERSISTENT POINTER VERSION)
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        "GAS_ScriptTab",
        FOnSpawnTab::CreateRaw(this, &FGAS_PreProToolsEditorModule::SpawnScriptTab)
    )
        .SetDisplayName(FText::FromString("GAS Script"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
}


TSharedRef<SDockTab> FGAS_PreProToolsEditorModule::SpawnMainToolTab(
    const FSpawnTabArgs& Args)
{
    TSharedPtr<SGAS_TestWindow> TestWindow;

    TSharedRef<SDockTab> DockTab =
        SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SAssignNew(TestWindow, SGAS_TestWindow)
        ];

    MainToolDockTab = DockTab;

    UE_LOG(LogGASPrePro, Verbose, TEXT("MAIN TOOL TAB: DockTab captured"));

    return DockTab;
}

TSharedRef<SDockTab> FGAS_PreProToolsEditorModule::SpawnScriptTab(
    const FSpawnTabArgs& Args)
{
    TSharedRef<SDockTab> DockTab =
        SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SAssignNew(PersistentScriptTab, SGAS_ScriptTab)
        ];

    return DockTab;
}

TSharedPtr<SGAS_ScriptTab> FGAS_PreProToolsEditorModule::GetPersistentScriptTab() const
{
    return PersistentScriptTab;
}



void FGAS_PreProToolsEditorModule::MarkToolDirty()
{

    if (MainToolDockTab.IsValid())
    {
        MainToolDockTab.Pin()->SetLabel(
            FText::FromString(TEXT("GAS Pre Pro Tools*"))
        );
    }
    else
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("MAIN TOOL TAB: DockTab INVALID"));
    }
}

void FGAS_PreProToolsEditorModule::ClearToolDirty()
{

    if (MainToolDockTab.IsValid())
    {
        MainToolDockTab.Pin()->SetLabel(
            FText::FromString(TEXT("GAS Pre Pro Tools"))
        );
    }
}


//------------------------------------------------------------------------------------------
//  Code for Actors
//------------------------------------------------------------------------------------------

void FGAS_PreProToolsEditorModule::LogBlockingAnchorsForShot(
    const FString& /*MapName*/,
    bool /*bAsTemplate*/
)
{
#if WITH_EDITOR
    if (!GEditor)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    const FName TargetShotId(TEXT("SHOT_010"));

    UE_LOG(
        LogGASPrePro,
        Warning,
        TEXT("---- Blocking Anchors for Shot %s ----"),
        *TargetShotId.ToString()
    );

    for (TActorIterator<AGAS_BlockingAnchorActor> It(World); It; ++It)
    {
        AGAS_BlockingAnchorActor* Anchor = *It;
        if (!Anchor)
        {
            continue;
        }

        UE_LOG(
            LogGASPrePro,
            Warning,
            TEXT("[FOUND ANCHOR] Label=%s | ShotId='%s' | CharacterId='%s'"),
            *Anchor->GetActorLabel(),
            *Anchor->ShotId.ToString(),
            *Anchor->CharacterId.ToString()
        );

        if (Anchor->ShotId != TargetShotId)
        {
            continue;
        }

        UE_LOG(
            LogGASPrePro,
            Warning,
            TEXT("[MATCH] Character %s at %s"),
            *Anchor->CharacterId.ToString(),
            *Anchor->GetActorLocation().ToString()
        );
    }

#endif
}


void FGAS_PreProToolsEditorModule::OnWorldActorsReady(
    UWorld* World,
    const UWorld::InitializationValues /*IVS*/
)
{
#if WITH_EDITOR
    if (!World || !World->IsEditorWorld())
    {
        return;
    }

    if (!PersistentScriptTab.IsValid())
    {
        return;
    }

    FGASScript* Script =
        PersistentScriptTab.IsValid()
        ? PersistentScriptTab->GetCurrentScript()
        : nullptr;

    if (!Script)
    {
        return;
    }

    for (TActorIterator<AGAS_BlockingAnchorActor> It(World); It; ++It)
    {
        AGAS_BlockingAnchorActor* Anchor = *It;

        if (!Anchor)
        {
            continue;
        }

        for (const FGASMarker& Marker : Script->Markers)
        {
            if (Marker.MarkerType == EGASMarkerType::ShotMarker)
            {
                Anchor->SetCurrentShotID(Marker.MarkerGuid);

                UE_LOG(LogGASPrePro, Warning,
                    TEXT("Auto-selected ShotID: %s"),
                    *Anchor->GetCurrentShotID().ToString());

                break;
            }
        }

        break; // only first anchor
    }
#endif
}


void FGAS_PreProToolsEditorModule::OnMapOpened(
    const FString& Filename,
    bool /*bAsTemplate*/
)
{
#if WITH_EDITOR
    if (!PersistentScriptTab.IsValid())
    {
        return;
    }

    FGASScript* Script =
        PersistentScriptTab->GetCurrentScript();

    if (!Script)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AGAS_BlockingAnchorActor> It(World); It; ++It)
    {
        AGAS_BlockingAnchorActor* Anchor = *It;
        if (!Anchor)
        {
            continue;
        }

        for (const FGASMarker& Marker : Script->Markers)
        {
            if (Marker.MarkerType == EGASMarkerType::ShotMarker)
            {
                Anchor->SetCurrentShotID(Marker.MarkerGuid);

                UE_LOG(LogGASPrePro, Warning,
                    TEXT("Auto-selected ShotID: %s"),
                    *Anchor->GetCurrentShotID().ToString());

                break;
            }
        }

        break;
    }
#endif
}

void FGAS_PreProToolsEditorModule::UnregisterTabSpawner()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("GAS_TestWindow");
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("GAS_ScriptTab");
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GAS_SetBrowserTabName);
}


