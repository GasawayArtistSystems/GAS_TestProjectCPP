#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GAS_SetActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USceneComponent;


UCLASS()
class GAS_TESTPROJECTCPP_API AGAS_SetActor : public AActor
{
	GENERATED_BODY()

public:
	AGAS_SetActor();

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void LoadSetById(FName InSetId);

protected:
	// -------------------------
	// Components
	// -------------------------
	UPROPERTY()
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS|Set", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* Set_Footprint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS|Set", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Set_Footprint_Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS|Set", meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* Set_Label;

public:
	// -------------------------
	// Data
	// -------------------------
	UPROPERTY(EditAnywhere, Category = "GAS|Set")
	FName SetName = NAME_None;

	// Single authoritative representation: FULL size in cm (X=Width, Y=Depth, Z=Thickness)
	UPROPERTY(EditAnywhere, Category = "GAS|Set|Footprint", meta = (ClampMin = "1.0", UIMin = "1.0"))
	FVector FootprintDimensions = FVector(400.f, 400.f, 10.f);

private:
	void ApplyFootprint();
	void ApplyLabel();
};
