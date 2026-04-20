#pragma once

#include "CoreMinimal.h"
#include "ScriptModel.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGASShotFactory, Log, All);

class UGASPreProProject;
class ULevelSequence;
class AActor;

struct FGASMarker;
struct FGASShotDefinition;

class FGASShotFactory
{
public:

    // --------------------------------------------
    // Shot creation
    // --------------------------------------------

    static FGuid CreateShotFromBlocking(
        UGASPreProProject* Project,
        const FString& SceneId
    );

    // --------------------------------------------
    // Shot production helpers
    // --------------------------------------------

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