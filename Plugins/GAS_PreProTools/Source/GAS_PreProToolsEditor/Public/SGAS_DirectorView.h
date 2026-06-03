#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "ScriptModel.h"

#include "SEditorViewport.h"
#include "EditorViewportClient.h"

class SGAS_DirectorView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGAS_DirectorView) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetActiveScene(const FString& SceneId, const FGASScript* InScript);

    void RefreshCast(const FGASScript* InScript);

private:
    void RebuildShotList();
    TSharedRef<ITableRow> GenerateShotRow(
        TSharedPtr<FString> Item,
        const TSharedRef<STableViewBase>& OwnerTable
    );

    FString ActiveSceneId;
    const FGASScript* Script = nullptr;

    TArray<TSharedPtr<FString>> ShotListItems;
    TSharedPtr<SListView<TSharedPtr<FString>>> ShotListView;

    TSharedPtr<SEditorViewport> ViewportWidget;
    TSharedRef<SWidget> GetLevelEditorViewport();

    TArray<TSharedPtr<FString>> CastListItems;
    TSharedPtr<SListView<TSharedPtr<FString>>> CastListView;

    void RebuildCastList();
    TSharedRef<ITableRow> GenerateCastRow(
        TSharedPtr<FString> Item,
        const TSharedRef<STableViewBase>& OwnerTable
    );


};