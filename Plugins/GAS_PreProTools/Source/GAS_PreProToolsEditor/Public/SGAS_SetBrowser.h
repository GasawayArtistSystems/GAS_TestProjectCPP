#pragma once

#include "SGAS_ScriptTab.h"
#include "GAS_SetDiscovery.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// Delegate fired when user selects a set
DECLARE_DELEGATE_OneParam(FOnSetSelected, const FGASSetDescriptor&);

class SGAS_SetBrowser : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGAS_SetBrowser) {}
        SLATE_ARGUMENT(TSharedPtr<SGAS_ScriptTab>, OwnerScriptTab)
        SLATE_EVENT(FOnSetSelected, OnSetSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:

    TWeakPtr<SGAS_ScriptTab> OwnerScriptTab;
    FOnSetSelected OnSetSelected;
};