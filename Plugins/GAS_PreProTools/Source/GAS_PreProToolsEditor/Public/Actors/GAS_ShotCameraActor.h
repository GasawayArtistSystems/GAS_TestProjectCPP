#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CineCameraComponent.h"
#include "GAS_ShotCameraActor.generated.h"

UCLASS()
class GAS_PREPROTOOLSEDITOR_API AGAS_ShotCameraActor : public AActor
{
    GENERATED_BODY()

public:
    AGAS_ShotCameraActor();

    // --------------------------------------------------
    // Shot Identity
    // --------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    FGuid ShotGuid;

    // --------------------------------------------------
    // Camera Component
    // --------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UCineCameraComponent* CineCamera;

    UPROPERTY()
    FString MarkerId;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShotCameraMoved, const FString& /*MarkerId*/, const FTransform& /*NewTransform*/);

    FOnShotCameraMoved OnShotCameraMoved;

#if WITH_EDITOR
    virtual void PostEditMove(bool bFinished) override;
#endif
};