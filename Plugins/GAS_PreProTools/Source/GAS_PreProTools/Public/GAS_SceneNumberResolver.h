#pragma once

#include "CoreMinimal.h"
#include "ScriptModel.h"

// ------------------------------------------------------------
// Scene Number Resolver (Read-only, Stateless)
// ------------------------------------------------------------
struct GAS_PREPROTOOLS_API FGASSceneNumberResolver 
{
public:
    // Computes scene labels in script order.
    // Returns: BlockId -> SceneLabel (e.g. "10", "20", "4A")
    static TMap<FString, FString> ResolveSceneNumbers(
        const TArray<FGASBlock>& Blocks,
        const FGASSceneNumberingSettings& Settings
    );
};
