#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWidget.h"

// -----------------------------------------------------------------------------
// Custom mouse wheel delegate (Slate does NOT expose FOnMouseWheel for SLATE_EVENT)
// -----------------------------------------------------------------------------
DECLARE_DELEGATE_RetVal_TwoParams(
    FReply,
    FScriptMouseWheelDelegate,
    const FGeometry&,
    const FPointerEvent&
);

// -----------------------------------------------------------------------------
// Wheel-capturing wrapper widget
// -----------------------------------------------------------------------------
class SScriptWheelCatcher : public SBorder
{
public:
    SLATE_BEGIN_ARGS(SScriptWheelCatcher)
        {
        }
        SLATE_EVENT(FScriptMouseWheelDelegate, OnMouseWheel)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS();

    void Construct(const FArguments& InArgs)
    {
        OnMouseWheelHandler = InArgs._OnMouseWheel;

        SBorder::Construct(
            SBorder::FArguments()
            [
                InArgs._Content.Widget
            ]
        );
    }

protected:
    virtual FReply OnMouseWheel(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent) override
    {
        return OnMouseWheelHandler.IsBound()
            ? OnMouseWheelHandler.Execute(MyGeometry, MouseEvent)
            : FReply::Unhandled();
    }

private:
    FScriptMouseWheelDelegate OnMouseWheelHandler;
};
