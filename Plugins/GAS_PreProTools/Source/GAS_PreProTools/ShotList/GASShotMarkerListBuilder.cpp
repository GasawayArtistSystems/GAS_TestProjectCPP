#include "GASShotMarkerListBuilder.h"

#include "GAS_PreProTools/Public/GAS_SceneNumberResolver.h"

// ------------------------------------------------------------
// Build a flat, ordered shot list from script markers
// ------------------------------------------------------------
TArray<FGASShotListItem> FGASShotMarkerListBuilder::Build(const FGASScript& Script)
{
    TArray<FGASShotListItem> Out;

    // Resolve scene labels once (read-only)
    const TMap<FString, FString> SceneLabelMap =
        FGASSceneNumberResolver::ResolveSceneNumbers(
            Script.Blocks,
            Script.SceneNumbering
        );

    // ------------------------------------------------------------
    // Build SceneBlockId -> SceneOrder (script order)
    // ------------------------------------------------------------
    TMap<FString, int32> SceneOrder;
    int32 SceneCounter = 0;

    for (const FGASBlock& Block : Script.Blocks)
    {
        if (Block.Type == EGASBlockType::SceneHeading)
        {
            SceneOrder.Add(Block.Id, SceneCounter++);
        }
    }

    // ------------------------------------------------------------
    // Collect shot markers as items
    // ------------------------------------------------------------
    for (const FGASMarker& Marker : Script.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        FGASShotListItem Item;
        Item.MarkerId = Marker.Id;
        Item.SceneBlockId = Marker.BlockId;

        // Scene label (if available)
        if (const FString* SceneLabel = SceneLabelMap.Find(Marker.BlockId))
        {
            Item.SceneLabel = *SceneLabel;
        }
        else
        {
            Item.SceneLabel = TEXT("");
        }

        int32 SceneIndex = INDEX_NONE;

        for (int32 i = 0; i < Script.Blocks.Num(); ++i)
        {
            if (Script.Blocks[i].Id == Marker.BlockId)
            {
                SceneIndex = i;
                break;
            }
        }

        Item.ShotLabel =
            ResolveShotDisplayLabel(
                Script,
                SceneIndex,
                Marker.ShotIndex
            );  

        // TEMP: stash numeric shot order in SortIndex for now (we’ll rewrite after sort)
        Item.SortIndex = Marker.ShotIndex;

        Out.Add(MoveTemp(Item));
    }

    // ------------------------------------------------------------
    // Sort: Scene order ASC, ShotIndex ASC
    // ------------------------------------------------------------
    Out.Sort([&SceneOrder](const FGASShotListItem& A, const FGASShotListItem& B)
        {
            const int32 SceneA = SceneOrder.Contains(A.SceneBlockId) ? SceneOrder[A.SceneBlockId] : INT_MAX;
            const int32 SceneB = SceneOrder.Contains(B.SceneBlockId) ? SceneOrder[B.SceneBlockId] : INT_MAX;

            if (SceneA != SceneB)
            {
                return SceneA < SceneB;
            }

            // NOTE: we used SortIndex as temporary "ShotIndex"
            return A.SortIndex < B.SortIndex;
        });

    // ------------------------------------------------------------
    // Re-assign stable sequential SortIndex for UI display / row order
    // ------------------------------------------------------------
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        Out[i].SortIndex = i;
    }

    return Out;
}

