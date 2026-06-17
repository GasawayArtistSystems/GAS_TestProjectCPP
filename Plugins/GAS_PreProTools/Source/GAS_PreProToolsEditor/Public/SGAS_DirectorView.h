#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "ScriptModel.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"

enum class EGASDirectorMode : uint8
{
    Blocking,
    ShotEdit,
    Watch
};

class SGAS_DirectorView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGAS_DirectorView) {}
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    void SetActiveScene(const FString& SceneId, const FGASScript* InScript);
    void RefreshCast(const FGASScript* InScript);
    const FString& GetActiveSceneId() const { return ActiveSceneId; }

    DECLARE_DELEGATE(FOnBlockingRequested);
    FOnBlockingRequested OnBlockingRequested;

    DECLARE_DELEGATE(FOnCastButtonClicked);
    FOnCastButtonClicked OnCastButtonClicked;

private:
    // --------------------------------------------------
    // Mode
    // --------------------------------------------------
    EGASDirectorMode ActiveMode = EGASDirectorMode::Blocking;
    void SetMode(EGASDirectorMode NewMode);
    FReply OnModeButtonClicked(EGASDirectorMode Mode);

    // Mode button styling helpers
    const FSlateBrush* GetModeButtonBrush(EGASDirectorMode Mode) const;
    FSlateColor GetModeButtonColor(EGASDirectorMode Mode) const;

    // --------------------------------------------------
    // Shot list
    // --------------------------------------------------
    void RebuildShotList();
    TSharedRef<ITableRow> GenerateShotRow(
        TSharedPtr<FString> Item,
        const TSharedRef<STableViewBase>& OwnerTable
    );

    // --------------------------------------------------
    // Cast list
    // --------------------------------------------------
    void RebuildCastList();
    TSharedRef<ITableRow> GenerateCastRow(
        TSharedPtr<FString> Item,
        const TSharedRef<STableViewBase>& OwnerTable
    );

    // --------------------------------------------------
    // Viewport
    // --------------------------------------------------
    TSharedRef<SWidget> GetLevelEditorViewport();

    FVector SavedPerspectiveLocation;
    FRotator SavedPerspectiveRotation;
    bool bHasSavedPerspective = false;

    // --------------------------------------------------
    // Data
    // --------------------------------------------------
    FString ActiveSceneId;
    const FGASScript* Script = nullptr;

    TArray<TSharedPtr<FString>> ShotListItems;
    TSharedPtr<SListView<TSharedPtr<FString>>> ShotListView;

    TArray<TSharedPtr<FString>> CastListItems;
    TSharedPtr<SListView<TSharedPtr<FString>>> CastListView;

    TSharedPtr<SEditorViewport> ViewportWidget;

    // --------------------------------------------------
    // UI references
    // --------------------------------------------------
    TSharedPtr<SBorder> ModeBadge;
    TSharedPtr<STextBlock> ModeBadgeText;

    FString ActiveShotMarkerId;
    void OnShotSelected(TSharedPtr<FString> Item);
    void PilotToShotCamera(int32 ShotIndex);
};