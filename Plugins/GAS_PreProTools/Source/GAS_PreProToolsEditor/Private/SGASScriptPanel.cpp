#include "SGASScriptPanel.h"
#include "ScriptLayoutEngine.h"
#include "GAS_FDXImporter.h"
#include "GASPreProProject.h"
#include "Rendering/DrawElements.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/AppStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Internationalization/Text.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Input/Reply.h"
#include "Input/Events.h"

// ============================================================================
// Construct
// ============================================================================

static_assert(true, "USING THIS SGASScriptPanel.cpp FILE");

void SGASScriptPanel::Construct(const FArguments& InArgs)
{
    OnParagraphClicked = InArgs._OnParagraphClicked;

    RenderedParagraphs.Empty();
    ShotStartParagraphs.Empty();
    ShotEndParagraphs.Empty();
    HoveredShotIndex = INDEX_NONE;
    SelectedShotIndex = INDEX_NONE;

    ChildSlot[
        SNew(SBox)
            .WidthOverride(800.f)
            [
                SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        // Dummy area until SetRenderedScript is called
                        SNew(STextBlock).Text(FText::FromString(TEXT("Loading script...")))
                    ]
            ]
    ];
}

// ============================================================================
// Supply computed paragraphs
// ============================================================================
void SGASScriptPanel::SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs)
{
    RenderedParagraphs = InParagraphs;
    UE_LOG(LogTemp, Warning, TEXT("SCRIPT PANEL RECEIVED %d PARAGRAPHS"), RenderedParagraphs.Num());

    // Compute total scroll height
    if (RenderedParagraphs.Num() > 0)
    {
        const FRenderedParagraph& Last = RenderedParagraphs.Last();
        CachedTotalHeight = Last.TopY + Last.Height + 200.f;
    }
    else
    {
        CachedTotalHeight = 0.f;
    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

// ============================================================================
// Scroll to a paragraph (if wrapped in a ScrollBox externally)
// ============================================================================
void SGASScriptPanel::ScrollToParagraph(int32 ParagraphIndex)
{
    // No internal scrollbox reference here (optional future feature)
}

// ============================================================================
// Mouse Down — detect paragraph or marker hit
// ============================================================================
FReply SGASScriptPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    float MouseX = Local.X;
    float MouseY = Local.Y;

    // 1. Detect paragraph click
    for (int32 i = 0; i < RenderedParagraphs.Num(); i++)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (MouseY >= P.TopY && MouseY <= (P.TopY + P.Height))
        {
            if (OnParagraphClicked.IsBound())
            {
                OnParagraphClicked.Execute(i);
                return FReply::Handled();
            }
        }

    }

    // 2. Detect shot marker hit (right side of panel)
    float MarkerXMin = MyGeometry.GetLocalSize().X - 60.f;
    float MarkerXMax = MyGeometry.GetLocalSize().X;

    for (int32 i = 0; i < ShotStartParagraphs.Num(); i++)
    {
        if (!RenderedParagraphs.IsValidIndex(ShotStartParagraphs[i]) ||
            !RenderedParagraphs.IsValidIndex(ShotEndParagraphs[i]))
        {
            continue;
        }

        const FRenderedParagraph& StartP = RenderedParagraphs[ShotStartParagraphs[i]];
        const FRenderedParagraph& EndP = RenderedParagraphs[ShotEndParagraphs[i]];

        float Y1 = StartP.TopY;
        float Y2 = EndP.TopY + EndP.Height;

        bool bInX = (MouseX >= MarkerXMin && MouseX <= MarkerXMax);
        bool bInY = (MouseY >= Y1 && MouseY <= Y2);

        if (bInX && bInY)
        {
            SelectedShotIndex = i;
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}

// ============================================================================
// Desired Size (vertical grows based on paginated paragraphs)
// ============================================================================
FVector2D SGASScriptPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    float TotalHeight = 0.f;

    // Sum paragraph heights
    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        TotalHeight += P.Height + 10.f;   // add your vertical spacing
    }

    // Guarantee a minimum width, but height reflects entire script
    return FVector2D(800.f, TotalHeight);
}

