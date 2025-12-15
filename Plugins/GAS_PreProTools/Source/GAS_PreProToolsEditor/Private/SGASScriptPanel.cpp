// NOTE:
// This panel NEVER mutates script content.
// All edits go through the script edit pipeline (JSON → ReLayout).


#include "SGASScriptPanel.h"
#include "ScriptFormatRules.h"
#include "SlateOptMacros.h"
#include "GAS_PreProToolsStyle.h"
#include "ScriptLayoutEngine.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

#include "SlateBasics.h"
#include "SlateExtras.h"




// =============================================================
//  Construct
// =============================================================
void SGASScriptPanel::Construct(const FArguments& InArgs)
{
    SourceScript = InArgs._SourceScript;
    OnParagraphClicked = InArgs._OnParagraphClicked;

    bNeedsRedraw = true;

    ChildSlot
        [
            SNew(SBorder)
                .Padding(0)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
        ];

}


void SGASScriptPanel::RebuildLayout()
{
    if (!SourceScript)
        return;

    // Panel width: fall back to something sane if geometry is tiny at startup
    float PanelWidth = GetCachedGeometry().GetLocalSize().X;
    if (PanelWidth < 200.f)
    {
        PanelWidth = 1100.f;
    }

    TArray<FRenderedParagraph> NewRendered =
        ScriptLayoutEngine::LayoutScript(
            SourceScript->Blocks,
            PanelWidth,
            SourceScript->UserPageBreakParagraphs,
            SourceScript->SuppressedAutoBreakBlockIds
        );

    SetRenderedScript(NewRendered);   // updates RenderedParagraphs + bNeedsRedraw

    if (OnRequestFullRedraw.IsBound())
    {
        OnRequestFullRedraw.Execute();
    }

}


// =============================================================
//  SetRenderedScript
// =============================================================
void SGASScriptPanel::SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs)
{
    RenderedParagraphs = InParagraphs;
    bNeedsRedraw = true;
}


// =============================================================
//  SetScript
// =============================================================
void SGASScriptPanel::SetScript(FGASScript* InScript)
{
    SourceScript = InScript;
}


// =============================================================
//  ComputeDesiredSize
// =============================================================
FVector2D SGASScriptPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(1000.f, CachedTotalHeight + 200.f);
}

// =====================================================
// SCRIPT EDIT PIPELINE (single entry point for edits)
// =====================================================


void SGASScriptPanel::ApplyScriptEdit(const FGASScriptEdit& Edit)
{
    if (!SourceScript)
        return;

    if (Edit.ParagraphIndex == INDEX_NONE)
        return;

    switch (Edit.Type)
    {
    case EGASScriptEditType::SetPageBreak:
        SourceScript->UserPageBreakParagraphs.AddUnique(Edit.ParagraphIndex);
        break;

    case EGASScriptEditType::ClearPageBreak:
        SourceScript->UserPageBreakParagraphs.Remove(Edit.ParagraphIndex);
        break;
    }

    // Always re-layout after edits so visuals match JSON
    RebuildLayout();
}


void SGASScriptPanel::ApplyTextEdit(
    const FString& BlockId,
    const FString& NewText)
{
    if (!SourceScript)
        return;

    for (FGASBlock& Block : SourceScript->Blocks)
    {
        if (Block.Id == BlockId)
        {
            Block.Text = NewText;
            break;
        }
    }

    if (OnScriptMutated.IsBound())
    {
        UE_LOG(LogTemp, Warning, TEXT("SCRIPT MUTATED → notifying owner"));
        OnScriptMutated.Execute();
    }


    RebuildLayout();
}




