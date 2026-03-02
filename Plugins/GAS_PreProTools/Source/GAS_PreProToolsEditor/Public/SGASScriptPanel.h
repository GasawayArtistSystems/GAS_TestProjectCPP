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
DECLARE_DELEGATE(FOnRequestShotListRebuild);

DECLARE_DELEGATE_OneParam(FOnRequestExternalScroll, float);


class UGASShotListSelectionState;


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

    // Access to last rendered layout (authoritative)
    const TArray<FRenderedParagraph>& GetRenderedParagraphs() const
    {
        return RenderedParagraphs;
    }



    // =========================================================
    // Editing / Interaction
    // =========================================================
    void SetEditMode(bool bInEditMode);
    void SetAddMode(bool bInAddMode);
    void ResetEditorModes();


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

    void ScrollToParagraph(int32 ParagraphIndex);
    void ScrollToBlockId(const FString& BlockId);

    // Returns BlockId for the currently selected paragraph (Edit Mode selection).
    // Empty if nothing is selected.
    FString GetSelectedBlockId() const;

    // =========================================================
    // Shot Selection
    // =========================================================
    void EnterShotSelectionMode();
    int32 CountShotsForScene(const FString& SceneBlockId) const;
    void SetAddShotArmed(bool bArmed);
    void SetShotAddArmed(bool bInArmed);

    DECLARE_DELEGATE_OneParam(FOnShotActivated, const FGuid&);

    FOnShotActivated OnShotActivated;

    // Shot range selection (drag)
    bool bIsShotRangeDragging = false;
    int32 ShotRangeStartParagraph = INDEX_NONE;
    int32 ShotRangeCurrentParagraph = INDEX_NONE;
    void SetEnabledCastNames(const TArray<TSharedPtr<FString>>& InNames);
    bool bPageBreakDidDrag = false;


    int32 DraggedPageBreakTargetRenderIndex = INDEX_NONE;
    int32 PendingDeletePageBreakIndex = INDEX_NONE;
    float PageBreakClickStartY = 0.f;


    FOnRequestShotListRebuild OnRequestShotListRebuild;

    void ModifyShotMarkerMetadata(
        const FGuid& ShotId,
        const FString& Key,
        const FString& Value
    );

    float GetScrollOffsetY() const { return ScrollOffsetY; }
    float GetClampedScrollOffsetY() const { return ScrollOffsetY; }
    void SetExternalScrollOffset(float InOffset);

    FOnRequestExternalScroll OnRequestExternalScroll;

    FGASScript* GetSourceScript() const { return SourceScript; }

    void OpenShotIntentPopup(const FString& MarkerId, bool bIsNewShot = false);


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

    virtual FReply OnMouseButtonDoubleClick(
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

    virtual FReply OnMouseWheel(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent
    ) override;

    virtual FCursorReply OnCursorQuery(
        const FGeometry& MyGeometry,
        const FPointerEvent& CursorEvent
    ) const override;

    virtual FReply OnKeyDown(
        const FGeometry& MyGeometry,
        const FKeyEvent& InKeyEvent
    ) override;


    virtual bool SupportsKeyboardFocus() const override { return true; }


    // ------------------------------------------------------------
    // Shot Add (Rubberband Commit)
    // ------------------------------------------------------------
    bool bShotAddArmed = false;

    float ShotRangeStartX = 0.f;
    float ShotRangeStartY = -1.f;



    int32 HoveredShotParagraphIndex = INDEX_NONE;

    FString HoveredShotMarkerId;
    FString SelectedShotMarkerId;

    mutable FString PendingScrollBlockId;
    mutable float LastKnownViewportHeight = 0.f;
    bool bPendingScrollAfterLayout = false;
    bool bLockExternalScroll = false;
    bool bIgnoreNextExternalScrollOnSetScript = false;



    int32 CachedDraggingShotIndex = INDEX_NONE;
    void EndAllDrags();
    bool bDragMoved = false;
        

    FString HitTestShot(const FVector2D& LocalPos) const;
    FString HitTestShotTail(const FVector2D& LocalPos) const;


    int32 FindOwningSceneParagraphIndex(int32 StartParagraphIndex) const;


    int32 PendingShotParagraphIndex = INDEX_NONE;

    void CommitShotAtParagraph(
        int32 SceneParaIndex,
        int32 InsertIndex,
        float ShotStartScriptY,
        float ShotEndScriptY,
        float CommitX
    );


    int32 ResolveSceneIndexAtY(float LocalY) const;

    TArray<TSharedPtr<FString>> EnabledCastNames;
    TArray<TSharedPtr<FString>> EnabledCastOptions;
    TSharedPtr<FString> SelectedShotIntentSubject;

    void RebuildEnabledCastOptions();
    TSharedPtr<class SGASShotCastList> ShotCastListWidget;


    UGASShotListSelectionState* ShotSelectionState = nullptr;


    // --------------------------------------------------
    // Shot Intent Popup Guard
    // --------------------------------------------------
    bool bShotIntentPopupOpen = false;

    FText GetHoveredShotTooltipText() const;
    bool GetShotPillLocalPos(
        const FString& ShotId,
        FVector2D& OutLocalPos
    ) const;


    // --------------------------------------------------
    // Root container for script surface (persistent)
    // --------------------------------------------------
    TSharedPtr<SBorder> ScriptRootBorder;

    // --------------------------------------------------
    // Shot pill tooltip widget
    // --------------------------------------------------
    TSharedPtr<SWidget> ShotTooltipWidget;
    TSharedPtr<SWindow> ShotHoverTooltipWindow;
    FString ShotHoverTooltipShotId;




    // =========================================================
    // Edit Pipeline
    // =========================================================
    void ApplyScriptEdit(const FGASScriptEdit& Edit);   

    void OpenEditActionDialog(int32 BlockIndex, const FText& WindowTitle);
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


    bool  bShowShotGhost = false;
    float ShotGhostY = 0.f;
    static constexpr float ShotStackSpacing = 6.f;
    static constexpr float ShotBaseOffset = 10.f;

    int32 HitTestParagraph(float LocalY) const;
    int32 HitTestAddGutter(float LocalY) const;


    // Cache of clickable shot icon rectangles built during OnPaint
    struct FShotHitRect
    {
        FString MarkerId;
        FSlateRect Rect;
    };


    mutable TArray<FShotHitRect> ShotHitRects;


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

    // =============================================================
    // SHOT RUBBER-BAND SELECTION (Phase 1)
    // =============================================================
    bool bIsShotSelectMode = false;     // Armed via camera icon
    bool bIsDraggingShot = false;       // Mouse currently dragging

    FString ShotStartBlockId;
    FString ShotEndBlockId;

    int32 ShotStartParagraphIndex = INDEX_NONE;
    int32 ShotCurrentParagraphIndex = INDEX_NONE;


    FGASScript* SourceScript = nullptr;
    mutable float ScrollOffsetY = 0.f;
    mutable int32 PendingScrollParagraph = INDEX_NONE;
    mutable bool bForceScrollReset = false;
    mutable bool bSuppressScrollInput = false;
    float ShotRangeStartLocalY = 0.f;

    // =============================================================
    // Block Type Context Menu (Edit Mode)
    // =============================================================
    void OpenChangeBlockTypeMenu(
        int32 ParagraphIndex,
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent
    );

    bool CanChangeBlockType(int32 ParagraphIndex, EGASBlockType NewType) const;

    void ApplyBlockTypeChange(int32 ParagraphIndex, EGASBlockType NewType);


    int32 ResolveParagraphForShot(float LocalY) const;

};
