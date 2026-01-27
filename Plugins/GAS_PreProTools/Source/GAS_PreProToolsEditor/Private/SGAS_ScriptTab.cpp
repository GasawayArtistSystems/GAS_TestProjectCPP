#include "SGAS_ScriptTab.h"
#include "GAS_ShotListBuilder.h"
#include "ScriptModel.h"
#include "SGASScriptPanel.h"
#include "GAS_FDXImporter.h"
#include "GASPreProProject.h"
#include "GAS_PreProToolsEditorModule.h"
#include "GAS_ImportNumberingTypes.h"
#include "GAS_ShotListTypes.h"
#include "GAS_ShotIntentTypes.h"
#include "ShotList/GAS_ShotListEighths.h"
#include "SScriptWheelCatcher.h"
#include "ShotList/GAS_ShotListBuilderV2.h"


#include "ScriptLayoutEngine.h"
#include "JsonObjectConverter.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"


#include "GAS_PreProToolsStyle.h"
#include "GASScriptAsset.h"


// JSON persistence is temporarily disabled; we keep FileManager for GetScriptJsonPath.
#include "HAL/FileManager.h"


// =======================================================
// Fixed panel widths (UI constants)
// =======================================================
static constexpr float ScriptPanelWidth = 810.f;
static constexpr float ShotListPanelWidth = 910.f;


// ------------------------------------------------------------
// Import-time numbering prompt (Scene + Shot)
// ------------------------------------------------------------
static FGASImportNumberingOptions PromptImportNumberingOptions()
{
    FGASImportNumberingOptions Options;

    // Defaults (match previous behavior)
    Options.SceneNumberingStyle = EGASNumberingStyle::CountBy10;
    Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_1s;

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Scene & Shot Numbering")))
        .ClientSize(FVector2D(420.f, 320.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    // ------------------------------------------------------------
    // Scene numbering checkboxes
    // ------------------------------------------------------------
    TSharedPtr<SCheckBox> SceneBy10;
    TSharedPtr<SCheckBox> SceneBy1;
    TSharedPtr<SCheckBox> SceneAlpha;
    TSharedPtr<SCheckBox> RespectScriptSceneNumbers;


    // ------------------------------------------------------------
    // Shot numbering checkboxes
    // ------------------------------------------------------------
    TSharedPtr<SCheckBox> ShotBy1;
    TSharedPtr<SCheckBox> ShotBy10;
    TSharedPtr<SCheckBox> ShotAlpha;

    Window->SetContent(
        SNew(SVerticalBox)

        // =========================================================
        // Header
        // =========================================================
        +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(
                    TEXT("Choose how scenes and shots should be numbered.\n")
                    TEXT("(These cannot be changed later.)")
                ))
        ]

    // =========================================================
    // Scene numbering source
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 8.f, 12.f, 4.f)
        [
            SAssignNew(RespectScriptSceneNumbers, SCheckBox)
                .IsChecked(ECheckBoxState::Unchecked) // default: OVERRIDE script numbers
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        Options.bRespectScriptSceneNumbers =
                            (State == ECheckBoxState::Checked);

                        if (State == ECheckBoxState::Checked)
                        {
                            Options.SceneNumberingStyle = EGASNumberingStyle::FromScript;

                            SceneBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                        }
                    })

                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Use scene numbers already in the FDX (if present)")))
                ]
        ]


    // =========================================================
    // Scene Numbering
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 8.f, 12.f, 4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Scene Numbering Style")))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(SceneBy10, SCheckBox)
                .IsChecked(ECheckBoxState::Checked)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            SceneBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.SceneNumberingStyle = EGASNumberingStyle::CountBy10;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 10 (10, 20, 30…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(SceneBy1, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            SceneBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.SceneNumberingStyle = EGASNumberingStyle::CountBy1;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 1 (1, 2, 3…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(SceneAlpha, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            SceneBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.SceneNumberingStyle = EGASNumberingStyle::Alphabetic;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("Alphabetical (A, B, C…)")))
                ]
        ]

    // =========================================================
    // Shot Numbering
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 10.f, 12.f, 4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Shot Numbering Style")))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(ShotBy1, SCheckBox)
                .IsChecked(ECheckBoxState::Checked)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            ShotBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            ShotAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_1s;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 1 (1, 2, 3…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(ShotBy10, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            ShotBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            ShotAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_10s;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 10 (10, 20, 30…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(ShotAlpha, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            ShotBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            ShotBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Alphabetic;
                        }
                    })
                [
                    SNew(STextBlock).Text(
                        FText::FromString(TEXT("Alphabetical (A, B, C… AA after Z)"))
                    )
                ]
        ]

    // =========================================================
    // OK button
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(FMargin(12.f, 12.f, 12.f, 24.f)) // extra bottom padding
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("OK")))
                .OnClicked_Lambda([&]()
                    {
                        Window->RequestDestroyWindow();
                        return FReply::Handled();
                    })
        ]

        );

    FSlateApplication::Get().AddModalWindow(Window, nullptr);

    return Options;
}

void SGAS_ScriptTab::RebuildCastList()
{
    if (!CastListContainer.IsValid())
    {
        return;
    }

    CastListContainer->ClearChildren();

    TArray<TSharedPtr<FString>> EnabledCast;
    GetEnabledCastNames(EnabledCast);

    for (FGASCastMember& Member : CastList)
    {
        CastListContainer->AddSlot()
            .AutoHeight()
            .Padding(2.f)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([&Member]()
                        {
                            return Member.bEnabled
                                ? ECheckBoxState::Checked
                                : ECheckBoxState::Unchecked;
                        })
                    .OnCheckStateChanged_Lambda([this, &Member](ECheckBoxState NewState)
                        {
                            Member.bEnabled = (NewState == ECheckBoxState::Checked);

                            TArray<TSharedPtr<FString>> EnabledNames;
                            GetEnabledCastNames(EnabledNames);

                            if (ScriptPanel.IsValid())
                            {
                                ScriptPanel->SetEnabledCastNames(EnabledNames);
                            }
                        })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Member.Name))
                    ]
            ];
    }

}




