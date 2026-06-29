#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

class UWorld;

class GAS_PREPROTOOLS_API FGASStageLightingUtils
{
public:
    static void SetupDefaultStageLighting(UWorld* World);
};