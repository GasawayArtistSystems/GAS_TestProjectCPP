#pragma once

#include "CoreMinimal.h"
#include "GAS_ShotListRules.generated.h"

UENUM()
enum class EGASShotListSelectionType : uint8
{
	None,
	Scene,
	Shot
};

UENUM()
enum class EGASShotInsertMode : uint8
{
	AfterSelection,      // If Shot selected: after that shot. If Scene selected: append to scene.
	BeforeSelection,     // If Shot selected: before that shot. If Scene selected: insert as first shot in scene.
	AppendToScene        // Explicit: append to selected scene (or scene owning selected shot).
};

USTRUCT()
struct FGASShotListSelection
{
	GENERATED_BODY()

	UPROPERTY()
	EGASShotListSelectionType Type = EGASShotListSelectionType::None;

	// Stable IDs (preferred)
	UPROPERTY()
	FString SceneId;

	UPROPERTY()
	FString ShotId;

	// Cached indices (optional, -1 if unknown)
	UPROPERTY()
	int32 SceneIndex = INDEX_NONE;

	UPROPERTY()
	int32 ShotIndex = INDEX_NONE;

	static FGASShotListSelection None()
	{
		return FGASShotListSelection();
	}

	bool HasScene() const
	{
		return !SceneId.IsEmpty() || SceneIndex != INDEX_NONE;
	}

	bool HasShot() const
	{
		return !ShotId.IsEmpty() || ShotIndex != INDEX_NONE;
	}
};

USTRUCT()
struct GAS_PREPROTOOLS_API FGASShotInsertPlan
{
	GENERATED_BODY()

	UPROPERTY()
	bool bCanInsert = false;

	// Target scene index for insertion (resolved)
	UPROPERTY()
	int32 TargetSceneIndex = INDEX_NONE;

	// Insert position within scene shots array (0..Num)
	UPROPERTY()
	int32 InsertShotIndex = INDEX_NONE;

	// Resolved mode used
	UPROPERTY()
	EGASShotInsertMode Mode = EGASShotInsertMode::AfterSelection;

	// If false: caller should surface UI guard (no-op)
	UPROPERTY()
	FString RejectReason;
};

namespace GAS_ShotListRules
{
	GAS_PREPROTOOLS_API FGASShotInsertPlan MakeInsertPlan(
		const FGASShotListSelection& Selection,
		EGASShotInsertMode Mode,
		int32 NumScenes,
		const TFunctionRef<int32(int32 SceneIndex)>& GetNumShotsInScene
	);

	GAS_PREPROTOOLS_API FGASShotInsertPlan MakeDefaultAddPlan(
		const FGASShotListSelection& Selection,
		int32 NumScenes,
		const TFunctionRef<int32(int32 SceneIndex)>& GetNumShotsInScene
	);
}