FReply SGAS_ScriptTab::OnOpenCastPopup()
{
    TSharedRef<SWindow> CastWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Cast")))
        .ClientSize(FVector2D(320.f, 420.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CastWindow->SetContent(
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .FillHeight(1.f)
        [
            SNew(SBorder)
                .Padding(8.f)
                [
                    SNew(SScrollBox)

                        + SScrollBox::Slot()
                        [
                            SAssignNew(CastListContainer, SVerticalBox)
                        ]
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(8.f, 12.f)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("Close")))
                .OnClicked_Lambda(
                    [CastWindow]()
                    {
                        CastWindow->RequestDestroyWindow();
                        return FReply::Handled();
                    }
                )
        ]
        );

    // Sync enabled cast → ScriptPanel before building UI
    if (ScriptPanel.IsValid())
    {
        TArray<TSharedPtr<FString>> EnabledNames;
        GetEnabledCastNames(EnabledNames);
        ScriptPanel->SetEnabledCastNames(EnabledNames);
    }


    RebuildCastList();

    FSlateApplication::Get().AddWindow(CastWindow);

    return FReply::Handled();
}



void SGAS_ScriptTab::Construct(const FArguments& InArgs)
{

    UE_LOG(LogTemp, Error, TEXT("=== GAS SCRIPT TAB CONSTRUCTED @ %s ==="), *FDateTime::Now().ToString());

    // --------------------------------------------------
    // Scene Numbering dropdown options
    // --------------------------------------------------
    SceneNumberingStyleOptions.Reset();
    SceneNumberingStyleOptions.Add(MakeShared<FString>(TEXT("By 10")));
    SceneNumberingStyleOptions.Add(MakeShared<FString>(TEXT("By 1")));
    SceneNumberingStyleOptions.Add(MakeShared<FString>(TEXT("Alphabetical")));

    // Default selection (match current Script.SceneNumbering if possible later)
    SceneNumberingStyleSelected = SceneNumberingStyleOptions[0];


    ChildSlot
        [
            SNew(SHorizontalBox)

                // =======================================================
                // LEFT PANEL — FIXED WIDTH (NO RESIZING)
                // =======================================================
                +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                        .WidthOverride(60.f)
                        .MinDesiredWidth(60.f)
                        .MaxDesiredWidth(60.f)
                        [
                            SNew(SVerticalBox)

                                // --- Shot Marking Button --------------------------------
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .OnClicked(this, &SGAS_ScriptTab::OnAddShotMarkerClicked)
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.CameraIcon"))
                                                        .ColorAndOpacity(this, &SGAS_ScriptTab::GetShotButtonTint)

                                                ]
                                        ]
                                ]

                            // --- Scene Number Toggle --------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleSceneNumbers))
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SceneNumberIcon"))
                                                        .ColorAndOpacity_Lambda([this]()
                                                            {
                                                                return bShowSceneNumbers
                                                                    ? FLinearColor(0.3f, 1.f, 0.3f, 1.f) // green = active
                                                                    : FLinearColor::White;
                                                            })
                                                ]
                                        ]
                                ]

                            // --- Save Script --------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnSaveScript))
                                                .ToolTipText(FText::FromString("Save Script"))
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SaveIcon"))
                                                ]
                                        ]

                                ]
                            // --- Divider --------------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 12, 0, 12)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.f))
                                        .Padding(FMargin(0.f, 1.f))
                                ]

                                // --- CAST BUTTON -----------------------------------------
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(4.f)
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .ToolTipText(FText::FromString(TEXT("Cast")))
                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnOpenCastPopup))
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.CastIcon"))
                                                ]
                                        ]
                                ]


                            // --- Divider --------------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 12, 0, 12)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.f))
                                        .Padding(FMargin(0.f, 1.f))
                                ]

                            // --- Edit Mode Toggle -----------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)

                                                .IsEnabled_Lambda([this]()
                                                    {
                                                        return CurrentScript.Blocks.Num() > 0;
                                                    })

                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleEditMode))
                                                .ToolTipText_Lambda([this]()
                                                    {
                                                        return bIsEditMode
                                                            ? FText::FromString("Exit Edit Mode")
                                                            : FText::FromString("Enter Edit Mode");
                                                    })
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.EditIcon"))
                                                        .ColorAndOpacity_Lambda([this]()
                                                            {
                                                                return bIsEditMode
                                                                    ? FLinearColor(0.2f, 0.9f, 0.2f, 1.f)   // green when editing
                                                                    : FLinearColor::White;                // normal when not
                                                            })
                                                ]
                                        ]
                                ]

                            // --- Add Mode Toggle -----------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)

                                                .IsEnabled_Lambda([this]()
                                                    {
                                                        return CurrentScript.Blocks.Num() > 0;
                                                    })

                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleAddMode))
                                                .ToolTipText_Lambda([this]()
                                                    {
                                                        return bIsAddMode
                                                            ? FText::FromString("Exit Add Mode")
                                                            : FText::FromString("Enter Add Mode");
                                                    })
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.EditAddIcon")) // you’ll add this icon
                                                        .ColorAndOpacity_Lambda([this]()
                                                            {
                                                                return bIsAddMode
                                                                    ? FLinearColor(0.2f, 0.6f, 1.0f, 1.f)  // blue when adding
                                                                    : FLinearColor::White;
                                                            })
                                                ]
                                        ]
                                ]



                            // --- Divider --------------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 12, 0, 12)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.f))
                                        .Padding(FMargin(0.f, 1.f))
                                ]


                            // --- Import Script --------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)
                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnImportScript))
                                        .ToolTipText(FText::FromString("Import Script"))
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.ImportScript"))
                                        ]

                                ]

                            // --- Generate Page Breaks -------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)

                                        // Enabled ONLY when no page breaks exist
                                        .IsEnabled_Lambda([this]()
                                            {
                                                return CurrentScript.Blocks.Num() > 0
                                                    && CurrentScript.UserPageBreaks.Num() == 0;
                                            })

                                        // Tooltip explains state
                                        .ToolTipText_Lambda([this]()
                                            {
                                                if (CurrentScript.Blocks.Num() == 0)
                                                {
                                                    return FText::FromString("No script loaded.");
                                                }

                                                if (CurrentScript.UserPageBreaks.Num() == 0)
                                                {
                                                    return FText::FromString(
                                                        "Generate approximate page breaks.\n"
                                                        "Page breaks can be edited afterward."
                                                    );
                                                }
                                                else
                                                {
                                                    return FText::FromString(
                                                        "Page breaks already exist.\n"
                                                        "Clear them to regenerate."
                                                    );
                                                }
                                            })




                                        .OnClicked(FOnClicked::CreateSP(
                                            this,
                                            &SGAS_ScriptTab::OnGeneratePageBreaks
                                        ))
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.PageBreakIcon"))
                                        ]

                                ]

                            
                            // --- Clear Script ---------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)
                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnClearScriptConfirm))
                                        .ToolTipText(FText::FromString("Clear Script"))
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.ClearIcon"))
                                        ]

                                ]
                        ]
                ]


            // =======================================================
            // MAIN PANELS (MIDDLE + RIGHT) INSIDE SPLITTER
            // =======================================================
                +SHorizontalBox::Slot()
                    .FillWidth(1.f)
                    [
                        SNew(SHorizontalBox)

                            // =======================================================
                            // SCRIPT PANEL (FIXED WIDTH)
                            // =======================================================
                            +SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                    .WidthOverride(ScriptPanelWidth)
                                    [
                                        SAssignNew(ScriptScrollBox, SScrollBox)
                                            .ScrollBarAlwaysVisible(true)
                                            .ConsumeMouseWheel(EConsumeMouseWheel::Always)
                                            .AllowOverscroll(EAllowOverscroll::No)
                                            .OnUserScrolled_Lambda(
                                                [this](float NewOffset)
                                                {
                                                    UE_LOG(LogTemp, Warning, TEXT("[ScrollBox] UserScrolled = %.2f"), NewOffset);

                                                    if (ScriptPanel.IsValid())
                                                    {
                                                        ScriptPanel->SetExternalScrollOffset(NewOffset);
                                                    }
                                                }
                                            )

                                            + SScrollBox::Slot()
                                            [
                                                SAssignNew(ScriptPanel, SGASScriptPanel)
                                            ]



                                    ]

                            ]

                        // =======================================================
                        // SHOT LIST PANEL (FIXED WIDTH)
                        // =======================================================
                        +SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                    .WidthOverride(ShotListPanelWidth)
                                    [
                                        SNew(SBorder)
                                            .Padding(8.f)
                                            [
                                                SNew(SScrollBox)
                                                    + SScrollBox::Slot()
                                                    [
                                                        SAssignNew(ShotListContainer, SVerticalBox)
                                                    ]
                                            ]
                                    ]
                            ]
                    ]

        ];
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnRequestExternalScroll.BindLambda(
                [this](float NewOffset)
                {
                    if (!ScriptScrollBox.IsValid())
                    {
                        return;
                    }

                    // ScrollBox is the ONLY scroll authority
                    const float ViewOffset =
                        NewOffset - ScriptScrollBox->GetScrollOffset();

                    ScriptScrollBox->SetScrollOffset(ViewOffset + ScriptScrollBox->GetScrollOffset());

                }
            );
        }


        // --------------------------------------------------
        // Bind Shot List rebuild request from ScriptPanel
        // --------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnRequestShotListRebuild.BindLambda(
                [this]()
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ShotListV2] Rebuild requested from ScriptPanel"));
                    RebuildShotList();
                }
            );
        }

        // ------------------------------------------------------------
        // Wire script mutation → project dirty state
        // ------------------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnScriptMutated.BindLambda([this]()
                {
                    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: Script mutated → marking dirty"));

                    MarkScriptDirty();
                    OnShotListNeedsRefresh.Broadcast();
                });


        }

        // ------------------------------------------------------------
        // Wire paragraph click → edit handling
        // ------------------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnParagraphClicked =
                FOnParagraphClicked::CreateSP(
                    this, &SGAS_ScriptTab::OnScriptParagraphClicked
                );

        }


        RebuildShotList();
        LoadScriptFromJsonIfExists();

        if (ScriptPanel.IsValid())
        {
            ScriptPanel->SetScript(&CurrentScript);

            TArray<FRenderedParagraph> Rendered =
                ScriptLayoutEngine::LayoutScript(
                    CurrentScript.Blocks,
                    CachedScriptPanelWidth,
                    CurrentScript.UserPageBreaks,
                    CurrentScript.SceneNumbering
                );

            ScriptPanel->SetRenderedScript(Rendered);
        }

        RebuildShotList();
        // ------------------------------------------------------------
        // TEMP: Create a project asset if none exists
        // ------------------------------------------------------------
        if (!ActiveProject)
        {
            ActiveProject = NewObject<UGASPreProProject>(
                GetTransientPackage(),
                UGASPreProProject::StaticClass()
            );

            UE_LOG(LogTemp, Warning, TEXT("DEBUG: Created transient GAS PrePro Project"));
        }

        RebuildCastUI();

}

