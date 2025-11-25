#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Custom panel that displays:
 *  - Script lines
 *  - Shot overlays (vertical lines)
 *  - Scene numbers & Dialogue numbers
 */
class SGASScriptPanel : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGASScriptPanel) {}
        SLATE_ARGUMENT(TArray<FString>, ScriptLines)
        SLATE_ARGUMENT(TArray<int32>, ShotStartLines)
        SLATE_ARGUMENT(TArray<int32>, ShotEndLines)
        SLATE_ARGUMENT(TArray<float>, ShotXs)
        SLATE_ARGUMENT(TArray<FString>, SceneNumbers)
        SLATE_ARGUMENT(TArray<FString>, DialogueNumbers)
    SLATE_END_ARGS()

        void Construct(const FArguments& InArgs);

        // Called when the user clicks inside the panel
        virtual FReply OnMouseButtonDown(
            const FGeometry& MyGeometry,
            const FPointerEvent& MouseEvent) override;

        // Update shot overlays from tab
        void SetShots(const TArray<int32>& Starts,
            const TArray<int32>& Ends,
            const TArray<float>& Xs);

        // Update scene & dialogue numbers at runtime
        void SetSceneNumbers(const TArray<FString>& InScenes,
            const TArray<FString>& InDialogues);

        void SetShotTooltips(const TArray<FString>& InTooltips);


protected:
    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

private:

    // Script text
    TArray<FString> ScriptLines;

    // Shot overlays
    TArray<int32> ShotStartLines;
    TArray<int32> ShotEndLines;
    TArray<float> ShotXs;

    // Tooltip data (Shot Number + Shot Type)
    TArray<FString> ShotTooltips;

    // Numbering
    TArray<FString> SceneNumbers;
    TArray<FString> DialogueNumbers;

    // Layout constants
    float LineHeight = 20.f;
    float TopPadding = 30.f;
    float NumberXOffset = -8.f;    // left margin for numbers

    // Script lines container
    TSharedPtr<SVerticalBox> LinesBox;
    // Overlay root (text + tooltips + shot overlays)
    TSharedPtr<SOverlay> RootOverlay;


    // Delegate back to parent tab
public:
    DECLARE_DELEGATE_TwoParams(FOnLineClicked, int32, float);
    FOnLineClicked OnLineClicked;
};
