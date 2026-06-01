#include "SGAS_TestWindow.h"
#include "SGAS_ScriptTab.h"
#include "GAS_ShotListBuilder.h"
#include "GAS_ShotListTypes.h"
#include "SGASScriptPanel.h"
#include "GASPreProLog.h"
#include "GASBlockingAccess.h"
#include "GAS_PreProToolsStyle.h"
#include "GAS_MRQUtils.h"


#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"



static TArray<TSharedPtr<FString>> EmptySceneNumberingOptions;


void SGAS_TestWindow::Construct(const FArguments& InArgs)
{
    UE_LOG(LogTemp, Error, TEXT("SGAS_TestWindow CONSTRUCT HIT"));
    ChildSlot
        [
            SNew(SVerticalBox)

                // Tab buttons row
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(1.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4)
                        [
                            SNew(SComboButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .ContentPadding(0.f)

                                .ButtonContent()
                                [
                                    SNew(SBox)
                                        .WidthOverride(28.f)
                                        .HeightOverride(28.f)
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.FileIcon_40"))
                                        ]
                                ]

                                .MenuContent()
                                [
                                    SNew(SVerticalBox)

                                        + SVerticalBox::Slot()
                                        .AutoHeight()
                                        [
                                            SNew(SButton)
                                                .Text(FText::FromString(TEXT("New Project")))
                                                .OnClicked_Lambda([this]()
                                                    {
                                                        if (ScriptTabWidget.IsValid())
                                                        {
                                                            ScriptTabWidget->PromptCreateNewProject();
                                                        }

                                                        return FReply::Handled();
                                                    })
                                        ]

                                    + SVerticalBox::Slot()
                                        .AutoHeight()
                                        [
                                            SNew(SButton)
                                                .Text(FText::FromString(TEXT("Open Project")))
                                                .OnClicked_Lambda([this]()
                                                    {
                                                        if (ScriptTabWidget.IsValid())
                                                        {
                                                            ScriptTabWidget->PromptOpenProject();
                                                        }

                                                        return FReply::Handled();
                                                    })
                                        ]
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.f)
                        [
                            SNew(SButton)

                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")

                                .OnClicked_Lambda([]()
                                    {
                                        FGlobalTabmanager::Get()->TryInvokeTab(
                                            FName("GAS_SetBrowser")
                                        );

                                        return FReply::Handled();
                                    })

                                [
                                    SNew(SImage)
                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SetsIcon_40"))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.f)
                        [
                            SNew(SButton)

                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")

                                .OnClicked_Lambda([]()
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("OVERHEAD CLICKED"));
                                        return FReply::Handled();
                                    })

                                [
                                    SNew(SImage)
                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.OverheadIcon_40"))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.f)
                        [
                            SNew(SButton)

                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")

                                .OnClicked_Lambda([]()
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("LENS TOOL CLICKED"));
                                        return FReply::Handled();
                                    })

                                [
                                    SNew(SImage)
                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.LensIcon_40"))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(12.f, 0.f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SHorizontalBox)

                                // --- LABEL ---
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this]()
                                            {
                                                if (!GASBlockingAccess::IsBlockingActive())
                                                {
                                                    return FText::GetEmpty();
                                                }

                                                if (!ScriptTabWidget.IsValid())
                                                {
                                                    return FText::FromString(TEXT("IN BLOCKING"));
                                                }

                                                const FGASScript& ScriptRef = ScriptTabWidget->GetScript();
                                                const FGuid SceneGuid = GASBlockingAccess::GetActiveSceneId();

                                                if (!SceneGuid.IsValid())
                                                {
                                                    return FText::FromString(TEXT("IN BLOCKING"));
                                                }

                                                int32 ZeroBasedSceneIndex = 0;

                                                for (int32 i = 0; i < ScriptRef.Blocks.Num(); ++i)
                                                {
                                                    const FGASBlock& Block = ScriptRef.Blocks[i];

                                                    if (Block.Type == EGASBlockType::SceneHeading)
                                                    {
                                                        if (Block.Id == SceneGuid.ToString())
                                                        {
                                                            FString NumberPart = GASSceneNumbering::MakeSceneNumber(
                                                                ZeroBasedSceneIndex,
                                                                ScriptRef.SceneNumbering.BaseStyle
                                                            );

                                                            FString Label =
                                                                TEXT("IN BLOCKING — ") +
                                                                NumberPart +
                                                                TEXT("_") +
                                                                Block.Text.ToUpper();

                                                            return FText::FromString(Label);
                                                        }

                                                        ZeroBasedSceneIndex++;
                                                    }
                                                }

                                                return FText::FromString(TEXT("IN BLOCKING"));
                                            })
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                        .ColorAndOpacity(FLinearColor(0.20f, 0.55f, 1.00f, 1.0f))
                                ]

                            // --- EXIT BUTTON ---
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(8.f, 0.f)
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SButton)
                                        .Visibility_Lambda([]()
                                            {
                                                return GASBlockingAccess::IsBlockingActive()
                                                    ? EVisibility::Visible
                                                    : EVisibility::Collapsed;
                                            })
                                        .OnClicked_Lambda([this]()
                                            {
                                                GASBlockingAccess::SetBlockingActive(false);
                                                GASBlockingAccess::SetActiveSceneId(FGuid());

                                                if (ScriptTabWidget.IsValid())
                                                {
                                                    ScriptTabWidget->ResumePendingBlocking(); // or optional reset
                                                }

                                                return FReply::Handled();
                                            })
                                        .ContentPadding(FMargin(6.f, 2.f))
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("EXIT")))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                                .ColorAndOpacity(FLinearColor(1.f, 0.25f, 0.25f, 1.f))
                                        ]
                                ]

                            // --- Send to Render Queue ----------------------------
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(2.f, 0.f)
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)
                                        .ToolTipText(FText::FromString(TEXT("Send to Render Queue")))
                                        .OnClicked_Lambda([this]()
                                            {
                                                if (ScriptTabWidget.IsValid())
                                                {
                                                    FGAS_MRQUtils::SendToRenderQueue(
                                                        ScriptTabWidget->GetActiveProject()
                                                    );
                                                }
                                                return FReply::Handled();
                                            })
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.TracksIcon"))
                                        ]
                                ]
                        ]


                ]

                + SVerticalBox::Slot()
                    .FillHeight(1.f)
                    .Padding(1.f)
                    [
                        SNew(SOverlay)

                            // --- TINT LAYER ---
                            + SOverlay::Slot()
                            [
                                SNew(SBorder)
                                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                    .BorderBackgroundColor_Lambda([this]()
                                        {
                                            if (GASBlockingAccess::IsBlockingActive())
                                            {
                                                return FLinearColor(0.0f, 0.3f, 1.0f, 0.06f); // 6% blue
                                            }

                                            return FLinearColor::Transparent;
                                        })
                            ]

                        // --- CONTENT LAYER ---
                        + SOverlay::Slot()
                            [
                                SAssignNew(ContentBox, SBox)
                                    [
                                        SAssignNew(ScriptTabWidget, SGAS_ScriptTab)
                                    ]
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
        LogGASPrePro,
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
