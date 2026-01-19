#pragma once

#include "CoreMinimal.h"
#include "ScriptModel.h"

// ------------------------------------------------------------
// Shot List Output Item
// Read-only, derived, no UI concerns
// ------------------------------------------------------------
struct FGASShotListItem
{
    // Marker identity
    FString MarkerId;

    // Scene context (SceneHeading BlockId)
    FString SceneBlockId;

    // Display labels (already resolved)
    FString ShotLabel;
    FString SceneLabel;

    // Absolute order in script
    int32 SortIndex = INDEX_NONE;
};


// ------------------------------------------------------------
// Shot List Builder
// Authoritative read-only derivation from FGASScript
// ------------------------------------------------------------
class FGASShotMarkerListBuilder
{
public:
    // Build a flat, ordered shot list from script markers
    static TArray<FGASShotListItem> Build(const FGASScript& Script);
};
