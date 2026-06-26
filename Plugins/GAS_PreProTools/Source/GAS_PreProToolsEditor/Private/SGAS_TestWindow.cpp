#include "SGAS_TestWindow.h"
#include "SGAS_ScriptTab.h"
#include "SGAS_DirectorView.h"
#include "GAS_ShotListBuilder.h"
#include "GAS_ShotListTypes.h"
#include "SGASScriptPanel.h"
#include "GASPreProLog.h"
#include "GASBlockingAccess.h"
#include "GAS_PreProToolsStyle.h"
#include "GAS_MRQUtils.h"


#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
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
                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                .ToolTipText(FText::FromString(TEXT("Save Script")))
                                .OnClicked_Lambda([this]()
                                    {
                                        if (ScriptTabWidget.IsValid())
                                        {
                                            ScriptTabWidget->OnSaveScript();
                                        }
                                        return FReply::Handled();
                                    })
                                [
                                    SNew(SImage)
                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SaveIcon"))
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
                                        if (!ScriptTabWidget.IsValid())
                                            return FReply::Handled();

                                        // Ask director for output format
                                        const FText Title = FText::FromString(TEXT("Choose Output Format"));
                                        const FText Message = FText::FromString(TEXT("Select render output format:\n\nPNG — standard quality, smaller files\nEXR — maximum quality, larger files"));

                                        EAppReturnType::Type Result = FMessageDialog::Open(
                                            EAppMsgType::YesNo,
                                            Message,
                                            &Title
                                        );

                                        EGASRenderFormat Format = (Result == EAppReturnType::Yes)
                                            ? EGASRenderFormat::PNG
                                            : EGASRenderFormat::EXR;

                                        FGAS_MRQUtils::SendToRenderQueue(
                                            ScriptTabWidget->GetActiveProject(),
                                            Format
                                        );

                                        return FReply::Handled();
                                    })
                                [
                                    SNew(SBox)
                                        .WidthOverride(40.f)
                                        .HeightOverride(40.f)
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SequencerIcon"))
                                        ]
                                ]
                        ]
                    // --- Vertical Separator ---
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(8.f, 4.f)
                        .VAlign(VAlign_Fill)
                        [
                            SNew(SSeparator)
                                .Orientation(Orient_Vertical)
                                .Thickness(1.f)
                        ]

                        // --- Script Tab ---
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.f, 0.f)
                        [
                            SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]()
                                    {
                                        SetActiveTab(EGASMainTab::Script);
                                        return FReply::Handled();
                                    })
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Script")))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                        .ColorAndOpacity_Lambda([this]()
                                            {
                                                return ActiveTab == EGASMainTab::Script
                                                    ? FLinearColor(1.f, 0.8f, 0.2f, 1.f)
                                                    : FLinearColor(0.7f, 0.7f, 0.7f, 1.f);
                                            })
                                ]
                        ]

                    // --- Shot List Tab ---
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.f, 0.f)
                        [
                            SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]()
                                    {
                                        SetActiveTab(EGASMainTab::ShotList);
                                        return FReply::Handled();
                                    })
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Shot List")))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                        .ColorAndOpacity_Lambda([this]()
                                            {
                                                return ActiveTab == EGASMainTab::ShotList
                                                    ? FLinearColor(1.f, 0.8f, 0.2f, 1.f)
                                                    : FLinearColor(0.7f, 0.7f, 0.7f, 1.f);
                                            })
                                ]
                        ]

                    // --- Director View Tab ---
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.f, 0.f)
                        [
                            SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]()
                                    {
                                        SetActiveTab(EGASMainTab::DirectorView);
                                        return FReply::Handled();
                                    })
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("Director View")))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                        .ColorAndOpacity_Lambda([this]()
                                            {
                                                return ActiveTab == EGASMainTab::DirectorView
                                                    ? FLinearColor(1.f, 0.8f, 0.2f, 1.f)
                                                    : FLinearColor(0.7f, 0.7f, 0.7f, 1.f);
                                            })
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


                        ]


                ]

                + SVerticalBox::Slot()
                    .FillHeight(1.f)
                    .Padding(1.f)
                    [
                        SNew(SOverlay)

                            + SOverlay::Slot()
                            [
                                SNew(SBorder)
                                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                    .BorderBackgroundColor_Lambda([this]()
                                        {
                                            if (GASBlockingAccess::IsBlockingActive())
                                                return FLinearColor(0.0f, 0.3f, 1.0f, 0.06f);
                                            return FLinearColor::Transparent;
                                        })
                            ]

                        + SOverlay::Slot()
                            [
                                SNew(SVerticalBox)

                                // --- CONTENT ---
                                + SVerticalBox::Slot()
                                    .FillHeight(1.f)
                                    [
                                        SAssignNew(ContentBox, SBox)
                                            [
                                                SAssignNew(ScriptTabWidget, SGAS_ScriptTab)
                                            ]
                                    ]
                            ]
                    ]
                ];
        WeakScriptTab = ScriptTabWidget;
        ScriptTabWidget->SetMainToolWindow(SharedThis(this));




    // Default to Script tab
    SetActiveTab(EGASMainTab::Script);
}

