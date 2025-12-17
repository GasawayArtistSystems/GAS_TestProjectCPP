#include "SGAS_TestWindow.h"
#include "SGAS_ScriptTab.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

void SGAS_TestWindow::Construct(const FArguments& InArgs)
{
    ChildSlot
        [
            SNew(SVerticalBox)

                // Tab buttons row
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.f, 0.f)
                        [
                            SNew(SButton)
                                .Text_Lambda([this]()
                                    {
                                        return GetScriptTabLabel();
                                    })
                                .OnClicked_Lambda([this]()
                                    {
                                        SetActiveTab(EGASMainTab::Script);
                                        return FReply::Handled();
                                    })

                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.f, 0.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Shot List")))
                                .OnClicked_Lambda([this]()
                                    {
                                        SetActiveTab(EGASMainTab::ShotList);
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.f, 0.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Shots")))
                                    .OnClicked_Lambda([this]()
                                        {
                                            SetActiveTab(EGASMainTab::Shots);
                                            return FReply::Handled();
                                        })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.f, 0.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Edit")))
                                .OnClicked_Lambda([this]()
                                    {
                                        SetActiveTab(EGASMainTab::Edit);
                                        return FReply::Handled();
                                    })
                        ]
                ]

            // Active tab content
            + SVerticalBox::Slot()
                .FillHeight(1.f)
                .Padding(4.f)
                [
                    SAssignNew(ContentBox, SBox)
                ]
        ];

    // Default to Script tab
    SetActiveTab(EGASMainTab::Script);
}

void SGAS_TestWindow::SetActiveTab(EGASMainTab NewTab)
{
    ActiveTab = NewTab;

    if (!ContentBox.IsValid())
    {
        return;
    }

    switch (ActiveTab)
    {
    case EGASMainTab::Script:
        ContentBox->SetContent(CreateScriptTabContent());
        break;

    case EGASMainTab::ShotList:
        ContentBox->SetContent(CreateShotListTabContent());
        break;

    case EGASMainTab::Shots:
        ContentBox->SetContent(CreateShotsTabContent());
        break;

    case EGASMainTab::Edit:
        ContentBox->SetContent(CreateEditTabContent());
        break;

    default:
        break;
    }
}

TSharedRef<SWidget> SGAS_TestWindow::CreateScriptTabContent()
{
    TSharedRef<SGAS_ScriptTab> ScriptTab =
        SNew(SGAS_ScriptTab);

    // ------------------------------------------------------------
    // Receive project ownership from the Script tab
    // ------------------------------------------------------------
    ScriptTab->OnProjectLoaded.BindLambda(
        [this](UGASPreProProject* InProject)
        {
            ActiveProject = InProject;

            UE_LOG(LogTemp, Warning, TEXT("Window received ActiveProject"));
        }
    );

    return SNew(SBorder)
        .Padding(4.f)
        [
            ScriptTab
        ];
}


TSharedRef<SWidget> SGAS_TestWindow::CreateShotListTabContent()
{
    return SNew(SBorder)
        .Padding(4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Shot List tab (coming soon)")))
        ];
}

TSharedRef<SWidget> SGAS_TestWindow::CreateShotsTabContent()
{
    return SNew(SBorder)
        .Padding(4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Shots tab (coming soon)")))
        ];
}

TSharedRef<SWidget> SGAS_TestWindow::CreateEditTabContent()
{
    return SNew(SBorder)
        .Padding(4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Edit tab (coming soon)")))
        ];
}


FText SGAS_TestWindow::GetScriptTabLabel() const
{

    const bool bDirty =
        (ActiveProject && ActiveProject->IsDirty());

    return FText::FromString(
        bDirty ? TEXT("Script*") : TEXT("Script")
    );
}

