#include "SGAS_ScriptTab.h"
#include "ScriptModel.h"
#include "SGASScriptPanel.h"
#include "GAS_FDXImporter.h"
#include "GASPreProProject.h"
#include "GAS_PreProToolsEditorModule.h"

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
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "GAS_PreProToolsStyle.h"

// JSON persistence is temporarily disabled; we keep FileManager for GetScriptJsonPath.
#include "HAL/FileManager.h"




void SGAS_ScriptTab::Construct(const FArguments& InArgs)
{
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
                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleShotMarking))
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.CameraIcon"))
                                                ]
                                        ]
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
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SaveIcon"))
                                                ]
                                        ]
                                ]


                            // --- Scene Number Toggle --------------------------------
                            +SVerticalBox::Slot()
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
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString("Import"))
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
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString("Clear"))
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
                        [
                            SNew(SBorder)
                                .Padding(8.f)
                                [
                                    SNew(SScrollBox)
                                        + SScrollBox::Slot()
                                        [
                                            SAssignNew(ScriptPanel, SGASScriptPanel)
                                                .OnParagraphClicked(FOnParagraphClicked::CreateSP(
                                                    this, &SGAS_ScriptTab::OnScriptParagraphClicked))

                                        ]
                                ]
                        ]


                    // =======================================================
                    // RIGHT PANEL
                    // =======================================================
                    +SSplitter::Slot()
                        .Value(0.25f)
                        [
                            SNew(SBorder)
                                .Padding(8.f)
                                [
                                    SAssignNew(ShotListContainer, SVerticalBox)
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



        

    RebuildShotList();
}




// Simple helpers to convert paragraph type to/from string for JSON

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
    if (!DesktopPlatform) return FReply::Handled();

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

    // ------------------------------------------------------------
    // Create a new project and hand ownership to the window
    // ------------------------------------------------------------
    UGASPreProProject* NewProject = NewObject<UGASPreProProject>();

    // Transfer script blocks into the project (authoritative copy)
    NewProject->ScriptBlocks = LoadedScript.Blocks;

    // Freshly imported project is clean
    NewProject->ClearDirty();

    // Notify owner (SGAS_TestWindow)
    if (OnProjectLoaded.IsBound())
    {
        OnProjectLoaded.Execute(NewProject);
    }

    if (bOpened && OutFiles.Num() > 0)
    {
        LoadScriptFromFDX(OutFiles[0]);
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



// ============================================================================
// LOAD SCRIPT (.FDX) — parse, layout, send to panel
// ============================================================================
void SGAS_ScriptTab::LoadScriptFromFDX(const FString& FilePath)
{
    // --------------------------------------------------------------------
    // 0. Confirm overwrite if script already loaded
    // --------------------------------------------------------------------
    if (bHasLoadedScript)
    {
        const FText Title = FText::FromString("Replace script?");
        const FText Message = FText::FromString(
            "Are you sure you want to load a new script?\n"
            "This will overwrite the current script, page breaks, and markers."
        );

        EAppReturnType::Type Response =
            FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title);

        if (Response == EAppReturnType::No)
            return;
    }

    // --------------------------------------------------------------------
    // 1. Clear previous script state COMPLETELY
    // --------------------------------------------------------------------
    LoadedScript = FGASScript();                 // wipe JSON model
    ScriptPanel->SetScript(&LoadedScript);       // clear panel reference
    ScriptPanel->SetRenderedScript({});          // wipe rendering
    ScriptPanel->SetShotMarkers({});             // wipe markers
    RebuildShotList();                           // clear shot list UI

    // --------------------------------------------------------------------
    // 2. Import FDX into a temporary FGASScript
    // --------------------------------------------------------------------
    FGASScript Script;
    if (!UGAS_FDXImporter::ImportFDXToScript(FilePath, Script))
    {
        UE_LOG(LogTemp, Error, TEXT("FDX import failed: %s"), *FilePath);
        return;
    }

    // Store imported script as current JSON model
    LoadedScript = Script;
    LoadedScript.UserPageBreakParagraphs.Empty();
    ScriptPanel->SetScript(&LoadedScript);
    


    // --------------------------------------------------------------------
    // 3. Determine panel width for layout
    // --------------------------------------------------------------------
    float PanelWidth = 1100.f;
    if (ScriptPanel.IsValid())
    {
        PanelWidth = ScriptPanel->GetCachedGeometry().GetLocalSize().X;
        if (PanelWidth < 200.f)
            PanelWidth = 1100.f;
    }

    // --------------------------------------------------------------------
    // 4. Layout script using NEW engine
    //     NOTE: On first load, UserPageBreakParagraphs is EMPTY
    //     so auto-page-breaks will be applied normally.
    // --------------------------------------------------------------------
    TArray<FRenderedParagraph> Rendered =
        ScriptLayoutEngine::LayoutScript(
            LoadedScript.Blocks,
            PanelWidth,
            LoadedScript.UserPageBreakParagraphs,
            LoadedScript.SuppressedAutoBreakBlockIds
        );


    // Send paragraphs to UI
    ScriptPanel->SetRenderedScript(Rendered);

    // --------------------------------------------------------------------
    // 5. Clear markers on load (FDX has none)
    // --------------------------------------------------------------------
    LoadedScript.Markers.Empty();
    ScriptPanel->SetShotMarkers(LoadedScript.Markers);

    // Rebuild right-side UI
    RebuildShotList();

    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: LoadScriptFromFDX finished, marking dirty"));

    MarkScriptDirty();


    // --------------------------------------------------------------------
    // 6. Save JSON for persistence
    // --------------------------------------------------------------------
    //SaveScriptToJson();

    // --------------------------------------------------------------------
    // 7. Mark script as loaded
    // --------------------------------------------------------------------
    bHasLoadedScript = true;

    UE_LOG(LogTemp, Log, TEXT("Successfully loaded script from FDX: %s"), *FilePath);


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
    LoadedScript = FGASScript();

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&LoadedScript);
        ScriptPanel->SetRenderedScript({});
        ScriptPanel->SetShotMarkers({});
    }

    // Clear any manual page breaks
    LoadedScript.UserPageBreakParagraphs.Empty();

    RebuildShotList();
    bHasLoadedScript = false;

    UE_LOG(LogTemp, Warning, TEXT("SCRIPT CLEARED"));
}