void SGAS_TestWindow::SetActiveTab(EGASMainTab NewTab)
{
    ActiveTab = NewTab;

    if (!ScriptTabWidget.IsValid() || !ContentBox.IsValid())
        return;

    switch (ActiveTab)
    {
    case EGASMainTab::Script:
        ScriptTabWidget->SetShowShotList(false);
        ContentBox->SetContent(ScriptTabWidget.ToSharedRef());
        break;

    case EGASMainTab::ShotList:
        ScriptTabWidget->SetShowShotList(true);
        ContentBox->SetContent(ScriptTabWidget.ToSharedRef());
        break;

    case EGASMainTab::DirectorView:
        ScriptTabWidget->SetShowShotList(false);
        if (!DirectorViewWidget.IsValid())
        {
            SAssignNew(DirectorViewWidget, SGAS_DirectorView);
            DirectorViewWidget->OnBlockingRequested.BindLambda([this]()
                {
                    if (ScriptTabWidget.IsValid() && DirectorViewWidget.IsValid())
                    {
                        const FString SceneId = DirectorViewWidget->GetActiveSceneId();
                        if (!SceneId.IsEmpty())
                        {
                            ScriptTabWidget->OnStartBlockingScene(SceneId);
                        }
                    }
                });
            DirectorViewWidget->OnSceneSelected.BindLambda([this](const FString& SceneId)
                {
                    if (!ScriptTabWidget.IsValid() || !DirectorViewWidget.IsValid())
                        return;

                    const FGASScript& Script = ScriptTabWidget->GetScript();

                    // Find the scene block
                    const FGASBlock* SceneBlock = nullptr;
                    for (const FGASBlock& Block : Script.Blocks)
                    {
                        if (Block.Id == SceneId && Block.Type == EGASBlockType::SceneHeading)
                        {
                            SceneBlock = &Block;
                            break;
                        }
                    }

                    if (!SceneBlock) return;

                    // Update active scene in Director View
                    DirectorViewWidget->SetActiveScene(SceneId, &Script);

                    // Has blocking — switch to it
                    if (!SceneBlock->BlockingLevelPath.IsEmpty())
                    {
                        ScriptTabWidget->OnStartBlockingScene(SceneId);
                    }
                    else
                    {
                        // No blocking — prompt
                        const FText Title = FText::FromString(TEXT("Start Blocking?"));
                        const FText Message = FText::FromString(TEXT("This scene has no blocking yet.\n\nStart blocking now?"));

                        EAppReturnType::Type Result = FMessageDialog::Open(
                            EAppMsgType::YesNo,
                            Message,
                            &Title
                        );

                        if (Result == EAppReturnType::Yes)
                        {
                            ScriptTabWidget->OnStartBlockingScene(SceneId);
                        }
                    }
                });
            DirectorViewWidget->OnCastButtonClicked.BindLambda([this]()
                {
                    if (ScriptTabWidget.IsValid() && DirectorViewWidget.IsValid())
                    {
                        const FString SceneId = DirectorViewWidget->GetActiveSceneId();
                        if (!SceneId.IsEmpty())
                        {
                            ScriptTabWidget->OpenCastWindowForScene(SceneId);
                        }
                    }
                });
            DirectorViewWidget->OnWatchRequested.BindLambda([this]()
                {
                    UE_LOG(LogTemp, Warning, TEXT("OnWatchRequested LAMBDA FIRED"));
                    if (ScriptTabWidget.IsValid())
                    {
                        ScriptTabWidget->OnOpenMasterSequence();
                    }
                });
            DirectorViewWidget->OnWatchExited.BindLambda([this]()
                {
                    if (ScriptTabWidget.IsValid())
                    {
                        ScriptTabWidget->OnExitWatchMode();
                    }
                });
        }
        if (ScriptTabWidget.IsValid())
        {
            DirectorViewWidget->RefreshSceneList(&ScriptTabWidget->GetScript());
        }
        ContentBox->SetContent(DirectorViewWidget.ToSharedRef());
        break;

    default:
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

// ================================================================
// DIRECTOR VIEW
// ================================================================

void SGAS_TestWindow::SwitchToDirectorView(const FString& SceneId)
{
    if (!ContentBox.IsValid())
        return;

    if (!DirectorViewWidget.IsValid())
    {
        SAssignNew(DirectorViewWidget, SGAS_DirectorView);
        DirectorViewWidget->OnBlockingRequested.BindLambda([this]()
            {
                if (ScriptTabWidget.IsValid() && DirectorViewWidget.IsValid())
                {
                    const FString SceneId = DirectorViewWidget->GetActiveSceneId();
                    if (!SceneId.IsEmpty())
                    {
                        ScriptTabWidget->OnStartBlockingScene(SceneId);
                    }
                }
            });
        DirectorViewWidget->OnCastButtonClicked.BindLambda([this]()
            {
                if (ScriptTabWidget.IsValid() && DirectorViewWidget.IsValid())
                {
                    const FString SceneId = DirectorViewWidget->GetActiveSceneId();
                    if (!SceneId.IsEmpty())
                    {
                        ScriptTabWidget->OpenCastWindowForScene(SceneId);
                    }
                }
            });
        DirectorViewWidget->OnSceneSelected.BindLambda([this](const FString& SceneId)
            {
                if (!ScriptTabWidget.IsValid() || !DirectorViewWidget.IsValid())
                    return;

                const FGASScript& Script = ScriptTabWidget->GetScript();

                // Find the scene block
                const FGASBlock* SceneBlock = nullptr;
                for (const FGASBlock& Block : Script.Blocks)
                {
                    if (Block.Id == SceneId && Block.Type == EGASBlockType::SceneHeading)
                    {
                        SceneBlock = &Block;
                        break;
                    }
                }

                if (!SceneBlock) return;

                // Update active scene in Director View
                DirectorViewWidget->SetActiveScene(SceneId, &Script);

                // Has blocking — switch to it
                if (!SceneBlock->BlockingLevelPath.IsEmpty())
                {
                    ScriptTabWidget->OnStartBlockingScene(SceneId);
                }
                else
                {
                    // No blocking — prompt
                    const FText Title = FText::FromString(TEXT("Start Blocking?"));
                    const FText Message = FText::FromString(TEXT("This scene has no blocking yet.\n\nStart blocking now?"));

                    EAppReturnType::Type Result = FMessageDialog::Open(
                        EAppMsgType::YesNo,
                        Message,
                        &Title
                    );

                    if (Result == EAppReturnType::Yes)
                    {
                        ScriptTabWidget->OnStartBlockingScene(SceneId);
                    }
                }
            });
    }

    // Pass scene ID AND script pointer
    if (ScriptTabWidget.IsValid())
    {
        DirectorViewWidget->SetActiveScene(
            SceneId,
            &ScriptTabWidget->GetScript()
        );
        DirectorViewWidget->RefreshSceneList(&ScriptTabWidget->GetScript());
        ScriptTabWidget->SetShowShotList(false);
    }

    ContentBox->SetContent(DirectorViewWidget.ToSharedRef());
    ActiveTab = EGASMainTab::DirectorView;
}

void SGAS_TestWindow::RefreshDirectorViewCast()
{
    if (DirectorViewWidget.IsValid() && ScriptTabWidget.IsValid())
    {
        DirectorViewWidget->RefreshCast(&ScriptTabWidget->GetScript());
    }
}