static FString ParagraphTypeToString(EGASBlockType Type)
{
    switch (Type)
    {
    case EGASBlockType::SceneHeading:   return TEXT("SceneHeading");
    case EGASBlockType::Action:         return TEXT("Action");
    case EGASBlockType::Character:      return TEXT("Character");
    case EGASBlockType::Dialogue:       return TEXT("Dialogue");
    case EGASBlockType::Parenthetical:  return TEXT("Parenthetical");
    case EGASBlockType::Transition:     return TEXT("Transition");
    default:                             return TEXT("Unknown");
    }
}

static EGASBlockType ParagraphTypeFromString(const FString& Str)
{
    if (Str == TEXT("SceneHeading"))   return EGASBlockType::SceneHeading;
    if (Str == TEXT("Action"))         return EGASBlockType::Action;
    if (Str == TEXT("Character"))      return EGASBlockType::Character;
    if (Str == TEXT("Dialogue"))       return EGASBlockType::Dialogue;
    if (Str == TEXT("Parenthetical"))  return EGASBlockType::Parenthetical;
    if (Str == TEXT("Transition"))     return EGASBlockType::Transition;
    return EGASBlockType::None;
}

static FString GetScriptJsonPath()
{
    const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GAS_Scripts"));
    IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);
    return FPaths::Combine(Folder, TEXT("LastScript.json"));
}


// ============================================================================
// IMPORT SCRIPT — BUTTON HANDLER
// ============================================================================
FReply SGAS_ScriptTab::OnImportScript()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // SAFETY WARNING — before opening file dialog
    // ------------------------------------------------------------
    if (CurrentScript.Blocks.Num() > 0)
    {
        const EAppReturnType::Type Response =
            FMessageDialog::Open(
                EAppMsgType::YesNo,
                FText::FromString(
                    TEXT("A script is already loaded.\n\n"
                        "ALL WORK WILL BE DESTROYED.\n\n"
                        "Are you sure you want to import a NEW script?")
                ),
                FText::FromString(TEXT("Import New Script?"))
            );

        if (Response != EAppReturnType::Yes)
        {
            return FReply::Handled();
        }

        bShotSelectArmed = false;

        if (ScriptPanel.IsValid())
        {
            ScriptPanel->ResetEditorModes();
            ScriptPanel->SetShotAddArmed(false);
        }
    }


    TArray<FString> OutFiles;

    const void* ParentWindow =
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

    bool bOpened = DesktopPlatform->OpenFileDialog(
        ParentWindow,
        TEXT("Select Final Draft Script"),
        FPaths::ProjectDir(),
        TEXT(""),
        TEXT("Final Draft Files (*.fdx)|*.fdx"),
        EFileDialogFlags::None,
        OutFiles
    );

    if (bOpened && OutFiles.Num() > 0)
    {
        // Import replaces the script wholesale
        FGASImportNumberingOptions ImportOptions =
            PromptImportNumberingOptions();

        LoadScriptFromFDX(
            OutFiles[0],
            ImportOptions
        );
        RebuildShotList();


    }

    return FReply::Handled();
}


// ============================================================================
// SAVE SCRIPT — BUTTON HANDLER
// ============================================================================

