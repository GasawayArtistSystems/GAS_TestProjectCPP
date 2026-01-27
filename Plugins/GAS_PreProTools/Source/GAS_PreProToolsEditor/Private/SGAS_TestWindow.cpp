#include "SGAS_TestWindow.h"
#include "SGAS_ScriptTab.h"
#include "GAS_ShotListBuilder.h"
#include "GAS_ShotListTypes.h"
#include "SGASScriptPanel.h"


#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"


static TArray<TSharedPtr<FString>> EmptySceneNumberingOptions;


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
                        [
                            SAssignNew(ScriptTabWidget, SGAS_ScriptTab)
                        ]

                ]
        ];
        WeakScriptTab = ScriptTabWidget;




    // Default to Script tab
    SetActiveTab(EGASMainTab::Script);
}

void SGAS_TestWindow::SetActiveTab(EGASMainTab NewTab)
{
    ActiveTab = NewTab;

    // We no longer swap widgets inside ContentBox.
    // ContentBox always contains ScriptTabWidget.
    if (!ScriptTabWidget.IsValid())
    {
        return;
    }

    // For now: only control whether the right-side Shot List panel is visible.
    // Script is always visible.
    switch (ActiveTab)
    {
    case EGASMainTab::Script:
        ScriptTabWidget->SetShowShotList(false);
        break;

    case EGASMainTab::ShotList:
        ScriptTabWidget->SetShowShotList(true);
        break;

    default:
        // We'll wire Shots/Edit later.
        break;
    }
}


TSharedRef<SWidget> SGAS_TestWindow::CreateScriptTabContent()
{
    ScriptTab = SNew(SGAS_ScriptTab);

    // Project load
    ScriptTab->OnProjectLoaded.BindLambda(
        [this](UGASPreProProject* InProject)
        {
            ActiveProject = InProject;
            UE_LOG(LogTemp, Warning, TEXT("Window received ActiveProject"));
        }
    );

    // 🔑 Script mutation → refresh shot list
    ScriptTab->OnShotListNeedsRefresh.AddSP(
        this,
        &SGAS_TestWindow::RefreshShotList
    );

    return SNew(SBorder)
        .Padding(4.f)
        [
            ScriptTab.ToSharedRef()
        ];
}




TSharedRef<SWidget> SGAS_TestWindow::CreateShotListTabContent()
{

    ShotListItems.Reset();

    TArray<FGASShotDefinitionListRow> SceneRows;
    BuildShotListFromJson(SceneRows);

    for (const FGASShotDefinitionListRow& Row : SceneRows)
    {
        ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(Row));
    }



    return SNew(SBorder)
        .Padding(4.f)
        [
            SAssignNew(ShotListView, SListView<TSharedPtr<FGASShotDefinitionListRow>>)
                .ItemHeight(24.f)
                .ListItemsSource(&ShotListItems)
                .OnGenerateRow(this, &SGAS_TestWindow::GenerateShotListRow)
        ];

}


TSharedRef<ITableRow> SGAS_TestWindow::GenerateShotListRow(
    TSharedPtr<FGASShotDefinitionListRow> InItem,
    const TSharedRef<STableViewBase>& OwnerTable)
{

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[UI ROW] Title='%s' Number='%s'"),
        *InItem->SceneTitle,
        *InItem->SceneNumber
    );

    return SNew(STableRow<TSharedPtr<FGASShotDefinitionListRow>>, OwnerTable)
        [
            SNew(SButton)
                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                .OnClicked_Lambda([this, InItem]()
                    {
                        if (OnSceneSelected.IsBound())
                        {
                            OnSceneSelected.Execute(InItem->SceneBlockIndex);
                        }
                        return FReply::Handled();
                    })
                [
                    SNew(SBorder)
                        .Padding(FMargin(6.f, 3.f))
                        .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(4.f, 0.f, 12.f, 0.f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(InItem->SceneNumber))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                        .ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f))
                                ]


                                + SHorizontalBox::Slot()
                                .FillWidth(1.f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(InItem->SceneTitle))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                                        .AutoWrapText(false)
                                        .Clipping(EWidgetClipping::ClipToBounds)
                                ]
                        ]
                ]
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

void SGAS_TestWindow::RefreshShotList()
{
    ShotListItems.Reset();

    TArray<FGASShotDefinitionListRow> SceneRows;
    BuildShotListFromJson(SceneRows);

    for (const FGASShotDefinitionListRow& Row : SceneRows)
    {
        ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(Row));
    }

    if (ShotListView.IsValid())
    {
        ShotListView->RequestListRefresh();
    }
}
