#include "GASPreProProject.h"

// Why: create stable defaults, assign runtime GUIDs when adding entities.

FGuid UGASPreProProject::AddScene(const FString& Label, const FString& Heading)
{
    FGASScene NewScene;
    NewScene.SceneID = FGuid::NewGuid();
    NewScene.SceneLabel = Label;
    NewScene.HeadingText = Heading;

    Scenes.Add(NewScene);
    return NewScene.SceneID;
}

FGuid UGASPreProProject::AddShot(const FString& Label)
{
    FGASShot NewShot;
    NewShot.ShotID = FGuid::NewGuid();
    NewShot.ShotLabel = Label;

    Shots.Add(NewShot);
    return NewShot.ShotID;
}

void UGASPreProProject::AddShotMarker(
    const FGuid& ShotID,
    const FGuid& StartBlockID,
    const FGuid& EndBlockID,
    const FString& Notes)
{
    FGASShotMarker Marker;
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