FReply SGAS_ScriptTab::OnSaveScript()
{
    SaveScriptToJson();
    return FReply::Handled();
}

FReply SGAS_ScriptTab::OnToggleSceneNumbers()
{
    bShowSceneNumbers = !bShowSceneNumbers;

    UE_LOG(LogTemp, Warning,
        TEXT("SCRIPT TAB: Scene numbers %s"),
        bShowSceneNumbers ? TEXT("ON") : TEXT("OFF"));

    float SavedScrollOffset = 0.f;

    if (ScriptScrollBox.IsValid())
    {
        SavedScrollOffset = ScriptScrollBox->GetScrollOffset();
    }

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShowSceneNumbers(bShowSceneNumbers);
        ScriptPanel->RebuildLayout();
    }

    // 🔒 Restore scroll NEXT TICK (after Slate layout settles)
    if (ScriptScrollBox.IsValid())
    {
        TWeakPtr<SScrollBox> WeakScrollBox = ScriptScrollBox;
        const float OffsetToRestore = SavedScrollOffset;

        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([WeakScrollBox, OffsetToRestore](float)
                {
                    if (WeakScrollBox.IsValid())
                    {
                        WeakScrollBox.Pin()->SetScrollOffset(OffsetToRestore);
                    }
                    return false; // one-shot
                })
        );
    }

    return FReply::Handled();
}




FReply SGAS_ScriptTab::OnToggleShotMarking()
{
    // Simple toggle for now – we'll wire this into real behavior later.
    bIsShotMarkingActive = !bIsShotMarkingActive;

    UE_LOG(LogTemp, Log, TEXT("Shot marking %s"),
        bIsShotMarkingActive ? TEXT("ENABLED") : TEXT("DISABLED"));

    return FReply::Handled();
}

FReply SGAS_ScriptTab::OnAddShotMarkerClicked()
{
    bShotSelectArmed = !bShotSelectArmed;

    UE_LOG(LogTemp, Warning, TEXT("[ShotMarker] Shot mode = %s"),
        bShotSelectArmed ? TEXT("ON") : TEXT("OFF"));

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShotAddArmed(bShotSelectArmed);
    }

    return FReply::Handled();
}


// ============================================================================
// LOAD SCRIPT (.FDX) — parse, layout, send to panel
// ============================================================================
void SGAS_ScriptTab::LoadScriptFromFDX(
    const FString& FilePath,
    const FGASImportNumberingOptions& ImportOptions
)

{
    // --------------------------------------------------------------------
    // 0. Confirm overwrite if script already loaded
    // --------------------------------------------------------------------
    if (CurrentScript.Blocks.Num() > 0)

    {
        const FText Title = FText::FromString("Replace script?");
        const FText Message = FText::FromString(
            "Are you sure you want to load a new script?\n"
            "This will overwrite the current script, page breaks, and markers."
        );

        if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title)
            == EAppReturnType::No)
        {
            return;
        }
    }

    // --------------------------------------------------------------------
    // 1. Clear previous script state
    // --------------------------------------------------------------------
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetRenderedScript({});
        ScriptPanel->SetShotMarkers({});
    }

    RebuildShotList();

    // --------------------------------------------------------------------
    // 2. Import FDX into a temporary FGASScript
    // --------------------------------------------------------------------
    FGASScript Script;

    // Import options now come directly from the import dialog
    const FGASImportNumberingOptions& Options = ImportOptions;



    if (!UGAS_FDXImporter::ImportFDXToScript(FilePath, Script, Options))
    {
        UE_LOG(LogTemp, Error, TEXT("FDX import failed: %s"), *FilePath);
        return;
    }

    // Overwrite current in-memory script (JSON-authoritative)
    //CurrentScript = FGASScript();



    // --------------------------------------------------------------------
    // 4. Populate authoritative in-memory script (JSON-backed)
    // --------------------------------------------------------------------
    CurrentScript = Script;
    BuildCastListFromScript();
    RebuildCastUI();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[IMPORT VERIFY] Scene BaseStyle = %d  Style = %d"),
        (int32)CurrentScript.SceneNumbering.BaseStyle,
        (int32)CurrentScript.SceneNumbering.Style
    );
    // ------------------------------------------------------------
    // Persist shot numbering policy into script (CRITICAL)
    // ------------------------------------------------------------
    CurrentScript.ShotNumberingPolicy = ImportOptions.ShotNumberingPolicy;



    CurrentScript.UserPageBreaks.Empty();


    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&CurrentScript);
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("AFTER IMPORT: Blocks=%d  PageBreaks=%d"),
        CurrentScript.Blocks.Num(),
        CurrentScript.UserPageBreaks.Num()
    );


    // --------------------------------------------------------------------
    // 5. Layout script
    // --------------------------------------------------------------------
    float PanelWidth = 1100.f;
    if (ScriptPanel.IsValid())
    {
        PanelWidth = ScriptPanel->GetCachedGeometry().GetLocalSize().X;
        if (PanelWidth < 200.f)
            PanelWidth = 1100.f;
    }

    TArray<FRenderedParagraph> Rendered =
        ScriptLayoutEngine::LayoutScript(
            CurrentScript.Blocks,
            PanelWidth,
            CurrentScript.UserPageBreaks,
            CurrentScript.SceneNumbering
        );

    ScriptPanel->SetRenderedScript(Rendered);

    TArray<FGASShotListSceneRow> SceneRowsV2;

    BuildShotListFromScriptV2(
        CurrentScript,
        CurrentScript.SceneNumbering,
        Rendered,
        SceneRowsV2
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[ShotListV2] Using %d V2 scene rows"),
        SceneRowsV2.Num()
    );



    // --------------------------------------------------------------------
    // 6. Clear markers (FDX has none)
    // --------------------------------------------------------------------

    RebuildShotList();

    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: LoadScriptFromFDX finished"));

    MarkScriptDirty();



    UE_LOG(LogTemp, Log, TEXT("Successfully loaded script from FDX: %s"), *FilePath);
    OnSaveScript(); // make JSON authoritative immediately

}


// NOTE:
// Auto page breaks are a bootstrap helper.
// Final layout uses semantic page breaks only.
// Tune ApproxLinesPerPage (≈53) for Final Draft parity.


