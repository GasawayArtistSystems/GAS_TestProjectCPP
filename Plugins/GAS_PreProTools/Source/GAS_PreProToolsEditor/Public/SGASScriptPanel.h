#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"


#include "ScriptModel.h"
#include "ScriptLayoutEngine.h"


// Paragraph click delegate
DECLARE_DELEGATE_OneParam(FOnParagraphClicked, int32 /* ParagraphIndex */);


// =====================================================
// Script Edit Types (internal)
// =====================================================

enum class EGASScriptEditType
{
    SetPageBreak,
    ClearPageBreak
};

struct FGASScriptEdit
{
    EGASScriptEditType Type;
    int32 ParagraphIndex = INDEX_NONE;
};



struct FRenderedParagraph;

class SGASScriptPanel : public SCompoundWidget
{
public:

    // =========================================================
    // Construction / Setup
    // =========================================================
    SLATE_BEGIN_ARGS(SGASScriptPanel) {}
        SLATE_ARGUMENT(FGASScript*, SourceScript)
        SLATE_EVENT(FOnParagraphClicked, OnParagraphClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void SetScript(FGASScript* InScript);
    void SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs);
    void SetShotMarkers(const TArray<FGASMarker>& Markers);

    void SetShowSceneNumbers(bool bInShow)
    {
        bShowSceneNumbers = bInShow;
        bNeedsRedraw = true;
    }

    // =========================================================
    // View / Navigation
    // =========================================================
    void ScrollToParagraph(int32 ParagraphIndex);

    // =========================================================
    // Editing / Interaction
    // =========================================================
    void SetEditMode(bool bInEditMode)
    {
        bEditMode = bInEditMode;
    }

    void ShowPageBreakMenu(int32 ParagraphIndex);
    void InsertPageBreak(int32 ParagraphIndex);
    void RemovePageBreak(int32 ParagraphIndex);

    // =========================================================
    // Layout
    // =========================================================
    void RebuildLayout();

    // =========================================================
    // Redraw Delegate
    // =========================================================
    DECLARE_DELEGATE(FOnRequestFullRedraw);
    FOnRequestFullRedraw OnRequestFullRedraw;

    // =========================================================
    // Script Mutation Notification
    // =========================================================
    DECLARE_DELEGATE(FOnScriptMutated);
    FOnScriptMutated OnScriptMutated;


private:

    // =========================================================
    // Slate Overrides
    // =========================================================
    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    // =========================================================
    // Edit Pipeline
    // =========================================================
    void ApplyScriptEdit(const FGASScriptEdit& Edit);   


    // =========================================================
    // Helpers
    // =========================================================
    int32 SnapToNearestParagraph(float ReleaseY, const TArray<FRenderedParagraph>& Paragraphs) const;

    // =========================================================
    // Cached State
    // =========================================================
    TArray<FRenderedParagraph> RenderedParagraphs;
    FOnParagraphClicked OnParagraphClicked;

    FGASScript* SourceScript = nullptr;
    TArray<FGASMarker> ShotMarkers;

    mutable float CachedTotalHeight = 0.f;
    mutable bool  bNeedsRedraw = true;

    // =============================================================
    // EDIT MODE (Scaffolding)
    // -------------------------------------------------------------
    // Edit Mode is an intentional, opt-in state.
    // When OFF: script behaves as a read-only viewer.
    // When ON: clicks resolve paragraphs for editing via dialogs.
    // Inline editing is NOT implemented yet.
    // =============================================================

    bool bShowSceneNumbers = false;

    bool bEditMode = false;

    void OpenEditDialog(int32 ParagraphIndex);

    FString GetSourceTextForBlock(const FString& BlockId) const;

    int32 SelectedParagraphIndex = INDEX_NONE;


    // ------------------------------------------------------------
    // Applies a text edit to a script block and triggers re-layout.
    // ------------------------------------------------------------
    void ApplyTextEdit(const FString& BlockId, const FString& NewText);


    mutable int32 HoveredShotIndex = INDEX_NONE;
    mutable int32 SelectedShotIndex = INDEX_NONE;

    // =========================================================
    // Page Break Dragging
    // =========================================================
    mutable bool  bIsDraggingPageBreak = false;
    mutable int32 DraggedPageBreakParagraph = INDEX_NONE;
    mutable float DragOffsetY = 0.f;

    mutable bool  bShowDragPreview = false;
    mutable float DragPreviewY = 0.f;
    mutable int32 DragPreviewParagraphIndex = INDEX_NONE;

    int32 HitTestParagraph(float LocalY) const;

    void SilentRemoveUserBreak(int32 ParagraphIndex);


};
