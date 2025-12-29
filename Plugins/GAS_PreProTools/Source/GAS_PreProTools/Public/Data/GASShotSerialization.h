#pragma once

#include "CoreMinimal.h"
#include "Data/GASShot.h"

struct FGASShotSerialization
{
    static bool SerializeShotsToJsonString(
        const TArray<FGASShotDefinition>& Shots,
        FString& OutJsonString
    );

    static bool DeserializeShotsFromJsonString(
        const FString& JsonString,
        TArray<FGASShotDefinition>& OutShots
    );
};