// =============================================================
//  PAINT — Draw text, page breaks, preview line
// =============================================================
int32 SGASScriptPanel::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{


    // Track total height
    CachedTotalHeight = 0.f;

    const FSlateFontInfo FontInfo =
        FGAS_PreProToolsStyle::Get().GetFontStyle("GAS.ScriptFont");


    for (int32 i = 0; i < RenderedParagraphs.Num(); i++)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        // ------------------------------------------------------------
        // Draw selection highlight (Edit Mode only)
        // ------------------------------------------------------------
        if (bEditMode && i == SelectedParagraphIndex)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(0.f, P.TopY),
                    FVector2D(AllottedGeometry.GetLocalSize().X, P.Height)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.15f, 0.35f, 0.6f, 0.25f)
            );
        }

        // ----------------------------------------------
        // Draw lines at TRUE layout coordinates
        // ----------------------------------------------
        float LineY = P.TopY;

        for (const FString& Ln : P.Lines)
        {
            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(P.IndentLeft, LineY),   // <<< USE REAL INDENT
                    FVector2D::UnitVector
                ),
                FText::FromString(Ln),
                FontInfo,
                ESlateDrawEffect::None,
                FLinearColor::White
            );

            LineY += ScriptFormat::LineHeight;
        }

        // ----------------------------------------------
        // PAGE BREAK MARKER
        // ----------------------------------------------
        if (P.bStartsPage && P.PageNumber > 1)
        {
            FString Marker = FString::Printf(TEXT("-- PAGE %d --"), P.PageNumber);

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(0.f, P.TopY),
                    FVector2D::UnitVector
                ),
                FText::FromString(Marker),
                FontInfo,
                ESlateDrawEffect::None,
                FLinearColor::Yellow
            );
        }

        // ----------------------------------------------
        // Update panel height
        // ----------------------------------------------
        CachedTotalHeight = FMath::Max(CachedTotalHeight, P.TopY + P.Height);
    }


    // Draw preview line during drag
    if (bShowDragPreview)
    {
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 5,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(0.f, DragPreviewY),
                FVector2D(AllottedGeometry.GetLocalSize().X, 2.f)
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor::Yellow
        );
    }

    bNeedsRedraw = false;
    return LayerId + 10;
}



// =============================================================
//  OnMouseButtonDown — begin drag / detect clicks
// =============================================================
FReply SGASScriptPanel::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    FVector2D Local = MyGeometry.AbsoluteToLocal(
        MouseEvent.GetScreenSpacePosition());
    float MouseY = Local.Y;

    UE_LOG(LogTemp, Warning, TEXT("MOUSEDOWN: Alt=%d Left=%d  PosY=%.2f"),
        MouseEvent.IsAltDown(),
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton,
        MouseY);

    // ============================================================
    // ALT + LEFT = begin DRAG of a page break
    // (always allowed, even in Edit Mode)
    // ============================================================
    // NOTE:
    // ALT+drag page break interaction is gated correctly by Edit Mode,
    // but drag mechanics are temporarily disabled pending refactor.
    // (Hit-testing + selection must be stable first.)

    if (bEditMode &&
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
        MouseEvent.IsAltDown())

    {
        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            const FRenderedParagraph& P = RenderedParagraphs[i];

            if (P.bStartsPage && P.PageNumber > 1)
            {
                float Top = P.TopY;
                float Bottom = Top + ScriptFormat::LineHeight;

                if (MouseY >= Top && MouseY <= Bottom)
                {
                    bIsDraggingPageBreak = true;
                    DraggedPageBreakParagraph = i;
                    DragOffsetY = MouseY - Top;

                    UE_LOG(LogTemp, Error,
                        TEXT("  >>> DRAG STARTED on break paragraph %d"), i);

                    return FReply::Handled().CaptureMouse(AsShared());
                }
            }
        }
    }

    // ------------------------------------------------------------
    // In View Mode, do not allow any structural interaction
    // ------------------------------------------------------------
    if (!bEditMode)
    {
        return FReply::Unhandled();
    }


    // ============================================================
    // RIGHT CLICK = popup menu
    // (allowed regardless of Edit Mode)
    // ============================================================
    if (bEditMode &&
        MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("RIGHT CLICK detected in Edit Mode"));

        int32 HitIndex = HitTestParagraph(MouseY);
        if (HitIndex != INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("RIGHT CLICK hit paragraph %d"), HitIndex);

            ShowPageBreakMenu(HitIndex);
            return FReply::Handled();
        }
    }


    // ------------------------------------------------------------
    // View Mode: consume right-click so nothing mutates accidentally
    // ------------------------------------------------------------
    if (!bEditMode &&
        MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        return FReply::Unhandled();
    }


    // ============================================================
    // NORMAL LEFT CLICK
    // ============================================================
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
        !MouseEvent.IsAltDown())
    {
        int32 HitIndex = HitTestParagraph(MouseY);
        if (HitIndex == INDEX_NONE)
            return FReply::Unhandled();

        // -----------------------------
        // EDIT MODE CLICK
        // -----------------------------
        if (bEditMode)
        {
            SelectedParagraphIndex = HitIndex;

            UE_LOG(LogTemp, Warning,
                TEXT("EDIT MODE CLICK on paragraph %d"), HitIndex);

            OpenEditDialog(HitIndex);
            return FReply::Handled();
        }


        // -----------------------------
        // EDIT MODE CLICK
        // -----------------------------
        // Edit Mode enables intentional editing only.
        // Clicking a paragraph resolves its index and
        // opens an edit dialog (no inline editing).
        // -----------------------------

        if (OnParagraphClicked.IsBound())
        {
            OnParagraphClicked.Execute(HitIndex);
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}




// =============================================================
//  OnMouseMove — drag preview
// =============================================================
FReply SGASScriptPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (!bIsDraggingPageBreak)
        return FReply::Unhandled();

    FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    float DragY = LocalPos.Y - DragOffsetY;

    bShowDragPreview = true;
    DragPreviewY = DragY;

    UE_LOG(LogTemp, Warning, TEXT("DRAGGING: PreviewY=%.2f Offset=%.2f"), DragPreviewY, DragOffsetY);

    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);


    return FReply::Handled();
}



