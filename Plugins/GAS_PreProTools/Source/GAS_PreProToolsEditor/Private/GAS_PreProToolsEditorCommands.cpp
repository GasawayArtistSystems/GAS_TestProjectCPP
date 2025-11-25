#include "GAS_PreProToolsEditorCommands.h"
#define LOCTEXT_NAMESPACE "GASPreProTools"

void FGAS_PreProToolsEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenTestWindow, "Open Test", "Opens the test window.", EUserInterfaceActionType::Button, FInputChord());
}
#undef LOCTEXT_NAMESPACE
