#include "SGASScriptPanel.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Rendering/DrawElements.h"
#include "Input/Reply.h"
#include "Input/Events.h"

//
// ============================================================
// Construct
// ============================================================
void SGASScriptPanel::Construct(const FArguments& InArgs)
{
    ScriptLines = InArgs._ScriptLines;
    ShotStartLines = InArgs._ShotStartLines;
    ShotEndLines = InArgs._ShotEndLines;
    ShotXs = InArgs._ShotXs;

    SceneNumbers = InArgs._SceneNumbers;
    DialogueNumbers = InArgs._DialogueNumbers;

    // ---------------------------------------------------------
    // Build TEXT column
    // ---------------------------------------------------------
    SAssignNew(LinesBox, SVerticalBox);

    for (int32 i = 0; i < ScriptLines.Num(); ++i)
    {
        LinesBox->AddSlot()
            .AutoHeight()
            .Padding(FMargin(40.f, 2.f))  // space left for numbers
            [
                SNew(STextBlock)
                    .Text(FText::FromString(ScriptLines[i]))
                    .AutoWrapText(true)
            ];
    }

    // ---------------------------------------------------------
    // Build overlay: text + (later) tooltip heads
    // ---------------------------------------------------------
    SAssignNew(RootOverlay, SOverlay)

        + SOverlay::Slot()
        [
            LinesBox.ToSharedRef()
        ];


    ChildSlot
        [
            RootOverlay.ToSharedRef()
        ];
}

