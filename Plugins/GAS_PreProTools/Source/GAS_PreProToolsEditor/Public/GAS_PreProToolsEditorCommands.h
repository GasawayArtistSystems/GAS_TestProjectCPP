#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"   // FAppStyle for icon set

class FGAS_PreProToolsEditorCommands
    : public TCommands<FGAS_PreProToolsEditorCommands>
{
public:

    FGAS_PreProToolsEditorCommands()
        : TCommands<FGAS_PreProToolsEditorCommands>(
            TEXT("GAS_PreProToolsEditor"),
            NSLOCTEXT("Contexts", "GAS_PreProToolsEditor", "GAS Pre Pro Tools Editor"),
            NAME_None,
            FAppStyle::GetAppStyleSetName()  // use default editor style
        )
    {
    }

    virtual void RegisterCommands() override;

public:
    TSharedPtr<FUICommandInfo> OpenTestWindow;
};