FReply SGAS_ScriptTab::OnGeneratePageBreaks()
{
    UE_LOG(LogTemp, Warning, TEXT("Generate Page Breaks clicked"));

    // One-time bootstrap only
    if (CurrentScript.UserPageBreaks.Num() > 0)
    {
        return FReply::Handled();
    }

    int32 LinesOnPage = 0;
    const int32 ApproxLinesPerPage = 54;

    for (int32 BlockIndex = 0; BlockIndex < CurrentScript.Blocks.Num(); ++BlockIndex)
    {
        const FGASBlock& Block = CurrentScript.Blocks[BlockIndex];

        // Ignore right-side dual dialogue
        if (Block.bIsDualDialogue && Block.DualRole == EGASDualRole::Right)
        {
            continue;
        }

        // VERY rough estimate (fine for bootstrap)
        // Estimate line usage based on block type (bootstrap only)
        int32 EstimatedLines = 0;

        switch (Block.Type)
        {
        case EGASBlockType::SceneHeading:
            EstimatedLines = 2;
            break;

        case EGASBlockType::Action:
            EstimatedLines = 3;
            break;

        case EGASBlockType::Character:
            EstimatedLines = 1;
            break;

        case EGASBlockType::Dialogue:
            EstimatedLines = 2;
            break;

        case EGASBlockType::Parenthetical:
            EstimatedLines = 1;
            break;

        case EGASBlockType::Transition:
            EstimatedLines = 1;
            break;

        default:
            EstimatedLines = 1;
            break;
        }

        LinesOnPage += EstimatedLines;


        if (LinesOnPage >= ApproxLinesPerPage)
        {
            FGASUserPageBreak NewBreak;
            NewBreak.AfterBlockId = Block.Id;
            CurrentScript.UserPageBreaks.Add(NewBreak);

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[GEN] PAGE BREAK after BlockId='%s'"),
                *Block.Id
            );

            LinesOnPage = 0; // reset for next page
        }
    }

    // Let normal pipeline rebuild layout
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->RebuildLayout();
    }

    RebuildShotList();
    return FReply::Handled();
    

}

void SGAS_ScriptTab::EnsureScriptSaved()
{
    UE_LOG(LogTemp, Warning, TEXT("EnsureScriptSaved() called"));

    if (CurrentScript.Blocks.Num() == 0)
    {
        return;
    }

    OnSaveScript();
}

void SGAS_ScriptTab::MarkScriptDirty()
{
    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: MarkScriptDirty CALLED"));

    bIsScriptDirty = true;

    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();
}

void SGAS_ScriptTab::ClearScriptDirty()
{
    bIsScriptDirty = false;
}

bool SGAS_ScriptTab::IsScriptDirty() const
{
    return bIsScriptDirty;
}

void SGAS_ScriptTab::ClearScript()
{
    // Clear authoritative in-memory script
    CurrentScript = FGASScript();

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&CurrentScript);
        ScriptPanel->SetRenderedScript({});
    }

    // ✅ Force the parent ScrollBox (the real scroll owner) back to top
    if (ScriptScrollBox.IsValid())
    {
        ScriptScrollBox->SetScrollOffset(0.f);
    }

    RebuildShotList();

    UE_LOG(LogTemp, Warning, TEXT("SCRIPT CLEARED"));
}



FReply SGAS_ScriptTab::OnToggleEditMode()
{
    bIsEditMode = !bIsEditMode;

    UE_LOG(LogTemp, Error,
        TEXT("EDIT MODE TOGGLED: %s"),
        bIsEditMode ? TEXT("ON") : TEXT("OFF")
    );

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetEditMode(bIsEditMode);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("EDIT MODE: ScriptPanel INVALID"));
    }

    return FReply::Handled();
}

void SGAS_ScriptTab::BuildCastListFromScript()
{
    CastList.Empty();

    TSet<FString> UniqueNames;

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Type == EGASBlockType::Character)
        {
            FString Name = Block.Text.TrimStartAndEnd();

            if (!Name.IsEmpty() && !UniqueNames.Contains(Name))
            {
                UniqueNames.Add(Name);

                FGASCastMember Member;
                Member.Name = Name;
                Member.bEnabled = true;

                CastList.Add(Member);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Cast] Built %d cast members"), CastList.Num());
}

void SGAS_ScriptTab::RebuildCastUI()
{
    if (!CastListContainer.IsValid())
        return;

    CastListContainer->ClearChildren();

    if (CastList.Num() == 0)
    {
        CastListContainer->AddSlot()
            .AutoHeight()
            .Padding(4.f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("No characters found")))
                    .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
            ];
        return;
    }

    for (FGASCastMember& Member : CastList)
    {
        CastListContainer->AddSlot()
            .AutoHeight()
            .Padding(2.f)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([&Member]()
                        {
                            return Member.bEnabled
                                ? ECheckBoxState::Checked
                                : ECheckBoxState::Unchecked;
                        })
                    .OnCheckStateChanged_Lambda([&Member](ECheckBoxState State)
                        {
                            Member.bEnabled = (State == ECheckBoxState::Checked);
                        })
                    [
                        SNew(STextBlock)
                            .Text_Lambda([&Member]()
                                {
                                    return FText::FromString(Member.Name);
                                })
                    ]
            ];
    }
}


void SGAS_ScriptTab::GetEnabledCastNames(
    TArray<TSharedPtr<FString>>& OutNames) const
{
    OutNames.Reset();

    for (const FGASCastMember& Member : CastList)
    {
        if (Member.bEnabled && !Member.Name.IsEmpty())
        {
            OutNames.Add(MakeShared<FString>(Member.Name));
        }
    }
}




// ============================================================================
// HELPER FUNCTIONS - Numbering style
// ============================================================================

void SGAS_ScriptTab::ApplySceneNumberingBaseStyle(EGASSceneNumberBaseStyle InBaseStyle)
{
    // Update authoritative model
    CurrentScript.SceneNumbering.BaseStyle = InBaseStyle;

    // Mark dirty so JSON stays in sync
    MarkScriptDirty();

    // Rebuild dependent UI
    RebuildShotList();

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->Invalidate(EInvalidateWidget::LayoutAndVolatility);
    }

}


// ============================================================================
// PARAGRAPH CLICKED — shot marking logic
// ============================================================================
void SGAS_ScriptTab::OnScriptParagraphClicked(int32 BlockIndex)
{
    UE_LOG(LogTemp, Error, TEXT("SCRIPT TAB: Paragraph clicked index=%d"), BlockIndex);

    // Make sure index is valid
    if (!CurrentScript.Blocks.IsValidIndex(BlockIndex))
        return;

    // -------------------------------------------------
    // SHOT MARKING MODE (Phase 1 enforced)
    // -------------------------------------------------
    if (bIsShotMarkingActive)
    {
        const FString& BlockId = CurrentScript.Blocks[BlockIndex].Id;

        FGASMarker NewMarker;
        NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        NewMarker.MarkerType = EGASMarkerType::ShotMarker;
        NewMarker.ShotOrigin = EGASShotOrigin::User;
        NewMarker.BlockId = BlockId;

        NewMarker.Metadata.Add(TEXT("Type"), TEXT("—"));
        NewMarker.Metadata.Add(TEXT("Lens"), TEXT("—"));
        NewMarker.Metadata.Add(TEXT("Camera"), TEXT("Handheld"));
        NewMarker.Metadata.Add(TEXT("Description"), TEXT(""));

        CurrentScript.Markers.Add(NewMarker);

        RebuildShotList();
        MarkScriptDirty();

        return;
    }



    // -------------------------------------------------
    // EDIT MODE (THIS WAS MISSING)
    // -------------------------------------------------
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->TryEditParagraph(BlockIndex);
    }
}


