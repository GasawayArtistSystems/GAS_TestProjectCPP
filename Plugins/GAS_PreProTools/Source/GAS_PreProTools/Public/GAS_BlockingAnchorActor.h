#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GAS_BlockingAnchorActor.generated.h"

UCLASS()
class GAS_PREPROTOOLS_API AGAS_BlockingAnchorActor : public AActor
{
    GENERATED_BODY()

public:
    AGAS_BlockingAnchorActor();

    // ----------------------------------------
    // Authorial identity
    // ----------------------------------------

    UPROPERTY(EditAnywhere, Category = "GAS Blocking")
    FName ShotId;

    UPROPERTY(EditAnywhere, Category = "GAS Blocking")
    FName CharacterId;

    void SetCurrentSceneID(const FGuid& InSceneID);
    FGuid GetCurrentSceneID() const;

    void SetCurrentShotID(const FGuid& InShotID);
    FGuid GetCurrentShotID() const;

protected:
    UPROPERTY(VisibleAnywhere)
    class UArrowComponent* FacingArrow;

private:
    UPROPERTY()
    FGuid CurrentSceneID;

    UPROPERTY()
    FGuid CurrentShotID;
};
