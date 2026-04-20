#pragma once

#include "CoreMinimal.h"
#include "GASShot.generated.h"

UENUM()
enum class EGASShotLifecycleState : uint8
{
    Draft       UMETA(DisplayName = "Draft"),
    Rehearsal   UMETA(DisplayName = "Rehearsal"),
    Locked      UMETA(DisplayName = "Locked"),
    Final       UMETA(DisplayName = "Final")
};

USTRUCT()
struct FGASShotDefinition
{
    GENERATED_BODY()

    // ----------------------------------
    // Identity
    // ----------------------------------

    UPROPERTY()
    FGuid ShotId;


    UPROPERTY()
    FString SceneId;

    // ----------------------------------
    // Script Anchors (authoritative)
    // ----------------------------------

    UPROPERTY()
    FString StartAnchorId;

    UPROPERTY()
    FString EndAnchorId;

    // ----------------------------------
    // Lifecycle
    // ----------------------------------

    UPROPERTY()
    EGASShotLifecycleState LifecycleState = EGASShotLifecycleState::Draft;
};
