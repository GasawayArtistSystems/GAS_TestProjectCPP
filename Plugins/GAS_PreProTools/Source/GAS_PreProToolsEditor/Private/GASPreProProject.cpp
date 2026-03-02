#include "GASPreProProject.h"
#include "Logging/LogMacros.h"
#include "GASPreProLog.h"

// Why: create stable defaults, assign runtime GUIDs when adding entities.


struct FGASDerivedShotRow
{
    FGuid ShotId;
    int32 StartBlockIndex = INDEX_NONE;
    int32 EndBlockIndex = INDEX_NONE;
};


FGuid UGASPreProProject::AddScene(const FString& Label, const FString& Heading)
{
    FGASScene NewScene;
    NewScene.SceneID = FGuid::NewGuid();
    NewScene.SceneLabel = Label;
    NewScene.HeadingText = Heading;

    Scenes.Add(NewScene);
    return NewScene.SceneID;
}

FGuid UGASPreProProject::AddShot()
{
    FGASShotDefinition NewShot;
    NewShot.ShotId = FGuid::NewGuid();

    ShotDefinitions.Add(NewShot);

    UE_LOG(LogGASPrePro, Warning, TEXT("DEBUG: AddShot() created Shot %s"),
        *NewShot.ShotId.ToString()
    );

    MarkDirty();
    return NewShot.ShotId;
}




void UGASPreProProject::AddShotMarker(
    const FGuid& ShotID,
    const FGuid& StartBlockID,
    const FGuid& EndBlockID,
    const FString& Notes)
{
    FGASShotDefinitionMarker Marker;
    Marker.ShotID = ShotID;
    Marker.StartBlockID = StartBlockID;
    Marker.EndBlockID = EndBlockID;
    Marker.Notes = Notes;

    ShotMarkers.Add(Marker);
}

void UGASPreProProject::MarkDirty()
{
    bIsDirty = true;
}

void UGASPreProProject::ClearDirty()
{
    bIsDirty = false;
}

bool UGASPreProProject::IsDirty() const
{
    return bIsDirty;
}


void UGASPreProProject::BuildDerivedShotList(
    TArray<FGASDerivedShotRow>& OutShots
) const
{
    OutShots.Reset();

    // --------------------------------------------------
    // Build fast lookup: BlockID -> Index
    // --------------------------------------------------
    TMap<FGuid, int32> BlockIndexById;
    BlockIndexById.Reserve(ScriptBlocks.Num());

    for (int32 Index = 0; Index < ScriptBlocks.Num(); ++Index)
    {
        BlockIndexById.Add(ScriptBlocks[Index].BlockID, Index);
    }

    // --------------------------------------------------
    // Resolve shots
    // --------------------------------------------------
    for (const FGASShotDefinition& Shot : ShotDefinitions)
    {
        const FGASShotDefinitionMarker* Marker =
            ShotMarkers.FindByPredicate(
                [&Shot](const FGASShotDefinitionMarker& M)
                {
                    return M.ShotID == Shot.ShotId;
                }
            );

        if (!Marker)
        {
            UE_LOG(
                LogGASPrePro,
                Warning,
                TEXT("[BuildDerivedShotList] Shot %s has no marker"),
                *Shot.ShotId.ToString()
            );
            continue;
        }

        const int32* StartIndex =
            BlockIndexById.Find(Marker->StartBlockID);

        const int32* EndIndex =
            BlockIndexById.Find(Marker->EndBlockID);


        if (!StartIndex || !EndIndex)
        {
            UE_LOG(
                LogGASPrePro,
                Warning,
                TEXT(
                    "[BuildDerivedShotList] Shot %s has invalid anchors "
                    "(Start=%s End=%s)"
                ),
                *Shot.ShotId.ToString(),
                *Marker->StartBlockID.ToString(),
                *Marker->EndBlockID.ToString()
            );

            continue;
        }

        FGASDerivedShotRow Row;
        Row.ShotId = Shot.ShotId;
        Row.StartBlockIndex = *StartIndex;
        Row.EndBlockIndex = *EndIndex;

        // Sanity: enforce forward range
        if (Row.EndBlockIndex < Row.StartBlockIndex)
        {
            Swap(Row.StartBlockIndex, Row.EndBlockIndex);
        }

        OutShots.Add(Row);
    }

    // --------------------------------------------------
    // Sort by script order
    // --------------------------------------------------
    OutShots.Sort(
        [](const FGASDerivedShotRow& A, const FGASDerivedShotRow& B)
        {
            return A.StartBlockIndex < B.StartBlockIndex;
        }
    );

#if WITH_EDITOR
    UE_LOG(LogGASPrePro, Verbose, TEXT("---- Derived Shot List ----"));

    for (const FGASDerivedShotRow& Row : OutShots)
    {
        UE_LOG(
            LogGASPrePro,
            Warning,
            TEXT("SHOT %s  [%d → %d]"),
            *Row.ShotId.ToString(),
            Row.StartBlockIndex,
            Row.EndBlockIndex
        );
    }
#endif

}


void UGASPreProProject::PostLoad()
{
    Super::PostLoad();

#if WITH_EDITOR
    if (ShotDefinitions.Num() == 0 && ScriptBlocks.Num() >= 6)
    {
        FGASShotDefinition Shot;
        Shot.ShotId = FGuid::NewGuid();
        ShotDefinitions.Add(Shot);

        FGASShotDefinitionMarker Marker;
        Marker.ShotID = Shot.ShotId;
        Marker.StartBlockID = ScriptBlocks[0].BlockID;
        Marker.EndBlockID = ScriptBlocks[5].BlockID;

        ShotMarkers.Add(Marker);

        UE_LOG(LogGASPrePro, Warning, TEXT("DEBUG: Injected test shot + marker"));
    }

    TArray<FGASDerivedShotRow> DebugShots;
    BuildDerivedShotList(DebugShots);
#endif


}
