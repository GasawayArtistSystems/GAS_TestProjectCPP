#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GAS_StageCenterActor.generated.h"

class UGASStageCenterVizComponent;

UCLASS(NotBlueprintable, NotPlaceable)
class GAS_PREPROTOOLS_API AGAS_StageCenterActor : public AActor
{
	GENERATED_BODY()

public:
	AGAS_StageCenterActor();

	FVector GetDefaultActorSpawnLocation() const;


#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostRegisterAllComponents() override;
#endif

private:
	void EnforceWorldLock();

private:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<USceneComponent> Root;

};