FReply SGAS_ScriptTab::OnClearScriptClicked()
{
    // 🔴 Reset toolbar shot state (controls icon tint)
    bShotSelectArmed = false;

    // 🔴 Reset panel interaction state
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->ResetEditorModes();
        ScriptPanel->SetShotAddArmed(false);
    }

    ClearScript();
    return FReply::Handled();
}




static FString ResolveShotLabel_Stub(
    int32 InSceneIndex,
    int32 InShotIndex,
    EGASShotNumberingPolicy InPolicy
)
{
    // STUB ONLY — numbering logic implemented later
    return FString();
}

// ============================================================================
// Build Shot List UI
// ============================================================================
void SGAS_ScriptTab::RebuildShotList()
{

    // --------------------------------------------------
    // V2 SCENE BUILD (authoritative)
    // --------------------------------------------------
    TArray<FGASShotListSceneRow> SceneRowsV2;
    if (ScriptPanel.IsValid())
    {
        const TArray<FRenderedParagraph>& Rendered =
            ScriptPanel->GetRenderedParagraphs();

        if (Rendered.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShotListV2] RenderedParagraphs is empty"));
        }
        else
        {
            BuildShotListFromScriptV2(
                CurrentScript,
                CurrentScript.SceneNumbering,
                Rendered,
                SceneRowsV2
            );

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[ShotListV2] Built %d scene rows"),
                SceneRowsV2.Num()
            );

        }
    }

#if 0 // LEGACY SHOT LIST PIPELINE (disabled during V2 swap)


    // -------------------------------------------------
    // Helper: find owning SceneBlockIndex for ANY block
    // (walk backward to nearest SceneHeading)
    // -------------------------------------------------
    auto FindOwningSceneBlockIndex =
        [this](const FString& BlockId) -> int32
        {
            if (BlockId.IsEmpty())
            {
                return INDEX_NONE;
            }

            int32 BlockIndex = INDEX_NONE;

            for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
            {
                if (CurrentScript.Blocks[i].Id == BlockId)
                {
                    BlockIndex = i;
                    break;
                }
            }

            if (BlockIndex == INDEX_NONE)
            {
                return INDEX_NONE;
            }

            for (int32 i = BlockIndex; i >= 0; --i)
            {
                if (CurrentScript.Blocks[i].Type == EGASBlockType::SceneHeading)
                {
                    return i;
                }
            }

            return INDEX_NONE;
        };


    // -------------------------------------------------
    // STEP 5 (PART 4): ONE-PASS SHOT BUILD
    // Derived first, User second, scene-local numbering
    // -------------------------------------------------

    TMap<FString, TArray<const FGASMarker*>> UserByScene;

    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        UserByScene.FindOrAdd(Marker.BlockId).Add(&Marker);

    }

    // Walk V2 scenes in authoritative order for shot attachment
    for (const FGASShotListSceneRow& Scene : SceneRowsV2)
    {
        const FString& SceneBlockId = Scene.SceneId;

        // Find legacy SceneBlockIndex from SceneId
        int32 SceneBlockIndex = INDEX_NONE;
        for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
        {
            if (CurrentScript.Blocks[i].Id == SceneBlockId)
            {
                SceneBlockIndex = i;
                break;
            }
        }

        if (!CurrentScript.Blocks.IsValidIndex(SceneBlockIndex))
        {
            continue;
        }

        const TArray<const FGASMarker*>* UserShots =
            UserByScene.Find(SceneBlockId);

        int32 ShotIndexZeroBased = 0;

        if (UserShots)
        {
            for (const FGASMarker* Marker : *UserShots)
            {
                FGASShotDefinitionListRow ShotRow;
                ShotRow.bIsShotRow = true;
                ShotRow.SceneBlockIndex = SceneBlockIndex;
                ShotRow.SceneId = SceneBlockId;
                ShotRow.MarkerId = Marker->Id;

                ShotRow.DisplayName =
                    ResolveShotDisplayLabel(
                        CurrentScript,
                        SceneBlockIndex,
                        ShotIndexZeroBased++
                    );

                ShotListItems.Add(
                    MakeShared<FGASShotDefinitionListRow>(ShotRow)
                );
            }
        }
    }





    // -------------------------------------------------
    // Reorder ShotListItems so shots follow their parent scene deterministically
    // -------------------------------------------------
    {
        TArray<TSharedPtr<FGASShotDefinitionListRow>> OrderedItems;

        // Cache shot rows grouped by SceneBlockIndex
        TMultiMap<FString, TSharedPtr<FGASShotDefinitionListRow>> ShotsByScene;

        for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
        {
            if (Row.IsValid() && Row->bIsShotRow)
            {
                if (CurrentScript.Blocks.IsValidIndex(Row->SceneBlockIndex))
                {
                    const FString& SceneBlockId =
                        CurrentScript.Blocks[Row->SceneBlockIndex].Id;

                    ShotsByScene.Add(SceneBlockId, Row);
                }

            }
        }

        // Walk scenes in original order and append their shots
        for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
        {
            if (!Row.IsValid() || Row->bIsShotRow)
            {
                continue;
            }

            // Scene row first
            OrderedItems.Add(Row);

            // Then its shots — ONLY if scene is expanded
            const int32 SceneBlockIndex = Row->SceneBlockIndex;

            if (CurrentScript.Blocks.IsValidIndex(SceneBlockIndex))
            {
                const FString& SceneBlockId =
                    CurrentScript.Blocks[SceneBlockIndex].Id;

                if (ExpandedScenes.Contains(SceneBlockId))
                {
                    TArray<TSharedPtr<FGASShotDefinitionListRow>> SceneShots;
                    ShotsByScene.MultiFind(SceneBlockId, SceneShots);

                    for (const TSharedPtr<FGASShotDefinitionListRow>& ShotRow : SceneShots)
                    {
                        OrderedItems.Add(ShotRow);
                    }
                }
            }

        }

        ShotListItems = MoveTemp(OrderedItems);
    }

