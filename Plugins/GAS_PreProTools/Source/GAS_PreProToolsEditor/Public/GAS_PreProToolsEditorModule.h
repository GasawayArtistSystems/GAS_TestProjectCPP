#pragma once

#include "Modules/ModuleManager.h"

class FGAS_PreProToolsEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    void MarkToolDirty();
    void ClearToolDirty();

private:
    void RegisterMenus();
    void RegisterTabSpawner();
    void UnregisterTabSpawner();

    TWeakPtr<SDockTab> MainToolDockTab;
    TSharedRef<SDockTab> SpawnMainToolTab(const FSpawnTabArgs& Args);
};

