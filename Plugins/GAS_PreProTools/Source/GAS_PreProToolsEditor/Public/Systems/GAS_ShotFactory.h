#pragma once

#include "CoreMinimal.h"

class UGASPreProProject;
class ULevelSequence;
class UWorld;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogGASShotFactory, Log, All);

struct FGASShotFactory
{
    static FGuid CreateShotFromBlocking(
        UGASPreProProject* Project,
        const FString& SceneId
    );

    static ULevelSequence* CreateShotSequence(
        UGASPreProProject* Project,
        const FGuid& ShotId
    );

    static AActor* CreateCameraForShot(
        UWorld* World,
        const FGuid& ShotId
    );

    static void BindActorsToShot(
        ULevelSequence* Sequence,
        const TArray<AActor*>& Actors
    );
};