#pragma once

#include "CoreMinimal.h"

class ULevelSequence;
class AActor;
class UWorld;
class UMovieScene3DTransformTrack;

struct GAS_PREPROTOOLS_API FGASSequencerBindingUtils 
{
    static void ClearAllStandInActors(UWorld* World);

    static void ClearAllSequencerBindings(ULevelSequence* Sequence);

    static UMovieScene3DTransformTrack* EnsureActorTransformTrack(
        ULevelSequence* LevelSequence,
        AActor* Actor
    );

    static void BindAllStandInsToSequence(
        UWorld* World,
        ULevelSequence* Sequence
    );

    static class AGAS_StandInActor* SpawnCharacterForBlocking(
        UWorld* World,
        ULevelSequence* Sequence,
        const FString& CharacterName
    );

    static bool IsActorAlreadyBound(
        ULevelSequence* Sequence,
        AActor* Actor
    );

    static void RemoveCharacterFromSequencer(
        ULevelSequence* Sequence,
        AActor* ActorToRemove
    );
};