// =============================================================
//  SNAP → returns nearest paragraph index
// =============================================================
int32 SGASScriptPanel::SnapToNearestParagraph(float ReleaseY, const TArray<FRenderedParagraph>& Paragraphs) const
{
    int32 BestIndex = INDEX_NONE;
    float BestDist = FLT_MAX;

    for (int32 i = 0; i < Paragraphs.Num(); i++)
    {
        float Dist = FMath::Abs(ReleaseY - Paragraphs[i].TopY);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            BestIndex = i;
        }
    }

    return BestIndex;
}

// =============================================================
// Paragraph Hit Testing
// -------------------------------------------------------------
// Maps a local Y coordinate to a rendered paragraph index.
// This is deterministic and independent of Edit Mode.
// Used by editing, selection, and context menus.
// =============================================================



int32 SGASScriptPanel::HitTestParagraph(float LocalY) const
{
    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (LocalY >= P.TopY && LocalY <= (P.TopY + P.Height))
        {
            return i;
        }
    }

    return INDEX_NONE;
}




// =============================================================
//  OnMouseButtonUp — FINISH DRAG
// =============================================================
FReply SGASScriptPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    UE_LOG(LogTemp, Error, TEXT("MOUSEUP: Dragging=%d"), bIsDraggingPageBreak);

    if (!bIsDraggingPageBreak)
        return FReply::Unhandled();

    bIsDraggingPageBreak = false;
    bShowDragPreview = false;

    FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    float ReleaseY = LocalPos.Y;

    // SNAP to nearest paragraph
    int32 NewIndex = SnapToNearestParagraph(ReleaseY, RenderedParagraphs);

    UE_LOG(LogTemp, Error, TEXT("  >>> DROP TARGET = %d"), NewIndex);

    if (SourceScript && DraggedPageBreakParagraph != INDEX_NONE && NewIndex != INDEX_NONE)
    {
        // --------------------------------------------------------
        // 1. Remove old break
        // --------------------------------------------------------
        SilentRemoveUserBreak(DraggedPageBreakParagraph);

        // --------------------------------------------------------
        // 2. Insert NEW break
        // --------------------------------------------------------
        InsertPageBreak(NewIndex);
    }

    DraggedPageBreakParagraph = INDEX_NONE;

    return FReply::Handled().ReleaseMouseCapture();
}

FString SGASScriptPanel::GetSourceTextForBlock(const FString& BlockId) const
{
    if (!SourceScript)
        return FString();

    for (const FGASBlock& Block : SourceScript->Blocks)
    {
        if (Block.Id == BlockId)
        {
            return Block.Text;
        }
    }

    return FString();
}


// =============================================================
//  Edit Dialog
// =============================================================

