#include "GAS_SetActor.h"
#include "GAS_SetDiscovery.h"
#include "GASPreProLog.h"


#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/LevelStreamingDynamic.h"


AGAS_SetActor::AGAS_SetActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// --------------------------------------------------
	// Scene root (editor-movable anchor)
	// --------------------------------------------------
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	// --------------------------------------------------
	// Footprint (semantic size + ground reference)
	// --------------------------------------------------
	Set_Footprint = CreateDefaultSubobject<UBoxComponent>(TEXT("Set_Footprint"));
	Set_Footprint->SetupAttachment(SceneRoot);
	Set_Footprint->SetMobility(EComponentMobility::Movable);
	Set_Footprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// NOTE: size is authoritative via FootprintDimensions
	Set_Footprint->SetBoxExtent(FVector(200.f, 200.f, 5.f));

	// --------------------------------------------------
	// Visual footprint (editor-only visualization)
	// --------------------------------------------------
	Set_Footprint_Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Set_Footprint_Visual"));
	Set_Footprint_Visual->SetupAttachment(Set_Footprint);
	Set_Footprint_Visual->SetMobility(EComponentMobility::Movable);
	Set_Footprint_Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Set_Footprint_Visual->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Set_Footprint_Visual->SetStaticMesh(CubeMesh.Object);
	}

	// --------------------------------------------------
	// Label (data-driven, no manual transform)
	// --------------------------------------------------
	Set_Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Set_Label"));
	Set_Label->SetupAttachment(Set_Footprint);
	Set_Label->SetHorizontalAlignment(EHTA_Center);
	Set_Label->SetVerticalAlignment(EVRTA_TextCenter);
	Set_Label->SetWorldSize(50.f);
	Set_Label->SetCastShadow(false);

	// --------------------------------------------------
	// Initial application
	// --------------------------------------------------
	ApplyFootprint();
	ApplyLabel();
}


void AGAS_SetActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Force world-locked origin
	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector(1.f));

	ApplyFootprint();
}


#if WITH_EDITOR
void AGAS_SetActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGAS_SetActor, FootprintDimensions))
	{
		ApplyFootprint();
		ApplyLabel();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGAS_SetActor, SetName))
	{
		ApplyLabel();
		Set_Label->MarkRenderStateDirty();
	}

	// HARD LOCK transform
	SetActorLocation(FVector::ZeroVector);
	SetActorRotation(FRotator::ZeroRotator);
	SetActorScale3D(FVector(1.f));
}
#endif




void AGAS_SetActor::ApplyFootprint()
{
	if (!Set_Footprint || !Set_Footprint_Visual)
	{
		return;
	}

	// sanitize
	FootprintDimensions.X = FMath::Max(1.f, FootprintDimensions.X);
	FootprintDimensions.Y = FMath::Max(1.f, FootprintDimensions.Y);
	FootprintDimensions.Z = FMath::Max(1.f, FootprintDimensions.Z);

	const FVector Extent = FootprintDimensions * 0.5f;

	// Box is centered; lift so bottom sits on Z=0
	Set_Footprint->SetBoxExtent(Extent, /*bUpdateOverlaps=*/false);
	Set_Footprint->SetRelativeLocation(FVector(0.f, 0.f, Extent.Z));

	// Visual assumes Engine cube = 100cm
	static constexpr float CubeSizeCm = 100.f;
	const FVector VisualScale = FootprintDimensions / CubeSizeCm;

	Set_Footprint_Visual->SetRelativeScale3D(VisualScale);
	Set_Footprint_Visual->SetRelativeLocation(FVector(0.f, 0.f, Extent.Z));
}

void AGAS_SetActor::ApplyLabel()
{
	if (!Set_Label)
	{
		return;
	}

	// HARD reset any editor overrides
	Set_Label->SetRelativeTransform(FTransform::Identity);

	Set_Label->SetText(FText::FromName(SetName));

	const float Z = FMath::Max(50.f, FootprintDimensions.Z + 50.f);
	Set_Label->SetRelativeLocation(FVector(0.f, 0.f, Z));
}

void AGAS_SetActor::LoadSetById(FName InSetId)
{
	const TArray<FGASSetDescriptor>& Sets =
		FGASSetDiscovery::GetAvailableSets();

	FString LevelPath;

	for (const FGASSetDescriptor& Set : Sets)
	{
		if (Set.SetId == InSetId)
		{
			SetName = Set.SetId;
			LevelPath = Set.LevelPath;
			break;
		}
	}

	if (LevelPath.IsEmpty())
	{
		UE_LOG(LogGASPrePro, Error,
			TEXT("GAS_SetActor: LevelPath EMPTY for %s"),
			*InSetId.ToString());
		return;
	}

	ApplyLabel();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogGASPrePro, Error, TEXT("GAS_SetActor: No valid World"));
		return;
	}

	bool bSuccess = false;

	ULevelStreamingDynamic* StreamingLevel =
		ULevelStreamingDynamic::LoadLevelInstance(
			World,
			LevelPath,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			bSuccess
		);

	if (!StreamingLevel || !bSuccess)
	{
		UE_LOG(LogGASPrePro, Error,
			TEXT("GAS_SetActor: Failed to stream %s"),
			*LevelPath);
		return;
	}

	StreamingLevel->SetShouldBeVisible(true);
	StreamingLevel->SetShouldBeLoaded(true);

	ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();

	if (LoadedLevel)
	{
		for (AActor* Actor : LoadedLevel->Actors)
		{
			if (!Actor) continue;

#if WITH_EDITOR
			Actor->Modify();
			Actor->SetIsTemporarilyHiddenInEditor(false);
			Actor->SetActorEnableCollision(false);
			Actor->SetFlags(RF_Transactional);
			Actor->SetActorEnableCollision(false);
			Actor->SetIsTemporarilyHiddenInEditor(false);
#endif
		}

	}


	UE_LOG(LogGASPrePro, Verbose,
		TEXT("GAS_SetActor: Streaming level %s"),
		*LevelPath);

}


