#pragma once

#include "Modules/ModuleManager.h"

class FGAS_PreProToolsEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void RegisterTabSpawner();
    void UnregisterTabSpawner();
};

