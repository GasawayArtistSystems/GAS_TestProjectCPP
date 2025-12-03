#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Delegates/DelegateCombinations.h"

// Forward declarations
DECLARE_DELEGATE_OneParam(FOnParagraphClicked, int32);
struct FRenderedParagraph;
struct FScriptParagraph;


/**
 * SGASScriptPanel
 *
 * Final Draft–style screenplay renderer.
 * Renders paragraphs based on computed layout (indentation, wrapping, page breaks).
 * Line-based system removed — now fully paragraph-based.
 */
class SGASScriptPanel : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGASScriptPanel) {}
        SLATE_EVENT(FOnParagraphClicked, OnParagraphClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Supply the formatted, paginated paragraphs */
    void SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs);

    /** Scroll to a given paragraph */
    void ScrollToParagraph(int32 ParagraphIndex);

    /** Set start/end paragraph indices for each shot */
    void SetShotMarkers(const TArray<int32>& InStarts, const TArray<int32>& InEnds)
    {
        ShotStartParagraphs = InStarts;
        ShotEndParagraphs = InEnds;
        Invalidate(EInvalidateWidget::LayoutAndVolatility);
    }

private:

    /** Rendering */
    virtual int32 OnPaint(const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

    /** Hit testing: detect which paragraph was clicked */
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override;

private:

    /** All paragraphs after Final Draft layout / pagination */
    TArray<FRenderedParagraph> RenderedParagraphs;

    /** Event callback for paragraph click */
    FOnParagraphClicked OnParagraphClicked;

    /** Cached scroll offset (if using external scroll box) */
    mutable float CachedTotalHeight = 0.f;

    // ------------------------------------------------------
    // Shot Marking Data (Paragraph-Based)
    // ------------------------------------------------------

    // Start and end paragraph indices for each shot
    TArray<int32> ShotStartParagraphs;
    TArray<int32> ShotEndParagraphs;

    // Hover + selection state for markers
    mutable int32 HoveredShotIndex = INDEX_NONE;
    mutable int32 SelectedShotIndex = INDEX_NONE;
};
