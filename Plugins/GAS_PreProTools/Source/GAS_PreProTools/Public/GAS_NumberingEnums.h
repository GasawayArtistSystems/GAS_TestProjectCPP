#pragma once
#include "CoreMinimal.h"

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
