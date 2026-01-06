#pragma once

#include "CoreMinimal.h"
#include "GAS_NumberingEnums.h"

#include "GAS_ImportNumberingTypes.generated.h"

// ------------------------------------------------------------
// Numbering Styles
// ------------------------------------------------------------

USTRUCT()
struct FGASImportNumberingOptions
{
    GENERATED_BODY()

    // ----------------------------------------------------
    // Scene numbering
    // ----------------------------------------------------
    UPROPERTY()
    bool bRespectScriptSceneNumbers = false;


    UPROPERTY()
    EGASNumberingStyle SceneNumberingStyle = EGASNumberingStyle::CountBy10;

    // ----------------------------------------------------
    // Shot numbering (NEW)
    // ----------------------------------------------------
    UPROPERTY()
    EGASShotNumberingPolicy ShotNumberingPolicy =
        EGASShotNumberingPolicy::Numeric_1s;
};

