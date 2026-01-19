#pragma once

#include "CoreMinimal.h"
#include "GAS_ShotListRules.h"
#include "Delegates/Delegate.h"

#include "GAS_ShotListSelection.generated.h"




DECLARE_MULTICAST_DELEGATE_OneParam(FOnGASShotListSelectionChanged, const FGASShotListSelection&);

UCLASS()
class UGASShotListSelectionState : public UObject
{
	GENERATED_BODY()

public:
	const FGASShotListSelection& Get() const
	{
		return Selection;
	}

	void Clear()
	{
		Selection = FGASShotListSelection::None();
		OnSelectionChanged.Broadcast(Selection);
	}

	void SelectScene(const FString& InSceneId, int32 InSceneIndex)
	{
		Selection.Type = EGASShotListSelectionType::Scene;
		Selection.SceneId = InSceneId;
		Selection.SceneIndex = InSceneIndex;
		Selection.ShotId.Empty();
		Selection.ShotIndex = INDEX_NONE;

		OnSelectionChanged.Broadcast(Selection);
	}

	void SelectShot(
		const FString& InSceneId,
		int32 InSceneIndex,
		const FString& InShotId,
		int32 InShotIndex
	)
	{
		Selection.Type = EGASShotListSelectionType::Shot;
		Selection.SceneId = InSceneId;
		Selection.SceneIndex = InSceneIndex;
		Selection.ShotId = InShotId;
		Selection.ShotIndex = InShotIndex;

		OnSelectionChanged.Broadcast(Selection);
	}

	FOnGASShotListSelectionChanged OnSelectionChanged;

private:
	UPROPERTY()
	FGASShotListSelection Selection;
};