#endif // LEGACY SHOT LIST PIPELINE

    // --------------------------------------------------
    // BUILD SCENE LIST UI (RIGHT PANEL)
    // --------------------------------------------------
    if (!ShotListContainer.IsValid())
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[ShotList UI] Children BEFORE clear = %d"),
        ShotListContainer->GetChildren()->Num()
    );


    ShotListContainer->ClearChildren();
    // =====================================================
    // V2 SCENE HEADER ROW (STATIC)
    // =====================================================
    ShotListContainer->AddSlot()
        .AutoHeight()
        .Padding(2.f, 4.f)
        [
            SNew(SBorder)
                .Padding(4.f)
                .BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Horizontal)

                        // Expand spacer (NO arrow in header)
                        + SSplitter::Slot()
                        .Value(0.06f)
                        [
                            SNew(STextBlock)
                                .Text(FText::GetEmpty())
                        ]

                        // PG
                        + SSplitter::Slot().Value(ColPage)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("PG")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Scene #
                        + SSplitter::Slot().Value(ColScene)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Scene")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Heading
                        + SSplitter::Slot().Value(ColHeading)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Scene Heading")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Length
                        + SSplitter::Slot().Value(ColLength)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("1/8")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Time
                        + SSplitter::Slot().Value(ColTime)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Time")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Set
                        + SSplitter::Slot().Value(ColSet)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Set")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Notes
                        + SSplitter::Slot().Value(ColNotes)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Notes")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]
                ]
        ];

    if (SceneRowsV2.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShotListV2] No scenes to render"));
        return;
    }

    for (const FGASShotListSceneRow& Scene : SceneRowsV2)
    {
        ShotListContainer->AddSlot()
            .AutoHeight()
            .Padding(4.f, 2.f)
            [
                SNew(SSplitter)
                    .Orientation(Orient_Horizontal)

                    // Expand arrow
                    + SSplitter::Slot()
                    .Value(0.06f)
                    [
                        SNew(SButton)
                            .ButtonStyle(FAppStyle::Get(), "NoBorder")
                            .OnClicked_Lambda([this, Scene]()
                                {
                                    if (ExpandedScenes.Contains(Scene.SceneId))
                                        ExpandedScenes.Remove(Scene.SceneId);
                                    else
                                        ExpandedScenes.Add(Scene.SceneId);

                                    RebuildShotList();
                                    return FReply::Handled();
                                })
                            [
                                SNew(STextBlock)
                                    .Text_Lambda([this, Scene]()
                                        {
                                            return ExpandedScenes.Contains(Scene.SceneId)
                                                ? FText::FromString(TEXT("▼"))
                                                : FText::FromString(TEXT("▶"));
                                        })
                            ]
                    ]

                // PG
                + SSplitter::Slot().Value(ColPage)
                    [
                        SNew(STextBlock)
                            .Text(FText::AsNumber(Scene.StartPage))
                    ]

                    // Scene #
                    + SSplitter::Slot().Value(ColScene)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Scene.SceneNumber))
                    ]

                    // Scene Heading (clickable)
                    + SSplitter::Slot().Value(ColHeading)
                    [
                        SNew(SButton)
                            .ButtonStyle(FAppStyle::Get(), "NoBorder")
                            .ContentPadding(FMargin(0))
                            .OnClicked_Lambda([this, SceneId = Scene.SceneId]()
                                {
                                    if (!ScriptPanel.IsValid() || !ScriptScrollBox.IsValid())
                                    {
                                        return FReply::Handled();
                                    }

                                    // Step 1: request the jump
                                    ScriptPanel->ScrollToBlockId(SceneId);

                                    return FReply::Handled();
                                })



                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Scene.SceneHeading.ToUpper()))
                            ]
                    ]


                    // Length
                    + SSplitter::Slot().Value(ColLength)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Scene.FormattedLength))
                    ]

                    // Time (empty for now)
                    + SSplitter::Slot().Value(ColTime)
                    [
                        SNew(STextBlock)
                            .Text(FText::GetEmpty())
                    ]

                    // Set (empty for now)
                    + SSplitter::Slot().Value(ColSet)
                    [
                        SNew(STextBlock)
                            .Text(FText::GetEmpty())
                    ]

                    // Notes (empty for now)
                    + SSplitter::Slot().Value(ColNotes)
                    [
                        SNew(STextBlock)
                            .Text(FText::GetEmpty())
                    ]

            ];

        // --------------------------------------------------
        // SHOTS (only if scene is expanded)
        // --------------------------------------------------
        if (ExpandedScenes.Contains(Scene.SceneId))
        {
            // -----------------------------
            // Shot Header Row (UI only)
            // -----------------------------
            ShotListContainer->AddSlot()
                .AutoHeight()
                .Padding(28.f, 4.f, 4.f, 2.f)
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Horizontal)

                        // SHOT
                        + SSplitter::Slot().Value(ColScene)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("SHOT")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // TYPE (tight)
                        + SSplitter::Slot().Value(ColType)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("TYPE")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // 1/8
                        + SSplitter::Slot().Value(ColLength)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("1/8")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // DESCRIPTION (long, user-added)
                        + SSplitter::Slot().Value(ColDes)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("DESCRIPTION")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // LENS
                        + SSplitter::Slot().Value(ColLens)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("LENS")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // CAMERA (rig / movement flags)
                        + SSplitter::Slot().Value(ColCamera)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("CAMERA")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // NOTES (long)
                        + SSplitter::Slot().Value(ColNotes)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("NOTES")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]
                ];


            // -----------------------------
            // Shot Rows
            // -----------------------------
            for (const FGASShotListShotRow& Shot : Scene.Shots)
            {
                ShotListContainer->AddSlot()
                    .AutoHeight()
                    .Padding(28.f, 2.f, 4.f, 2.f)
                    [
                        SNew(SSplitter)
                            .Orientation(Orient_Horizontal)

                            + SSplitter::Slot().Value(ColScene)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Shot.ShotNumber))
                            ]
                            + SSplitter::Slot().Value(ColType)
                            [
                                SNew(STextBlock)
                                    .Text(
                                        Shot.ShotType.IsEmpty()
                                        ? FText::FromString(TEXT("—"))
                                        : FText::FromString(Shot.ShotType)
                                    )
                            ]
                            + SSplitter::Slot().Value(ColLength)
                            [
                                SNew(STextBlock)
                                    .Text(
                                        Shot.LengthEighths > 0
                                        ? FText::FromString(GAS_FormatPagesEighths(Shot.LengthEighths))
                                        : FText::FromString(TEXT("—"))
                                    )
                            ]
                            + SSplitter::Slot().Value(ColDes)
                                [
                                    SNew(SEditableTextBox)
                                        .Text(FText::FromString(Shot.Description))
                                        .OnTextCommitted_Lambda(
                                            [this, Shot](const FText& NewText, ETextCommit::Type)
                                            {
                                                this->UpdateShotDescription(
                                                    Shot.ShotId,
                                                    NewText.ToString()
                                                );

                                            }
                                        )
                                ]

                            + SSplitter::Slot().Value(ColLens)
                            [
                                SNew(STextBlock)
                                    .Text(
                                        Shot.Lens.IsEmpty()
                                        ? FText::FromString(TEXT("—"))
                                        : FText::FromString(Shot.Lens)
                                    )
                            ]

                            + SSplitter::Slot().Value(ColCamera)
                            [
                                SNew(STextBlock).Text(FText::FromString(Shot.Camera))
                            ]


                            + SSplitter::Slot().Value(ColNotes)
                            [

                                SNew(SMultiLineEditableTextBox)
                                    .Text(FText::FromString(Shot.Notes))
                                    .AutoWrapText(true)
                                    .OnTextCommitted_Lambda(
                                        [this, Shot](const FText& NewText, ETextCommit::Type)
                                        {
                                            this->UpdateShotNotes(
                                                Shot.ShotId,
                                                NewText.ToString()
                                            );
                                        }
                                    )
                            ]
                    ];
            }
        }

    }
    

}



