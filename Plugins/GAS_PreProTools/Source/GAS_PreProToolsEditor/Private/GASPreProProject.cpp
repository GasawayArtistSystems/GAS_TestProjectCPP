#include "GASPreProProject.h"

/*-----------------------------------------
    ADD SCENE
------------------------------------------*/
FGuid UGASPreProProject::AddScene(const FString& Label, const FString& Heading)
{
    FGASScene NewScene;
    NewScene.SceneLabel = Label;
    NewScene.HeadingText = Heading;

    Scenes.Add(NewScene);
    return NewScene.SceneID;
}

/*-----------------------------------------
    ADD SHOT
------------------------------------------*/
FGuid UGASPreProProject::AddShot(const FString& Label)
{
    FGASShot NewShot;
    NewShot.ShotLabel = Label;

    Shots.Add(NewShot);
    return NewShot.ShotID;
}

/*-----------------------------------------
    ADD SHOT MARKER
------------------------------------------*/
void UGASPreProProject::AddShotMarker(
    const FGuid& ShotID,
    const FGuid& StartBlockID,
    const FGuid& EndBlockID,
    const FString& Notes
)
{
    FGASShotMarker Marker;
    Marker.ShotID = ShotID;
    Marker.StartBlockID = StartBlockID;
    Marker.EndBlockID = EndBlockID;
    Marker.Notes = Notes;

    ShotMarkers.Add(Marker);
}
