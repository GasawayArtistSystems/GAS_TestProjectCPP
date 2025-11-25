#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FGAS_PreProToolsStyle
{
public:

    static void Initialize();
    static void Shutdown();
    static FName GetStyleSetName();
    static const ISlateStyle& Get();

private:
    static TSharedPtr<FSlateStyleSet> StyleInstance;
};
