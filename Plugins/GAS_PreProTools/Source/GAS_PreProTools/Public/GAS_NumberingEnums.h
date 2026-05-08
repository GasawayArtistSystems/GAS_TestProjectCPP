#pragma once
#include "CoreMinimal.h"
#include "GAS_NumberingEnums.generated.h"

UENUM(BlueprintType)
enum class EGASNumberingStyle : uint8
{
    CountBy10,
    CountBy1,
    Alphabetic,
    FromScript
};

UENUM(BlueprintType)
enum class EGASShotNumberingPolicy : uint8
{
    Numeric_1s,
    Numeric_10s,
    Alphabetic
};

UENUM(BlueprintType)
enum class EGASProjectFrameRate : uint8
{
    FPS_23_976,
    FPS_24,
    FPS_25,
    FPS_29_97,
    FPS_30
};