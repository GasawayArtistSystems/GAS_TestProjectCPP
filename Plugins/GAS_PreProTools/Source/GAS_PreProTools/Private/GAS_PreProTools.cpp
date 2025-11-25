#include "GAS_PreProTools.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FGAS_PreProToolsModule, GAS_PreProTools)

void FGAS_PreProToolsModule::StartupModule()
{
    UE_LOG(LogTemp, Warning, TEXT("GAS Pre Pro Tools: StartupModule"));
}

void FGAS_PreProToolsModule::ShutdownModule()
{
    UE_LOG(LogTemp, Warning, TEXT("GAS Pre Pro Tools: ShutdownModule"));
}