//
// ============================================================
// SetShots — parent updates overlays
// ============================================================
void SGASScriptPanel::SetShots(const TArray<int32>& Starts,
    const TArray<int32>& Ends,
    const TArray<float>& Xs)
{
    ShotStartLines = Starts;
    ShotEndLines = Ends;
    ShotXs = Xs;

    if (RootOverlay.IsValid())
    {
        RootOverlay->ClearChildren();

        // Add text lines back
        RootOverlay->AddSlot()
            [
                LinesBox.ToSharedRef()
            ];

        // Tooltip hitboxes (ShotTooltips must match ShotStartLines length)
        for (int32 i = 0; i < ShotStartLines.Num(); i++)
        {
            float Y1 = ShotStartLines[i] * (LineHeight + 4.f) + TopPadding;
            float X = ShotXs[i];

            FVector2D BoxSize(60.f, 22.f);
            FVector2D BoxPos(X - BoxSize.X * 0.5f,
                Y1 - BoxSize.Y - 6.f);

            FString Tooltip = ShotTooltips.IsValidIndex(i)
                ? ShotTooltips[i]
                : FString::Printf(TEXT("Shot %03d — Unknown"), i + 1);

            RootOverlay->AddSlot()
                .ZOrder(10 + i)
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Top)
                .Padding(BoxPos.X, BoxPos.Y, 0, 0)
                [
                    SNew(SBox)
                        .WidthOverride(BoxSize.X)
                        .HeightOverride(BoxSize.Y)
                        .Visibility(EVisibility::Visible)
                        .ToolTipText(FText::FromString(Tooltip))
                ];

        }


    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SGASScriptPanel::SetShotTooltips(const TArray<FString>& InTooltips)
{
    ShotTooltips = InTooltips;
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


//
// ============================================================
// SetSceneNumbers — parent updates numbering arrays
// ============================================================
void SGASScriptPanel::SetSceneNumbers(const TArray<FString>& InScenes,
    const TArray<FString>& InDialogues)
{
    SceneNumbers = InScenes;
    DialogueNumbers = InDialogues;

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

//
// ============================================================
// Mouse detection — convert click to line + X
// ============================================================
FReply SGASScriptPanel::OnMouseButtonDown(const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        float AdjustedY = LocalPos.Y - TopPadding;
        AdjustedY = FMath::Max(0.f, AdjustedY);

        int32 LineIndex = FMath::FloorToInt(AdjustedY / (LineHeight + 4.f));
        float ClickX = LocalPos.X;

        if (OnLineClicked.IsBound())
            OnLineClicked.Execute(LineIndex, ClickX);

        return FReply::Handled();
    }

    return FReply::Unhandled();
}

//
// ============================================================
// OnPaint — draws:
//   • dim overlay
//   • shot lines, ticks, labels
//   • scene numbers + dialogue numbers
// ============================================================
int32 SGASScriptPanel::OnPaint(const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const
{
    // Draw text normally
    int32 TextLayer = SCompoundWidget::OnPaint(
        Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    int32 OverlayLayer = TextLayer + 10;

    // ---------------------------------------------------------
    // Draw Scene & Dialogue Numbers to the LEFT of the text
    // ---------------------------------------------------------
    for (int32 i = 0; i < ScriptLines.Num(); i++)
    {
        float Y = i * (LineHeight + 4.f) + TopPadding;

        FString Scene = (SceneNumbers.IsValidIndex(i)) ? SceneNumbers[i] : TEXT("");
        FString Dial = (DialogueNumbers.IsValidIndex(i)) ? DialogueNumbers[i] : TEXT("");

        FString Combined;
        if (!Scene.IsEmpty())
            Combined = Scene;
        else if (!Dial.IsEmpty())
            Combined = Dial;

        if (!Combined.IsEmpty())
        {
            FVector2D Pos(NumberXOffset, Y);
            FSlateDrawElement::MakeText(
                OutDrawElements,
                OverlayLayer + 2,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(100.f, LineHeight),
                    FSlateLayoutTransform(Pos)),
                Combined,
                FCoreStyle::GetDefaultFontStyle("Regular", 10),
                ESlateDrawEffect::None,
                FLinearColor::Gray
            );
        }
    }

    // ---------------------------------------------------------
    // Draw shot overlays
    // ---------------------------------------------------------
    for (int32 i = 0; i < ShotStartLines.Num(); i++)
    {
        int32 StartLine = ShotStartLines[i];
        int32 EndLine = ShotEndLines[i];
        float LineX = ShotXs[i];

        float Y1 = StartLine * (LineHeight + 4.f) + TopPadding;
        float Y2 = (EndLine + 1) * (LineHeight + 4.f) + TopPadding;

        FLinearColor ShotColor(0.0f, 0.85f, 0.95f, 1.0f);

        // Vertical line
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            OverlayLayer + 3,
            AllottedGeometry.ToPaintGeometry(),
            { FVector2D(LineX, Y1), FVector2D(LineX, Y2) },
            ESlateDrawEffect::None,
            ShotColor,
            true,
            3.f);

        // Start tick
        float TickHalf = 12.f;
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            OverlayLayer + 4,
            AllottedGeometry.ToPaintGeometry(),
            { FVector2D(LineX - TickHalf, Y1), FVector2D(LineX + TickHalf, Y1) },
            ESlateDrawEffect::None,
            ShotColor,
            true,
            2.f);

        // End tick
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            OverlayLayer + 4,
            AllottedGeometry.ToPaintGeometry(),
            { FVector2D(LineX - TickHalf, Y2), FVector2D(LineX + TickHalf, Y2) },
            ESlateDrawEffect::None,
            ShotColor,
            true,
            2.f);

        // Floating label box
        FVector2D BoxSize(60.f, 22.f);
        FVector2D BoxPos(LineX - BoxSize.X * 0.5f, Y1 - BoxSize.Y - 6.f);

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            OverlayLayer + 5,
            AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxPos)),
            FCoreStyle::Get().GetDefaultBrush(),
            ESlateDrawEffect::None,
            ShotColor);

        FString Label = FString::Printf(TEXT("S%03d"), i + 1);

        FSlateDrawElement::MakeText(
            OutDrawElements,
            OverlayLayer + 6,
            AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxPos)),
            Label,
            FCoreStyle::GetDefaultFontStyle("Regular", 10),
            ESlateDrawEffect::None,
            FLinearColor::Black);
    }

    return OverlayLayer + 20;
}
