#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "GASScriptAsset.h"


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
        SLATE_EVENT(FOnParagraphClicked, OnParagraphClicked)
    SLATE_END_ARGS()


    void Construct(const FArguments& InArgs);

    void SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs);
    void SetShotMarkers(const TArray<FGASMarker>& Markers);

    void SetShowSceneNumbers(bool bInShow);

    void SetScript(FGASScript* InScript);



    // =========================================================
    // View / Navigation
    // =========================================================
    void ScrollToParagraph(int32 ParagraphIndex);

    // =========================================================
    // Editing / Interaction
    // =========================================================
    void SetEditMode(bool bInEditMode);
    void SetAddMode(bool bInAddMode);



    void OpenDialoguePreviewDialog(int32 BlockIndex);
    void OpenEditDialogueDialog(int32 BlockIndex);

    void TryEditParagraph(int32 BlockIndex);

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

    FOnParagraphClicked OnParagraphClicked;

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

    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    virtual FReply OnMouseButtonDown(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent
    ) override;

    virtual FReply OnMouseMove(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent
    ) override;

    virtual FReply OnMouseButtonUp(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent
    ) override;

    virtual FCursorReply OnCursorQuery(
        const FGeometry& MyGeometry,
        const FPointerEvent& CursorEvent
    ) const override;


    // =========================================================
    // Edit Pipeline
    // =========================================================
    void ApplyScriptEdit(const FGASScriptEdit& Edit);   


    void OpenEditActionDialog(int32 BlockIndex);
    void OpenAddBlockDialog(int32 InsertAfterParagraphIndex);
    void InsertNewBlock(int32 InsertAfterParagraphIndex, int32 BlockTypeChoice);
    void OpenEditCharacterDialog(int32 BlockIndex);



    // =========================================================
    // Cached State
    // =========================================================
    TArray<FRenderedParagraph> RenderedParagraphs;
    
    TArray<FGASMarker> ShotMarkers;

    mutable float CachedTotalHeight = 0.f;
    mutable bool  bNeedsRedraw = true;

    FText CachedEditText;
    TSharedPtr<SMultiLineEditableTextBox> EditTextBox;

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
    bool bAddMode = false;

    void OpenDeleteBlockDialog(int32 BlockIndex);
    void DeleteBlock(int32 BlockIndex);


    FString GetSourceTextForBlock(const FString& BlockId) const;

    int32 SelectedParagraphIndex = INDEX_NONE;
    int32 PendingEditBlockIndex = INDEX_NONE;

    void OnEditCancelled();

    int32 PendingAddStartIndex = INDEX_NONE;
    int32 PendingAddBlockCount = 0;
    int32 PendingDialogueAfterCharacterIndex = INDEX_NONE;



    // ------------------------------------------------------------
    // Applies a text edit to a script block and triggers re-layout.
    // ------------------------------------------------------------
    void ApplyTextEdit(const FString& BlockId, const FString& NewText);


    mutable int32 HoveredShotIndex = INDEX_NONE;
    mutable int32 SelectedShotIndex = INDEX_NONE;


    int32 HitTestParagraph(float LocalY) const;
    int32 HitTestAddGutter(float LocalY) const;


    // Page break drag state
    bool bIsDraggingPageBreak = false;
    int32 DraggedPageBreakIndex = INDEX_NONE; // index into UserPageBreaks
    float DragPreviewY = 0.f;
    int32 HoveredPageBreakIndex = INDEX_NONE;
    // ------------------------------------------------------------
    // Page break snap animation
    // ------------------------------------------------------------
    bool  bAnimatingPageBreak = false;
    float SnapStartY = 0.f;
    float SnapTargetY = 0.f;
    float SnapAnimAlpha = 0.f; // 0 → 1


    FGASScript* SourceScript = nullptr;


};