// ============================================================================
// ON PAINT — render the paragraphs, wrapped lines, alignment, and shot markers
// ============================================================================
int32 SGASScriptPanel::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    const float PanelWidth = AllottedGeometry.GetLocalSize().X;

    // Screenplay formatting constants (from ScriptLayoutEngine.h)
    const float PageWidth = GFD_PageWidth;
    const float MarginLeft = GFD_MarginLeft;
    const float MarginRight = GFD_MarginRight;
    const float LineHeight = GFD_LineHeight;

    // How much space does the text column have?
    const float UsableWidth = PageWidth - MarginLeft - MarginRight;

    // Clip drawing to widget bounds
    OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.GetLayoutBoundingRect()));

    if (RenderedParagraphs.Num() == 0)
    {
        OutDrawElements.PopClip();
        return LayerId;
    }

    // Standard font
    const FSlateFontInfo FontInfo = FAppStyle::Get().GetFontStyle("NormalFont");

    // Keep text ABOVE the UI (avoids flicker/overlap)
    int32 TextLayer = LayerId + 900;

    // ------------------------------------------------------------------------
    // DRAW EACH PARAGRAPH
    // ------------------------------------------------------------------------
    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        float Y = P.TopY;

        for (int32 i = 0; i < P.Lines.Num(); i++)
        {
            const FString& Line = P.Lines[i];

            FVector2D DrawPos(MarginLeft + P.IndentLeft, Y);

            // --- Draw text ---
            FPaintGeometry TextGeo = AllottedGeometry.ToPaintGeometry(
                FVector2f(1.f, 1.f),
                FSlateLayoutTransform(DrawPos)
            );

            FSlateDrawElement::MakeText(
                OutDrawElements,
                TextLayer + 1,
                TextGeo,
                FText::FromString(Line),
                FontInfo,
                ESlateDrawEffect::None,
                FLinearColor::White
            );

            // Advance only ONE line normally
            Y += LineHeight;

            // SPECIAL CASE: Character → Dialogue should NOT add extra vertical space
            if (P.BlockType == EGASBlockType::Character)
            {
                // Do NOT add extra spacing after CHARACTER name
                // The default +LineHeight above is the only line we want.
            }
        }


        TextLayer++;
    }

    // ------------------------------------------------------------------------
    // SHOT MARKERS (UNCHANGED)
    // ------------------------------------------------------------------------
    int32 MarkerLayer = TextLayer + 200;

    for (int32 i = 0; i < ShotStartParagraphs.Num(); i++)
    {
        const int32 StartIdx = ShotStartParagraphs[i];
        const int32 EndIdx = ShotEndParagraphs[i];

        if (!RenderedParagraphs.IsValidIndex(StartIdx) ||
            !RenderedParagraphs.IsValidIndex(EndIdx))
        {
            continue;
        }

        const FRenderedParagraph& Start = RenderedParagraphs[StartIdx];
        const FRenderedParagraph& End = RenderedParagraphs[EndIdx];

        float Y1 = Start.TopY + 4.f;
        float Y2 = End.TopY + End.Height - 4.f;
        float X = PanelWidth - 40.f;

        // Circles
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MarkerLayer,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(16.f, 16.f),
                FSlateLayoutTransform(FVector2D(X, Y1))),
            FAppStyle::Get().GetBrush("Icons.Circle"),
            ESlateDrawEffect::None,
            FLinearColor::Blue
        );

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            MarkerLayer,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(16.f, 16.f),
                FSlateLayoutTransform(FVector2D(X, Y2))),
            FAppStyle::Get().GetBrush("Icons.Circle"),
            ESlateDrawEffect::None,
            FLinearColor::Blue
        );

        // Connecting line
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            MarkerLayer + 1,
            AllottedGeometry.ToPaintGeometry(),
            { FVector2D(X + 8.f, Y1 + 8.f), FVector2D(X + 8.f, Y2 + 8.f) },
            ESlateDrawEffect::None,
            FLinearColor::Blue,
            true,
            2.f
        );
    }

    OutDrawElements.PopClip();
    return TextLayer;
}


