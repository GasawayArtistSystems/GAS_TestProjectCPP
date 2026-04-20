#pragma once

#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "SGAS_TestWindow.h"


class UWorld;
class ACineCameraActor;

class FGAS_PreProToolsEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void MarkToolDirty();
    void ClearToolDirty();

    void OnMapOpened(const FString& Filename, bool bAsTemplate);

    //--------------CAMERA STUFF------------------------------------------------------------
    void SpawnTestCineCamera(const FString& MapName, bool bAsTemplate);
    void LogSelectedCineCameraData(const FString& MapName, bool bAsTemplate);
    void ApplyTestShotIntent(const FString& MapName, bool bAsTemplate);
    void ApplyShotCameraMapping(const FString& MapName, bool bAsTemplate);

    ACineCameraActor* ResolveCameraForShot(
        UWorld* World,
        const FName& ShotId
    );

    //---------------BLOCKING---------------------------------------------------------------
    TSharedPtr<SGAS_ScriptTab> GetPersistentScriptTab() const;

    //--------------ACTORS STUFF------------------------------------------------------------
    void LogBlockingAnchorsForShot(
        const FString& MapName,
        bool bAsTemplate
    );
    void OnWorldActorsReady(UWorld* World, const UWorld::InitializationValues IVS);

    //--------------END STUFF------------------------------------------------------------


private:
    void RegisterMenus();
    void RegisterTabSpawner();
    void UnregisterTabSpawner();

    TWeakPtr<SDockTab> MainToolDockTab;
    TSharedRef<SDockTab> SpawnMainToolTab(const FSpawnTabArgs& Args);
    TSharedRef<SDockTab> SpawnScriptTab(const FSpawnTabArgs& Args);


    TSharedPtr<SGAS_TestWindow> MainToolWindow;
    TSharedPtr<SGAS_ScriptTab> PersistentScriptTab;

};

