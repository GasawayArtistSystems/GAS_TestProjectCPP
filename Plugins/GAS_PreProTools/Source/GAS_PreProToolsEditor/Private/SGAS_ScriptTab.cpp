#include "SGAS_ScriptTab.h"
#include "SGASScriptPanel.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#include "GAS_PreProToolsStyle.h"


void SGAS_ScriptTab::Construct(const FArguments& InArgs)
{
    // -------------------------------------------------------------
    // Dummy script lines for now — will be replaced by importer
    // -------------------------------------------------------------
    ScriptLines.Empty();
    ScriptLines.Add(TEXT("INT. LIVING ROOM - NIGHT"));
    ScriptLines.Add(TEXT("Raya stands at the window, listening."));
    ScriptLines.Add(TEXT("AARON"));
    ScriptLines.Add(TEXT("    You really think this is going to work?"));
    ScriptLines.Add(TEXT("    She doesn’t answer. The house creaks."));
    ScriptLines.Add(TEXT("SASHA (O.S.)"));
    ScriptLines.Add(TEXT("    Raya? You okay in there?"));

    ShotList.Empty();
    SceneNums.Empty();
    DialogueNums.Empty();

    // -------------------------------------------------------------
    // Build initial numbering arrays
    // -------------------------------------------------------------
    ComputeLineNumbers(SceneNums, DialogueNums);

    // -------------------------------------------------------------
    // Build UI Layout
    // -------------------------------------------------------------
    ChildSlot
        [
            SNew(SSplitter)

                // =======================================================
                // LEFT PANEL (button strip: shot marking + numbering)
                // =======================================================
                +SSplitter::Slot()
                .Value(0.05f)
                [
                    SNew(SBorder)
                        .Padding(4)
                        [
                            SNew(SVerticalBox)

                                // --- Button: Shot Marking -----------------------
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 0, 0, 8)
                                [
                                    SNew(SButton)
                                        .OnClicked(this, &SGAS_ScriptTab::OnToggleShotMarking)
                                        .ToolTipText(FText::FromString(TEXT("Begin/End Shot Marking")))
                                        .ContentPadding(FMargin(0))
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

                            // --- Button: Toggle Scene/Dialogue Numbers ------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SButton)
                                        .OnClicked(this, &SGAS_ScriptTab::OnToggleNumbering)
                                        .ToolTipText(FText::FromString(TEXT("Toggle Scene & Dialogue Numbers")))
                                        .ContentPadding(FMargin(0))
                                        [
                                            SNew(SBox)
                                                .WidthOverride(30.f)
                                                .HeightOverride(30.f)
                                                [
                                                    SAssignNew(NumberingIcon, SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.NumberIcon"))

                                                ]
                                        ]

                                ]
                        ]
                ]

            // =======================================================
            // MIDDLE PANEL (script + lines + overlays)
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
                                        .ScriptLines(ScriptLines)
                                        .ShotStartLines(TArray<int32>())
                                        .ShotEndLines(TArray<int32>())
                                        .ShotXs(TArray<float>())
                                        .SceneNumbers(SceneNums)
                                        .DialogueNumbers(DialogueNums)
                                ]

                        ]
                ]

            // =======================================================
            // RIGHT PANEL (shot list)
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
    // 🔹 FIX: bind click event
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->OnLineClicked.BindSP(this, &SGAS_ScriptTab::OnScriptLineClicked);
    }
    RebuildShotList();
}



//
// -------------------------------------------------------------
// User clicks on a line in the script panel
// -------------------------------------------------------------
void SGAS_ScriptTab::OnScriptLineClicked(int32 LineIndex, float ClickX)
{
    if (!bIsMarkingShot)
        return;

    // FIRST CLICK → Start
    if (PendingStartLineIndex == INDEX_NONE)
    {
        PendingStartLineIndex = LineIndex;
        PendingStartX = ClickX;
        return;
    }

    // SECOND CLICK → End
    const int32 Start = FMath::Min(PendingStartLineIndex, LineIndex);
    const int32 End = FMath::Max(PendingStartLineIndex, LineIndex);

    FShotEntry NewShot;
    NewShot.StartLineIndex = Start;
    NewShot.EndLineIndex = End;
    NewShot.ShotX = PendingStartX;

    ShotList.Add(NewShot);

    // reset marking
    PendingStartLineIndex = INDEX_NONE;
    PendingStartX = 0.f;

    RebuildShotList();
    RebuildScriptList();
}



