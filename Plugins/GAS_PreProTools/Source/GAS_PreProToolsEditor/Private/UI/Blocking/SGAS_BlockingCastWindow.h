#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "LevelSequence.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/SkeletalMesh.h"

struct FGASScript;

class SGAS_BlockingCastWindow : public SCompoundWidget
{
public:
    DECLARE_DELEGATE(FOnCastModified);

    SLATE_BEGIN_ARGS(SGAS_BlockingCastWindow) {}
        SLATE_ARGUMENT(FGuid, SceneId)
        SLATE_ARGUMENT(FGASScript*, Script)
        SLATE_ARGUMENT(ULevelSequence*, BlockingSequence)
        SLATE_EVENT(FOnCastModified, OnCastModified)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    TWeakObjectPtr<ULevelSequence> BlockingSequence;

    TArray<TSharedPtr<FAssetData>> AvailableCharacterMeshes;

private:

    FGuid SceneId;
    FGASScript* Script = nullptr;

    // Persistent list data (required for SListView)
    TArray<TSharedPtr<FString>> LeftItems;
    TArray<TSharedPtr<FString>> RightItems;

    TSharedPtr<SListView<TSharedPtr<FString>>> LeftListView;
    TSharedPtr<SListView<TSharedPtr<FString>>> RightListView;

    TSharedPtr<FString> SelectedLeftItem;
    TSharedPtr<FString> SelectedRightItem;

    FOnCastModified OnCastModified;

    void RefreshLists();
    FReply OnAddClicked();
    FReply OnRemoveClicked();
};
