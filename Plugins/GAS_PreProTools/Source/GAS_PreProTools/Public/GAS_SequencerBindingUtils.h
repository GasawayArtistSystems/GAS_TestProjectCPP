#pragma once

#include "CoreMinimal.h"
#include "LevelSequence.h"
#include "Engine/World.h"

class ULevelSequence;
class UMovieScene3DTransformTrack;
class AGAS_StandInActor;
class UWorld;
class AActor;

struct GAS_PREPROTOOLS_API FGASSequencerBindingUtils
{
public:
    static UMovieScene3DTransformTrack* EnsureActorTransformTrack(
        ULevelSequence* LevelSequence,
        AActor* Actor
    );

    static void BindAllStandInsToSequence(
        UWorld* World,
        ULevelSequence* Sequence
    );

    static AGAS_StandInActor* SpawnCharacterForBlocking(
        UWorld* World,
        ULevelSequence* Sequence,
        const FString& CharacterName
    );

    static void RemoveCharacterFromSequencer(
        ULevelSequence* Sequence,
        AActor* Actor
    );

    static void ClearAllStandInActors(UWorld* World);

    static void ClearAllSequencerBindings(ULevelSequence* LevelSequence);

    static bool IsActorAlreadyBound(ULevelSequence* Sequence, AActor* Actor);
};