void SGASScriptPanel::OpenEditDialog(int32 ParagraphIndex)
{
    if (!RenderedParagraphs.IsValidIndex(ParagraphIndex))
        return;

    const FRenderedParagraph& P = RenderedParagraphs[ParagraphIndex];

    // ------------------------------------------------------------
    // Capture original text
    // ------------------------------------------------------------
    FString OriginalText = GetSourceTextForBlock(P.BlockId);

    // We store edited text here before commit
    TSharedRef<FString> EditedText =
        MakeShared<FString>(OriginalText);

    // ------------------------------------------------------------
    // Build modal window
    // ------------------------------------------------------------
    TSharedRef<SWindow> EditWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Paragraph")))
        .ClientSize(FVector2D(600.f, 400.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    EditWindow->SetContent(
        SNew(SVerticalBox)

        // -----------------------------
        // Text editor
        // -----------------------------
        +SVerticalBox::Slot()
        .FillHeight(1.f)
        .Padding(8.f)
        [
            SNew(SMultiLineEditableTextBox)
                .Text(FText::FromString(OriginalText))
                .OnTextChanged_Lambda(
                    [EditedText](const FText& NewText)
                    {
                        *EditedText = NewText.ToString();
                    })
        ]

    // -----------------------------
    // Buttons
    // -----------------------------
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(8.f)
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .HAlign(HAlign_Right)
                .FillWidth(1.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Cancel")))
                        .OnClicked_Lambda(
                            [EditWindow]()
                            {
                                EditWindow->RequestDestroyWindow();
                                return FReply::Handled();
                            })
                ]

            + SHorizontalBox::Slot()
                .HAlign(HAlign_Right)
                .AutoWidth()
                .Padding(8.f, 0.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("OK")))
                        .OnClicked_Lambda(
                            [this, EditWindow, P, EditedText]()
                            {
                                ApplyTextEdit(P.BlockId, *EditedText);
                                EditWindow->RequestDestroyWindow();
                                return FReply::Handled();
                            })
                ]
        ]
        );

    // ------------------------------------------------------------
    // Show modal
    // ------------------------------------------------------------
    FSlateApplication::Get().AddWindow(EditWindow);
}



// =============================================================
//  Insert NEW break
// =============================================================
void SGASScriptPanel::InsertPageBreak(int32 ParagraphIndex)
{
    if (!SourceScript || !RenderedParagraphs.IsValidIndex(ParagraphIndex))
        return;

    ApplyScriptEdit({
        EGASScriptEditType::SetPageBreak,
        ParagraphIndex
        });
}





// =============================================================
//  Remove break → JSON + flags
// =============================================================
void SGASScriptPanel::SilentRemoveUserBreak(int32 ParagraphIndex)
{
    if (!SourceScript)
        return;


    // Find the block that CAUSED this break
    int32 BlockParaIndex = ParagraphIndex;

    // Find the nearest paragraph that actually starts the next block
    while (BlockParaIndex < RenderedParagraphs.Num() &&
        !RenderedParagraphs[BlockParaIndex].bAutoBreakCausedHere)
    {
        BlockParaIndex++;
    }

    // Fallback
    if (!RenderedParagraphs.IsValidIndex(BlockParaIndex))
    {
        BlockParaIndex = ParagraphIndex;
    }

    // Suppress future auto-breaks on that block
    FString BlockId = RenderedParagraphs[BlockParaIndex].BlockId;

    UE_LOG(LogTemp, Warning,
        TEXT("SilentRemove: Suppressing auto-break block %s at paragraph %d"),
        *BlockId, BlockParaIndex);
}




// =============================================================
//  Remove (UI action)
// =============================================================
void SGASScriptPanel::RemovePageBreak(int32 ParagraphIndex)
{
    ApplyScriptEdit({
        EGASScriptEditType::ClearPageBreak,
        ParagraphIndex
        });
}





// =============================================================
//  Popup menu (placeholder)
// =============================================================
void SGASScriptPanel::ShowPageBreakMenu(int32 ParagraphIndex)
{
    FMenuBuilder MenuBuilder(true, nullptr);

    MenuBuilder.AddMenuEntry(
        FText::FromString("Insert Page Break"),
        FText::FromString("Insert a page break here"),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([this, ParagraphIndex]()
                {
                    InsertPageBreak(ParagraphIndex);
                })
        )
    );

    MenuBuilder.AddMenuEntry(
        FText::FromString("Remove Page Break"),
        FText::FromString("Remove page break"),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([this, ParagraphIndex]()
                {
                    RemovePageBreak(ParagraphIndex);
                })
        )
    );

    FSlateApplication::Get().PushMenu(
        AsShared(),                                   // anchor
        FWidgetPath(),
        MenuBuilder.MakeWidget(),
        FSlateApplication::Get().GetCursorPos(),
        FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
    );
}



void SGASScriptPanel::ScrollToParagraph(int32 ParagraphIndex)
{
    // Future: implement scrolling. For now, safe no-op.
}

void SGASScriptPanel::SetShotMarkers(const TArray<FGASMarker>& Markers)
{
    ShotMarkers = Markers;
    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);

}

void SGASScriptPanel::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    // Recompute total height before Slate asks for desired size
    float Y = 0.f;

    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        Y += P.Height;
    }

    CachedTotalHeight = Y;
}
