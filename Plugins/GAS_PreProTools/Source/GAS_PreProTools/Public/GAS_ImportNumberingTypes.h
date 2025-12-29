#pragma once

#include "CoreMinimal.h"
#include "GAS_ImportNumberingTypes.generated.h"

// ------------------------------------------------------------
// Numbering Styles
// ------------------------------------------------------------

UENUM()
enum class EGASNumberingStyle : uint8
{
    CountBy10 UMETA(DisplayName = "Count by 10 (10, 20, 30)"),
    CountBy1  UMETA(DisplayName = "Count by 1 (1, 2, 3)"),
    Alphabetic UMETA(DisplayName = "Alphabetic (A, B, C)"),

    // Special case: use existing numbers found in the script
    FromScript UMETA(DisplayName = "Use numbers from script")
};

// ------------------------------------------------------------
// Import Numbering Options
// ------------------------------------------------------------

USTRUCT()
struct FGASImportNumberingOptions
{
    GENERATED_BODY()

    // ----------------------------------------------------
    // Scene numbering
    // ----------------------------------------------------
    UPROPERTY()
    bool bScriptHasSceneNumbers = false;

    UPROPERTY()
    EGASNumberingStyle SceneNumberingStyle = EGASNumberingStyle::CountBy10;

    // ----------------------------------------------------
    // Shot numbering (NEW)
    // ----------------------------------------------------
    UPROPERTY()
    EGASShotNumberingPolicy ShotNumberingPolicy =
        EGASShotNumberingPolicy::Numeric_1s;
};

