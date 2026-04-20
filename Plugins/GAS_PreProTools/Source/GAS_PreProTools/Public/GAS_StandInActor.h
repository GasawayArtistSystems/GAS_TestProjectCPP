#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS_StandInTypes.h"
#include "Misc/Guid.h"


#include "GAS_StandInActor.generated.h"

UCLASS()
class GAS_PREPROTOOLS_API AGAS_StandInActor : public AActor
{
	GENERATED_BODY()

public:
	AGAS_StandInActor();

#if WITH_EDITOR
	virtual bool IsSelectable() const override;
#endif

	virtual EGASStandInType GetStandInType() const;

	FVector GetEyelineWorldLocation() const;

	void SetCharacterSkeletalMesh(USkeletalMesh* NewMesh);
	USkeletalMesh* GetCharacterMesh() const;

	void RefreshMeshFromScript(struct FGASScript* Script);

	// Script character this stand-in represents (inert binding for now)
	UPROPERTY(EditAnywhere, Category = "GAS|Binding")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FString GAS_CharacterId;

	UPROPERTY()
	FGuid SequencerBindingGuid;

	void FixMeshOffsetAfterSequencer();

	UPROPERTY(Transient)
	bool bIsSequencerDriven = false;

	virtual void PostRegisterAllComponents() override;
	virtual void PostLoad() override;


protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS|StandIn")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category = "GAS|StandIn")
	USceneComponent* StandInPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS|StandIn")
	UCapsuleComponent* BodyCapsule;

	UPROPERTY(VisibleAnywhere, Category = "GAS|StandIn")
	UBillboardComponent* DebugBillboard;

	struct FGASStandInPhysicalSpec
	{
		float CapsuleRadius;
		float CapsuleHalfHeight;
		float EyeHeight; // world-space height above floor
	};


	virtual FGASStandInPhysicalSpec GetPhysicalSpec() const;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "GAS|StandIn")
	USceneComponent* EyelinePivot;

	UPROPERTY(VisibleAnywhere, Category = "GAS|StandIn")
	UArrowComponent* EyelineArrow;

	// All visual meshes must attach here
	UPROPERTY(VisibleAnywhere, Category = "GAS|Mesh")
	USceneComponent* MeshPivot;

	// Mesh grounding / offset layer (never affects body truth)
	UPROPERTY(VisibleAnywhere, Category = "GAS|Mesh")
	USceneComponent* MeshVisualPivot;

	// Temporary proxy mesh for swap-proof testing
	UPROPERTY(VisibleAnywhere, Category = "GAS|Mesh")
	UStaticMeshComponent* ProxyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Stand-In")
	USkeletalMeshComponent* ProxySkeletalMesh;

	UPROPERTY(Transient)
	USkeletalMesh* SkeletalMeshAsset;

	UPROPERTY()
	bool bHasBeenGrounded = false;


	enum class EGASStandInVisualMode : uint8
	{
		Static,
		Skeletal
	};

	void SetVisualMode(EGASStandInVisualMode Mode);

	void ApplySkeletalAsset();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif



};
