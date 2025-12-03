#include "SGAS_ScriptTab.h"
#include "SGASScriptPanel.h"

#include "GAS_FDXImporter.h"
#include "ScriptLayoutEngine.h"

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
            SNew(SSplitter)

                // =======================================================
                // LEFT PANEL — Buttons
                // =======================================================
                +SSplitter::Slot()
                .Value(0.05f)
                [
                    SNew(SBorder)
                        .Padding(4)
                        [
                            SNew(SVerticalBox)

                                // --- Shot Marking Button --------------------------------
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 0, 0, 8)
                                [
                                    SNew(SButton)
                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleShotMarking))
                                        .ToolTipText(FText::FromString(TEXT("Begin/End Shot Marking")))
                                        [
                                            SNew(SBox)
                                                .WidthOverride(30.f)
                                                .HeightOverride(30.f)
                                                [
                                                    SAssignNew(ShotMarkingIcon, SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.CameraIcon"))
                                                ]
                                        ]
                                ]

                            // --- Import Script (.fdx) --------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SButton)
                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnImportScript))
                                        .ToolTipText(FText::FromString(TEXT("Import Final Draft Script")))
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("Import Script")))
                                        ]
                                ]
                        ]
                ]

            // =======================================================
            // MIDDLE PANEL — Script View
            // =======================================================
            +SSplitter::Slot()
                .Value(0.55f)
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
            // RIGHT PANEL — Shot List
            // =======================================================
            +SSplitter::Slot()
                .Value(0.35f)
                [
                    SNew(SBorder)
                        .Padding(8.f)
                        [
                            SAssignNew(ShotListContainer, SVerticalBox)
                        ]
                ]
        ];

    RebuildShotList();

    // Try to auto-load last saved script (if any)
    LoadScriptFromJsonIfExists();
}

// Simple helpers to convert paragraph type to/from string for JSON

static FString ParagraphTypeToString(EParagraphType Type)
{
    switch (Type)
    {
    case EParagraphType::SceneHeading:   return TEXT("SceneHeading");
    case EParagraphType::Action:         return TEXT("Action");
    case EParagraphType::Character:      return TEXT("Character");
    case EParagraphType::Dialogue:       return TEXT("Dialogue");
    case EParagraphType::Parenthetical:  return TEXT("Parenthetical");
    case EParagraphType::Transition:     return TEXT("Transition");
    default:                             return TEXT("Unknown");
    }
}

static EParagraphType ParagraphTypeFromString(const FString& Str)
{
    if (Str == TEXT("SceneHeading"))   return EParagraphType::SceneHeading;
    if (Str == TEXT("Action"))         return EParagraphType::Action;
    if (Str == TEXT("Character"))      return EParagraphType::Character;
    if (Str == TEXT("Dialogue"))       return EParagraphType::Dialogue;
    if (Str == TEXT("Parenthetical"))  return EParagraphType::Parenthetical;
    if (Str == TEXT("Transition"))     return EParagraphType::Transition;
    return EParagraphType::Unknown;
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

    const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

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
        LoadScriptFromFDX(OutFiles[0]);
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
    // -------------------------
    // Import paragraphs
    // -------------------------
    ParsedParagraphs.Empty();
    GAS_FDXImporter::ImportFDX(FilePath, ParsedParagraphs);

    if (ParsedParagraphs.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("FDX import failed or empty"));
        return;
    }

    // -------------------------
    // Layout into Final Draft formatting
    // -------------------------
    float PanelWidth = 1100.f; // safe default
    if (ScriptPanel.IsValid())
    {
        PanelWidth = ScriptPanel->GetCachedGeometry().GetLocalSize().X;
        if (PanelWidth < 200.f)
            PanelWidth = 1100.f; // fallback
    }

    TArray<FRenderedParagraph> Rendered =
        ScriptLayoutEngine::LayoutScript(ParsedParagraphs, PanelWidth);

    // -------------------------
    // Send to panel
    // -------------------------
    ScriptPanel->SetRenderedScript(Rendered);

    // Shots cleared, since script changed
    ShotList.Empty();
    ShotStartParagraphs.Empty();
    ShotEndParagraphs.Empty();
    ScriptPanel->SetShotMarkers(ShotStartParagraphs, ShotEndParagraphs);

    RebuildShotList();

    // Persist script + (currently empty) shots
    SaveScriptToJson();
}



// ============================================================================
// PARAGRAPH CLICKED — shot marking logic
// ============================================================================
void SGAS_ScriptTab::OnScriptParagraphClicked(int32 ParagraphIndex)
{
    if (!bIsMarkingShot) return;

    // If no start yet, set it
    if (PendingStartParagraph == INDEX_NONE)
    {
        PendingStartParagraph = ParagraphIndex;
        return;
    }

    // End paragraph clicked → finalize shot
    int32 Start = PendingStartParagraph;
    int32 End = ParagraphIndex;

    if (End < Start)
        Swap(Start, End);

    FShotEntry NewShot;
    NewShot.StartParagraph = Start;
    NewShot.EndParagraph = End;
    NewShot.ShotType = TEXT("Unknown");

    ShotList.Add(NewShot);

    ShotStartParagraphs.Add(Start);
    ShotEndParagraphs.Add(End);

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShotMarkers(ShotStartParagraphs, ShotEndParagraphs);
    }

    PendingStartParagraph = INDEX_NONE;

    RebuildShotList();

    // Save updated shots
    SaveScriptToJson();
}



// ============================================================================
// Build Shot List UI
// ============================================================================
void SGAS_ScriptTab::RebuildShotList()
{
    if (!ShotListContainer.IsValid())
        return;

    ShotListContainer->ClearChildren();

    for (int32 i = 0; i < ShotList.Num(); i++)
    {
        const FShotEntry& S = ShotList[i];

        ShotListContainer->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(
                        FString::Printf(TEXT("Shot %03d   (P %d–%d)"),
                            i + 1,
                            S.StartParagraph + 1,
                            S.EndParagraph + 1)))
            ];
    }
}


void SGAS_ScriptTab::SaveScriptToJson()
{
    // TEMP: JSON saving disabled so we can get the plugin compiling cleanly.
    // Later we’ll re-enable this with a UE 5.6–compatible Json setup.
    UE_LOG(LogTemp, Verbose, TEXT("SaveScriptToJson() stubbed – not saving script/shots this build."));
}


void SGAS_ScriptTab::LoadScriptFromJsonIfExists()
{
    // TEMP: JSON loading disabled so we start from a fresh state each run.
    // Later this can reload the last script and shots from disk.
    UE_LOG(LogTemp, Verbose, TEXT("LoadScriptFromJsonIfExists() stubbed – not loading script/shots this build."));
}