//
// -------------------------------------------------------------
// Toggle Shot-Marking Mode
// -------------------------------------------------------------
FReply SGAS_ScriptTab::OnToggleShotMarking()
{
    bIsMarkingShot = !bIsMarkingShot;

    if (ShotModeButtonLabel.IsValid())
    {
        ShotModeButtonLabel->SetText(
            bIsMarkingShot ?
            FText::FromString(TEXT("Cancel Shot Marking")) :
            FText::FromString(TEXT("Begin Shot Marking"))
        );
    }

    if (ShotMarkingIcon.IsValid())
    {
        FLinearColor ActiveColor(0.1f, 0.5f, 1.0f, 1.0f);  // blue
        FLinearColor NormalColor(1.f, 1.f, 1.f, 1.f);      // white

        ShotMarkingIcon->SetColorAndOpacity(
            bIsMarkingShot ? ActiveColor : NormalColor
        );
    }

    return FReply::Handled();
}



//
// -------------------------------------------------------------
// Toggle numbering
// -------------------------------------------------------------
FReply SGAS_ScriptTab::OnToggleNumbering()
{
    bShowNumbering = !bShowNumbering;

    if (NumberingIcon.IsValid())
    {
        FLinearColor ActiveColor(0.1f, 0.5f, 1.0f, 1.0f);  // blue
        FLinearColor NormalColor(1.f, 1.f, 1.f, 1.f);

        NumberingIcon->SetColorAndOpacity(
            bShowNumbering ? ActiveColor : NormalColor
        );
    }

    if (bShowNumbering)
        ComputeLineNumbers(SceneNums, DialogueNums);

    RebuildScriptList();
    return FReply::Handled();
}




//
// -------------------------------------------------------------
// Compute scene & dialogue numbering
// -------------------------------------------------------------
void SGAS_ScriptTab::ComputeLineNumbers(
    TArray<FString>& OutScenes,
    TArray<FString>& OutDialogues)
{
    OutScenes.SetNum(ScriptLines.Num());
    OutDialogues.SetNum(ScriptLines.Num());

    int32 SceneCounter = 10;
    int32 DialogueCounter = 1;

    for (int32 i = 0; i < ScriptLines.Num(); i++)
    {
        FString Line = ScriptLines[i];

        // --- Scene Headings ---
        if (Line.StartsWith(TEXT("INT.")) ||
            Line.StartsWith(TEXT("EXT.")))
        {
            OutScenes[i] = FString::Printf(TEXT("%03d"), SceneCounter);
            SceneCounter += 10;
            DialogueCounter = 1; // reset on new scene
            continue;
        }

        // --- Dialogue character name (detect uppercase names but NOT INT./EXT.) ---
        bool bIsAllCaps = (Line.ToUpper() == Line);
        bool bHasLetters = Line.Contains(TEXT("A")) || Line.Contains(TEXT("E")) || Line.Contains(TEXT("I")) ||
            Line.Contains(TEXT("O")) || Line.Contains(TEXT("U"));  // crude but safe
        bool bIsSceneHeading = Line.StartsWith(TEXT("INT.")) || Line.StartsWith(TEXT("EXT."));

        bool bIsCharacterName =
            bIsAllCaps &&
            bHasLetters &&
            !bIsSceneHeading &&
            !Line.Contains(TEXT("."));   // exclude "SASHA (O.S.)"

        if (bIsCharacterName)
        {
            OutDialogues[i] = FString::Printf(TEXT("%d"), DialogueCounter);
            DialogueCounter++;
        }

    }
}



//
// -------------------------------------------------------------
// Update the script panel (shot lines + numbering)
// -------------------------------------------------------------
void SGAS_ScriptTab::RebuildScriptList()
{
    if (!ScriptPanel.IsValid())
        return;

    // Prepare arrays to feed the script panel
    TArray<int32> StartLines;
    TArray<int32> EndLines;
    TArray<float> Xs;

    for (const FShotEntry& S : ShotList)
    {
        StartLines.Add(S.StartLineIndex);
        EndLines.Add(S.EndLineIndex);
        Xs.Add(S.ShotX);
    }

    // -----------------------------------------
    // NEW: Build tooltip strings
    // -----------------------------------------
    TArray<FString> TooltipList;

    for (int32 i = 0; i < ShotList.Num(); i++)
    {
        FString ShotNum = FString::Printf(TEXT("Shot %03d"), i + 1);
        FString ShotType = ShotList[i].ShotType;     // defaults to "Unknown"

        TooltipList.Add(FString::Printf(TEXT("%s — %s"), *ShotNum, *ShotType));
    }

    ScriptPanel->SetShotTooltips(TooltipList);

    // -----------------------------------------
    // Send everything to the script panel
    // -----------------------------------------
    ScriptPanel->SetShots(StartLines, EndLines, Xs);
    ScriptPanel->SetSceneNumbers(SceneNums, DialogueNums);
}




//
// -------------------------------------------------------------
// Build the right-hand shot list (simple, placeholder)
// -------------------------------------------------------------
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
                        FString::Printf(TEXT("Shot %03d   (Lines %d–%d)"),
                            i + 1,
                            S.StartLineIndex + 1,
                            S.EndLineIndex + 1)))
            ];
    }
}