void SGAS_ScriptTab::ScrollToScene(const FGASShotDefinitionListRow& Scene)
{
    if (!ScriptPanel.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ScrollToScene: ScriptPanel invalid"));
        return;
    }

    if (!CurrentScript.Blocks.IsValidIndex(Scene.SceneBlockIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ScrollToScene: invalid SceneBlockIndex %d"),
            Scene.SceneBlockIndex
        );
        return;
    }

    const FString& BlockId =
        CurrentScript.Blocks[Scene.SceneBlockIndex].Id;

    UE_LOG(LogTemp, Warning,
        TEXT("ScrollToScene: SceneBlockIndex=%d  BlockId=%s"),
        Scene.SceneBlockIndex,
        *BlockId
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[SCENE VERIFY] Index=%d Id=%s"),
        Scene.SceneBlockIndex,
        *CurrentScript.Blocks[Scene.SceneBlockIndex].Id
    );


    // ✅ THIS IS THE JUMP
    ScriptPanel->ScrollToBlockId(BlockId);

}

void SGAS_ScriptTab::UpdateShotDescription(
    const FGuid& ShotId,
    const FString& NewDescription)
{
    if (!ScriptPanel.IsValid())
    {
        return;
    }

    ScriptPanel->ModifyShotMarkerMetadata(
        ShotId,
        TEXT("Description"),
        NewDescription
    );

    MarkScriptDirty();
}

void SGAS_ScriptTab::UpdateShotNotes(
    const FGuid& ShotId,
    const FString& NewNotes)
{
    if (!ScriptPanel.IsValid())
    {
        return;
    }

    ScriptPanel->ModifyShotMarkerMetadata(
        ShotId,
        TEXT("Notes"),
        NewNotes
    );

    MarkScriptDirty();
}



void SGAS_ScriptTab::SaveScriptToJson()
{
    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: SaveScriptToJson CALLED"));

    FString JsonPath = GetAuthoritativeScriptJsonPath();

    // Build JSON from FGASScript
    FString OutputString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(CurrentScript, OutputString))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to convert script to JSON."));
        return;
    }

    // Save to file
    if (!FFileHelper::SaveStringToFile(OutputString, *JsonPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to write JSON file: %s"), *JsonPath);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Saved script JSON incl. UserPageBreakParagraphs → %s"), *JsonPath);

    // --- CLEAR DIRTY STATE (this was the missing part) ---
    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.ClearToolDirty();
    bIsScriptDirty = false;
}

FReply SGAS_ScriptTab::OnClearScriptConfirm()
{
    const FText DialogText = FText::FromString(
        TEXT("This will clear your entire script and marks.\nAre you sure?")
    );

    const FText DialogTitle = FText::FromString(TEXT("Confirm Clear"));

    // Correct UE 5.5+ signature:
    EAppReturnType::Type Result = FMessageDialog::Open(
        EAppMsgType::OkCancel,
        DialogText,
        &DialogTitle
    );

    if (Result == EAppReturnType::Ok)
    {
        OnClearScriptClicked();
    }

    return FReply::Handled();
}

FReply SGAS_ScriptTab::OnToggleAddMode()
{
    bIsAddMode = !bIsAddMode;

    UE_LOG(LogTemp, Warning,
        TEXT("ADD MODE TOGGLED: %s"),
        bIsAddMode ? TEXT("ON") : TEXT("OFF")
    );

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetAddMode(bIsAddMode);
    }

    return FReply::Handled();
}

void SGAS_ScriptTab::LoadScriptFromJsonIfExists()
{
    const FString ScriptPath = GetAuthoritativeScriptJsonPath();

    UE_LOG(LogTemp, Warning, TEXT("[GAS] Looking for script JSON at: %s"), *ScriptPath);

    if (!FPaths::FileExists(ScriptPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GAS] No saved script JSON found."));
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *ScriptPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[GAS] Failed to read script JSON: %s"), *ScriptPath);
        return;
    }

    FGASScript Loaded;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Loaded, 0, 0))
    {
        UE_LOG(LogTemp, Error, TEXT("[GAS] Failed to parse script JSON: %s"), *ScriptPath);
        return;
    }

    CurrentScript = MoveTemp(Loaded);
    BuildCastListFromScript();
    RebuildCastUI();


    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[GAS] Loaded script JSON OK: Blocks=%d PageBreaks=%d Markers=%d"),
        CurrentScript.Blocks.Num(),
        CurrentScript.UserPageBreaks.Num(),
        CurrentScript.Markers.Num()
    );

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&CurrentScript);

        TArray<FRenderedParagraph> Rendered =
            ScriptLayoutEngine::LayoutScript(
                CurrentScript.Blocks,
                CachedScriptPanelWidth,
                CurrentScript.UserPageBreaks,
                CurrentScript.SceneNumbering
            );

        ScriptPanel->SetRenderedScript(Rendered);
    }

    RebuildShotList();

}

FString SGAS_ScriptTab::GetAuthoritativeScriptJsonPath() const
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GAS"));
    IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
    return FPaths::Combine(Dir, TEXT("CurrentScript.gasjson"));
}

void SGAS_ScriptTab::ScrollToSceneBlock(const FString& BlockId)
{
    if (!ScriptPanel.IsValid())
        return;

    ScriptPanel->ScrollToBlockId(BlockId);
}

void SGAS_ScriptTab::AddShotMarkerForScene(const FString& SceneBlockId)
{
    FGASMarker NewMarker;

    if (!FGASShotMarkerFactory::CreateShotMarkerForSceneHeading(
        CurrentScript.Blocks,
        SceneBlockId,
        NewMarker))
    {
        return;
    }

    CurrentScript.Markers.Add(NewMarker);


    RebuildShotList();
}

FSlateColor SGAS_ScriptTab::GetShotButtonTint() const
{
    return bShotSelectArmed
        ? FLinearColor(0.25f, 0.6f, 1.f, 1.0f)   // 🔵 shot mode armed
        : FLinearColor(0.7f, 0.7f, 0.7f, 0.8f);  // ⚪ idle
}
