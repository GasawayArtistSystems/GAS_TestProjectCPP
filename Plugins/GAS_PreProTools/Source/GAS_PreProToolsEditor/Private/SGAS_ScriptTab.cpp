#include "SGAS_ScriptTab.h"
#include "GAS_ShotListBuilder.h"
#include "ScriptModel.h"
#include "SGASScriptPanel.h"
#include "GAS_FDXImporter.h"
#include "GASPreProProject.h"
#include "GAS_PreProToolsEditorModule.h"
#include "GAS_ImportNumberingTypes.h"
#include "GAS_ShotListTypes.h"
#include "SScriptWheelCatcher.h"


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
        .ClientSize(FVector2D(420.f, 300.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    // ------------------------------------------------------------
    // Scene numbering checkboxes
    // ------------------------------------------------------------
    TSharedPtr<SCheckBox> SceneBy10;
    TSharedPtr<SCheckBox> SceneBy1;
    TSharedPtr<SCheckBox> SceneAlpha;

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
        .Padding(12.f)
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
                    SNew(SSplitter)

                        // MIDDLE PANEL
                        + SSplitter::Slot()
                        .Value(0.75f)
                        .MinSize(400.f)
                        [
                            SNew(SScriptWheelCatcher)
                                .OnMouseWheel(FScriptMouseWheelDelegate::CreateLambda(
                                    [this](const FGeometry& Geo, const FPointerEvent& Event)
                                    {
                                        if (ScriptPanel.IsValid())
                                        {
                                            return ScriptPanel->HandleMouseWheel(Geo, Event);
                                        }
                                        return FReply::Unhandled();
                                    }
                                ))
                                [
                                    SAssignNew(ScriptScrollBox, SScrollBox)
                                        .ConsumeMouseWheel(EConsumeMouseWheel::Never)

                                        + SScrollBox::Slot()
                                        [
                                            SAssignNew(ScriptPanel, SGASScriptPanel)
                                        ]
                                ]


                        ]

                    // =======================================================
                    // RIGHT PANEL
                    // =======================================================
                    +SSplitter::Slot()
                        .Value(0.25f)
                        .MinSize(250.f)
                        [
                            SNew(SBorder)
                                .Padding(8.f)
                                .Visibility(EVisibility::Visible)
                                [
                                    SNew(SScrollBox)

                                        + SScrollBox::Slot()
                                        [
                                            SAssignNew(ShotListContainer, SVerticalBox)
                                        ]
                                ]
                        ]

                ]
        ];

        // ------------------------------------------------------------
        // Wire script mutation → project dirty state
        // ------------------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnScriptMutated.BindLambda([this]()
                {
                    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: Script mutated → marking dirty"));
                    MarkScriptDirty();
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

// Where we'll store/load the JSON
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




        // ------------------------------------------------------------
        // Reset derived data (shot list MUST be rebuilt)
        // ------------------------------------------------------------
        TArray<FGASShotDefinitionListRow> SceneRows;

        if (BuildShotListFromScript(
            CurrentScript,
            CurrentScript.SceneNumbering,
            SceneRows))
        {
            ShotListItems.Reset();

            for (const FGASShotDefinitionListRow& Row : SceneRows)
            {
                ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(Row));
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[ShotList] Script scene rebuild: %d scenes"),
                ShotListItems.Num()
            );

            for (const FGASShotDefinitionListRow& Row : SceneRows)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("[ShotList DEBUG] SceneRow: bIsShotRow=%d  SceneBlockIndex=%d  DisplayName='%s'"),
                    Row.bIsShotRow,
                    Row.SceneBlockIndex,
                    *Row.DisplayName
                );
            }

        }

        // -------------------------------------------------
        // Step 16: Append derived Shot rows under Scenes
        // (Display-only; importer remains authoritative)
        // -------------------------------------------------

        // Helper: resolve SceneBlockIndex from BlockId
        auto ResolveSceneBlockIndex = [&](const FString& BlockId) -> int32
            {
                for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
                {
                    if (CurrentScript.Blocks[i].Id == BlockId)
                    {
                        return i;
                    }
                }
                return INDEX_NONE;
            };

        for (const FGASMarker& Marker : CurrentScript.Markers)
        {
            if (Marker.MarkerType != EGASMarkerType::ShotMarker)
            {
                continue;
            }

            if (!Marker.bDerivedFromScene)
            {
                continue;
            }

            const int32 SceneBlockIndex = ResolveSceneBlockIndex(Marker.BlockId);
            if (SceneBlockIndex == INDEX_NONE)
            {
                continue;
            }

            FGASShotDefinitionListRow ShotRow;
            ShotRow.SceneBlockIndex = SceneBlockIndex;
            ShotRow.SceneNumber = Marker.ShotId; // display name only
            ShotRow.SceneTitle = Marker.ShotId;

            ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(ShotRow));
        }


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

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShowSceneNumbers(bShowSceneNumbers);

        ScriptPanel->RebuildLayout();
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
    if (!ScriptPanel.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShotMarker] ScriptPanel invalid"));
        return FReply::Handled();
    }

    const FString SelectedBlockId = ScriptPanel->GetSelectedBlockId();
    if (SelectedBlockId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShotMarker] No paragraph selected (no BlockId)"));
        return FReply::Handled();
    }

    FGASMarker NewMarker;
    NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    NewMarker.MarkerType = EGASMarkerType::ShotMarker;

    // Anchor to the selected paragraph
    NewMarker.BlockId = SelectedBlockId;

    // IMPORTANT: user-created (non-derived)
    NewMarker.bDerivedFromScene = false;

    CurrentScript.Markers.Add(NewMarker);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[ShotMarker] Created user ShotMarker Id=%s BlockId=%s"),
        *NewMarker.Id,
        *NewMarker.BlockId
    );

    RebuildShotList();
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
    CurrentScript = FGASScript();



    // --------------------------------------------------------------------
    // 4. Populate authoritative in-memory script (JSON-backed)
    // --------------------------------------------------------------------
    CurrentScript = Script;
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
    // SHOT MARKING MODE
    // -------------------------------------------------
    if (bIsShotMarkingActive)
    {
        if (PendingStartParagraph == INDEX_NONE)
        {
            // selecting start of shot
            PendingStartParagraph = BlockIndex;
        }
        else
        {
            // selecting end of shot → create JSON marker
            FGASMarker Marker;
            Marker.Id = FGuid::NewGuid().ToString();
            Marker.MarkerType = EGASMarkerType::ShotMarker;

            Marker.ShotId = FString::Printf(
                TEXT("SHOT_%d"),
                CurrentScript.Markers.Num() + 1
            );

            Marker.BlockId = CurrentScript.Blocks[PendingStartParagraph].Id;

            Marker.Metadata.Add(
                "EndBlockId",
                CurrentScript.Blocks[BlockIndex].Id
            );

            CurrentScript.Markers.Add(Marker);

            ScriptPanel->SetShotMarkers(CurrentScript.Markers);


            RebuildShotList();

            PendingStartParagraph = INDEX_NONE;

        }

        return; // 🚨 IMPORTANT: stop here in shot mode
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

// ------------------------------------------------------------
// UI-only shot label resolver (display only)
// ------------------------------------------------------------
static FString ResolveShotDisplayLabel(
    const FGASScript& Script,
    int32 SceneBlockIndex,
    int32 ShotIndexZeroBased
)
{
    switch (Script.ShotNumberingPolicy)
    {
    case EGASShotNumberingPolicy::Numeric_1s:
        return FString::FromInt(ShotIndexZeroBased + 1);

    case EGASShotNumberingPolicy::Numeric_10s:
        return FString::FromInt((ShotIndexZeroBased + 1) * 10);

    case EGASShotNumberingPolicy::Alphabetic:
    {
        int32 Index = ShotIndexZeroBased;
        FString Letters;

        while (Index >= 0)
        {
            Letters.InsertAt(0, TCHAR('A' + (Index % 26)));
            Index = (Index / 26) - 1;
        }
        return Letters;
    }

    default:
        return FString::FromInt(ShotIndexZeroBased + 1);
    }
}


// ============================================================================
// Build Shot List UI
// ============================================================================
void SGAS_ScriptTab::RebuildShotList()
{
    // -------------------------------------------------
    // OPTION A (Step 3): Remove previously derived shots
    // -------------------------------------------------
    {
        for (int32 i = CurrentScript.Markers.Num() - 1; i >= 0; --i)
        {
            const FGASMarker& M = CurrentScript.Markers[i];

            if (M.MarkerType == EGASMarkerType::ShotMarker && M.bDerivedFromScene)
            {
                CurrentScript.Markers.RemoveAt(i);
            }
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[ShotList] Markers=%d  ShotList=%d"),
        CurrentScript.Markers.Num(),
        ShotList.Num()
    );

    ShotListItems.Empty();

    // -------------------------------------------------
    // Helper: resolve SceneBlockIndex from Marker.BlockId
    // -------------------------------------------------
    auto FindSceneBlockIndexByBlockId =
        [this](const FString& BlockId) -> int32
        {
            for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
            {
                if (CurrentScript.Blocks[i].Id == BlockId &&
                    CurrentScript.Blocks[i].Type == EGASBlockType::SceneHeading)
                {
                    return i;
                }
            }
            return INDEX_NONE;
        };


    // -----------------------------------------
    // Scene rows from authoritative script
    // -----------------------------------------
    TArray<FGASShotDefinitionListRow> SceneRows;

    if (BuildShotListFromScript(
        CurrentScript,
        CurrentScript.SceneNumbering,
        SceneRows))
    {
        ShotListItems.Reset();

        for (const FGASShotDefinitionListRow& Row : SceneRows)
        {
            ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(Row));
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[ShotList] Script scene rebuild: %d scenes"),
            SceneRows.Num()
        );
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShotList] No JSON script for scene rebuild"));
    }

    // -------------------------------------------------
    // Helper: find SceneBlockIndex by BlockId (marker stores BlockId)
    // -------------------------------------------------
    auto FindBlockIndexById = [this](const FString& InBlockId) -> int32
        {
            if (InBlockId.IsEmpty())
            {
                return INDEX_NONE;
            }

            for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
            {
                if (CurrentScript.Blocks[i].Id == InBlockId)
                {
                    return i;
                }
            }
            return INDEX_NONE;
        };

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
    // OPTION A (Step 1 + 2): Ensure derived Shot markers
    // ONLY for scenes that already have USER shots
    // -------------------------------------------------
    {
        // 1) Collect scenes that have at least one USER shot
        TSet<FString> ScenesWithUserShots;

        for (const FGASMarker& M : CurrentScript.Markers)
        {
            if (M.MarkerType == EGASMarkerType::ShotMarker &&
                !M.bDerivedFromScene)
            {
                ScenesWithUserShots.Add(M.BlockId);
            }
        }

        // No user shots anywhere → do NOTHING
        if (ScenesWithUserShots.Num() == 0)
        {
            // intentionally empty
        }
        else
        {
            // 2) Collect scenes that already have ANY shot marker
            TSet<FString> SceneBlockIdsWithShotMarkers;

            for (const FGASMarker& M : CurrentScript.Markers)
            {
                if (M.MarkerType == EGASMarkerType::ShotMarker)
                {
                    SceneBlockIdsWithShotMarkers.Add(M.BlockId);
                }
            }

            // 3) Create derived shots ONLY for scenes with user shots
            for (const FGASShotDefinitionListRow& SceneRow : SceneRows)
            {
                if (!CurrentScript.Blocks.IsValidIndex(SceneRow.SceneBlockIndex))
                {
                    continue;
                }

                const FString& SceneBlockId =
                    CurrentScript.Blocks[SceneRow.SceneBlockIndex].Id;

                // 🔒 Only scenes that already contain USER shots
                if (!ScenesWithUserShots.Contains(SceneBlockId))
                {
                    continue;
                }

                if (SceneBlockIdsWithShotMarkers.Contains(SceneBlockId))
                {
                    continue;
                }

                FGASMarker NewMarker;
                NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
                NewMarker.MarkerType = EGASMarkerType::ShotMarker;
                NewMarker.BlockId = SceneBlockId;
                NewMarker.bDerivedFromScene = true;

                CurrentScript.Markers.Add(NewMarker);
                SceneBlockIdsWithShotMarkers.Add(SceneBlockId);
            }
        }
    }



    // -------------------------------------------------
    // Stable ordering of derived shot markers
    // -------------------------------------------------
    TArray<const FGASMarker*> DerivedShotMarkers;

    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType == EGASMarkerType::ShotMarker &&
            Marker.bDerivedFromScene)
        {
            DerivedShotMarkers.Add(&Marker);
        }
    }

    DerivedShotMarkers.Sort(
        [&](const FGASMarker& A, const FGASMarker& B)
        {
            const int32 SceneA = FindSceneBlockIndexByBlockId(A.BlockId);
            const int32 SceneB = FindSceneBlockIndexByBlockId(B.BlockId);

            if (SceneA != SceneB)
            {
                return SceneA < SceneB;
            }

            // Stable fallback
            return A.Id < B.Id;
        }
    );

    // -------------------------------------------------
    // OPTION A (Step 4): Append Shot rows (derived shots)
    // -------------------------------------------------
    for (const FGASMarker* MarkerPtr : DerivedShotMarkers)
    {
        const FGASMarker& Marker = *MarkerPtr;

        FGASShotDefinitionListRow ShotRow;
        ShotRow.bIsShotRow = true;

        // Resolve SceneBlockIndex from Marker.BlockId
        const int32 SceneBlockIndex = FindSceneBlockIndexByBlockId(Marker.BlockId);
        if (SceneBlockIndex == INDEX_NONE)
        {
            continue;
        }

        ShotRow.SceneBlockIndex = SceneBlockIndex;
        ShotRow.MarkerId = Marker.Id;

        // -------------------------------------------------
        // STEP 16: UI-only shot display naming
        // -------------------------------------------------
        TMap<int32, int32> SceneShotCounters;

        int32& Counter = SceneShotCounters.FindOrAdd(SceneBlockIndex);
        const int32 ShotIndexZeroBased = Counter++;

        ShotRow.DisplayName =
            ResolveShotDisplayLabel(
                CurrentScript,
                SceneBlockIndex,
                ShotIndexZeroBased
            );

        ShotRow.bDerivedFromScene = Marker.bDerivedFromScene;

        ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(ShotRow));
    }


    // -------------------------------------------------
    // STEP MVP: Append user-created shot rows (non-derived)
    // -------------------------------------------------
    TMap<int32, int32> SceneUserShotCounters;


    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        if (Marker.bDerivedFromScene)
        {
            continue; // derived already handled above
        }

        const int32 SceneBlockIndex =
            FindOwningSceneBlockIndex(Marker.BlockId);


        if (SceneBlockIndex == INDEX_NONE)
        {
            continue;
        }

        FGASShotDefinitionListRow ShotRow;
        ShotRow.bIsShotRow = true;
        ShotRow.bDerivedFromScene = false;

        ShotRow.SceneBlockIndex = SceneBlockIndex;
        ShotRow.MarkerId = Marker.Id;

        int32& Counter = SceneUserShotCounters.FindOrAdd(SceneBlockIndex);
        Counter++;

        // UI-only label for MVP
        ShotRow.DisplayName =
            FString::Printf(TEXT("U%d"), Counter);

        ShotListItems.Add(MakeShared<FGASShotDefinitionListRow>(ShotRow));
    }


    // -------------------------------------------------
    // Reorder ShotListItems so shots follow their parent scene deterministically
    // -------------------------------------------------
    {
        TArray<TSharedPtr<FGASShotDefinitionListRow>> OrderedItems;

        // Cache shot rows grouped by SceneBlockIndex
        TMultiMap<int32, TSharedPtr<FGASShotDefinitionListRow>> ShotsByScene;

        for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
        {
            if (Row.IsValid() && Row->bIsShotRow)
            {
                ShotsByScene.Add(Row->SceneBlockIndex, Row);
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

            // Then its shots
            TArray<TSharedPtr<FGASShotDefinitionListRow>> SceneShots;
            ShotsByScene.MultiFind(Row->SceneBlockIndex, SceneShots);

            for (const TSharedPtr<FGASShotDefinitionListRow>& ShotRow : SceneShots)
            {
                OrderedItems.Add(ShotRow);
            }
        }

        ShotListItems = MoveTemp(OrderedItems);
    }


    // -------------------------------------------------
    // Renumber shots per scene (S##_##)
    // -------------------------------------------------
    {
        TMap<int32, int32> SceneShotCounters;

        for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
        {
            if (!Row.IsValid() || !Row->bIsShotRow)
            {
                continue;
            }

            int32& Counter = SceneShotCounters.FindOrAdd(Row->SceneBlockIndex);
            Counter++;

            const int32 ShotNumber = Counter;

            // Format: S<SceneNumber>_<ShotNumber padded>
            FString SceneNum = Row->SceneBlockIndex != INDEX_NONE
                ? Row->SceneNumber
                : TEXT("X");

            if (SceneNum.IsEmpty())
            {
                SceneNum = TEXT("X");
            }

            //Row->DisplayName =
            //    FString::Printf(TEXT("S%s_%02d"), *SceneNum, ShotNumber);

            //// Also update backing marker name
            //for (FGASMarker& M : CurrentScript.Markers)
            //{
            //    if (M.Id == Row->MarkerId)
            //    {
            //        M.ShotId = Row->DisplayName;
            //        break;
            //    }
            //}
        }
    }
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

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[ShotList UI] Children AFTER clear = %d"),
        ShotListContainer->GetChildren()->Num()
    );


    if (SceneRows.Num() == 0)
    {
        ShotListContainer->AddSlot()
            .AutoHeight()
            .Padding(8.f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString("No scenes found."))
                    .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
            ];
        return;
    }

    for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
    {
        if (!Row.IsValid())
        {
            continue;
        }

        // HARD GUARD
        if (Row->bIsShotRow && Row->DisplayName.IsEmpty())
        {
            continue;
        }

        // -----------------------------
        // Scene row
        // -----------------------------
        if (!Row->bIsShotRow)
        {
            const FString SceneLabel = FString::Printf(
                TEXT("%s  %s"),
                *Row->SceneNumber,
                *Row->SceneTitle
            );

            ShotListContainer->AddSlot()
                .AutoHeight()
                .Padding(4.f, 6.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(SceneLabel))
                        .OnClicked_Lambda([this, Row]()
                            {
                                ScrollToScene(*Row);
                                return FReply::Handled();
                            })
                ];
        }
        // -----------------------------
        // Shot row
        // -----------------------------
        else
        {
            ShotListContainer->AddSlot()
                .AutoHeight()
                .Padding(24.f, 2.f)
                [
                    SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "NoBorder")
                        .OnClicked_Lambda([this, Row]()
                            {
                                FGASMarker* FoundMarker = nullptr;

                                for (FGASMarker& M : CurrentScript.Markers)
                                {
                                    if (M.Id == Row->MarkerId)
                                    {
                                        FoundMarker = &M;
                                        break;
                                    }
                                }

                                if (!FoundMarker)
                                {
                                    return FReply::Handled();
                                }

                                // -------------------------------------------------
                                // USER SHOT → scroll to paragraph anchor
                                // -------------------------------------------------
                                if (!FoundMarker->bDerivedFromScene)
                                {
                                    if (ScriptPanel.IsValid())
                                    {
                                        ScriptPanel->ScrollToBlockId(FoundMarker->BlockId);
                                    }
                                    return FReply::Handled();
                                }

                                // -------------------------------------------------
                                // DERIVED SHOT → semantic promotion + scroll to scene
                                // -------------------------------------------------
                                FoundMarker->bDerivedFromScene = false;

                                RebuildShotList();
                                ScrollToScene(*Row);

                                return FReply::Handled();
                            })


                        [
                            SNew(STextBlock)
                                .Text(
                                    Row->DisplayName.IsEmpty()
                                    ? FText::GetEmpty()
                                    : FText::FromString(Row->DisplayName)
                                )
                                .ColorAndOpacity(
                                    Row->bDerivedFromScene
                                    ? FLinearColor(0.65f, 0.65f, 0.65f)
                                    : FLinearColor::White
                                )
                        ]

                ];
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

    // ✅ THIS IS THE JUMP
    ScriptPanel->ScrollToBlockId(BlockId);

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
}


FString SGAS_ScriptTab::GetAuthoritativeScriptJsonPath() const
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GAS"));
    IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
    return FPaths::Combine(Dir, TEXT("CurrentScript.gasjson"));
}