FReply SGAS_ScriptTab::OnToggleEditMode()
{
    bIsEditMode = !bIsEditMode;

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetEditMode(bIsEditMode);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("SCRIPT EDIT MODE: %s"),
        bIsEditMode ? TEXT("ON") : TEXT("OFF"));

    return FReply::Handled();
}


// ============================================================================
// PARAGRAPH CLICKED — shot marking logic
// ============================================================================
void SGAS_ScriptTab::OnScriptParagraphClicked(int32 ParagraphIndex)
{// Make sure index is valid
    if (!LoadedScript.Blocks.IsValidIndex(ParagraphIndex))
        return;

    const FGASBlock& Block = LoadedScript.Blocks[ParagraphIndex];

    // Shot marking logic
    if (bIsMarkingShot)
    {
        if (PendingStartParagraph == INDEX_NONE)
        {
            // selecting start of shot
            PendingStartParagraph = ParagraphIndex;
        }
        else
        {
            // selecting end of shot → create JSON marker
            FGASMarker Marker;
            Marker.Id = FGuid::NewGuid().ToString();
            Marker.MarkerType = EGASMarkerType::ShotMarker;

            // temporary shot ID until UI supports naming
            Marker.ShotId = FString::Printf(TEXT("SHOT_%d"), LoadedScript.Markers.Num() + 1);

            // Map start block → BlockId
            Marker.BlockId = LoadedScript.Blocks[PendingStartParagraph].Id;

            // Map end block → store as metadata
            Marker.Metadata.Add("EndBlockId", LoadedScript.Blocks[ParagraphIndex].Id);

            LoadedScript.Markers.Add(Marker);

            PendingStartParagraph = INDEX_NONE;

            //SaveScriptToJson();

            // Update panel UI
            ScriptPanel->SetShotMarkers(LoadedScript.Markers);
        }
    }
}


FReply SGAS_ScriptTab::OnClearScriptClicked()
{
    ClearScript();
    return FReply::Handled();
}


// ============================================================================
// Build Shot List UI
// ============================================================================
void SGAS_ScriptTab::RebuildShotList()
{
    ShotList.Empty();

    for (const FGASMarker& Marker : LoadedScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
            continue;

        FShotEntry Entry;
        Entry.ShotType = Marker.ShotId;

        // Convert BlockId → paragraph index
        int32 StartIndex = LoadedScript.Blocks.IndexOfByPredicate(
            [&](const FGASBlock& B) { return B.Id == Marker.BlockId; });

        int32 EndIndex = StartIndex;
        if (Marker.Metadata.Contains("EndBlockId"))
        {
            FString EndId = Marker.Metadata["EndBlockId"];
            EndIndex = LoadedScript.Blocks.IndexOfByPredicate(
                [&](const FGASBlock& B) { return B.Id == EndId; });
        }

        Entry.StartParagraph = StartIndex;
        Entry.EndParagraph = EndIndex;

        ShotList.Add(Entry);
    }
}




void SGAS_ScriptTab::SaveScriptToJson()
{
    UE_LOG(LogTemp, Warning, TEXT("SCRIPT TAB: SaveScriptToJson CALLED"));

    FString JsonPath = FPaths::ProjectSavedDir() / TEXT("Scripts/CurrentScript.gasjson");

    // Build JSON from FGASScript
    FString OutputString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(LoadedScript, OutputString))
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




void SGAS_ScriptTab::LoadScriptFromJsonIfExists()
{
    FString JsonPath = FPaths::ProjectSavedDir() / TEXT("Scripts/CurrentScript.gasjson");

    if (!FPaths::FileExists(JsonPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("No existing script JSON found."));
        return;
    }

    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file: %s"), *JsonPath);
        return;
    }

    FGASScript Loaded;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonText, &Loaded, 0, 0))
    {
        UE_LOG(LogTemp, Error, TEXT("JSON parse failed"));
        return;
    }

    // Store into tab
    LoadedScript = Loaded;
    ScriptPanel->SetScript(&LoadedScript);

    // -------------------------
    // Determine panel width for layout
    // -------------------------
    float PanelWidth = 1100.f;

    if (ScriptPanel.IsValid())
    {
        PanelWidth = ScriptPanel->GetCachedGeometry().GetLocalSize().X;

        // Fallback if panel not initialized yet
        if (PanelWidth < 200.f)
            PanelWidth = 1100.f;
    }


    // Convert Blocks to Rendered Paragraphs
    TArray<FRenderedParagraph> Rendered =
        ScriptLayoutEngine::LayoutScript(
            LoadedScript.Blocks,
            PanelWidth,
            LoadedScript.UserPageBreakParagraphs,
            LoadedScript.SuppressedAutoBreakBlockIds
        );



    ScriptPanel->SetRenderedScript(Rendered);



    // Restore markers
    ScriptPanel->SetShotMarkers(Loaded.Markers);

    // Rebuild UI side panel
    RebuildShotList();

    UE_LOG(LogTemp, Log, TEXT("Loaded script JSON successfully."));
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

