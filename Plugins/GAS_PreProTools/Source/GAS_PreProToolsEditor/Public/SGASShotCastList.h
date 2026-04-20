#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FGASScript;

class SGASShotCastList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGASShotCastList) {}
        SLATE_ARGUMENT(FGASScript*, Script)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void Refresh();

private:
    void Rebuild();

    FGASScript* Script = nullptr;
    TSharedPtr<class SVerticalBox> ListBox;
};