// NOTE:
// Some shot-related UI logic currently assumes implicit structure.
// This is intentional for now and will be cleaned up when the Shot List
// feature formalizes rendered shot data from markers.



#include "SGASScriptPanel.h"
#include "SGAS_ScriptTab.h"
#include "ScriptFormatRules.h"
#include "SlateOptMacros.h"
#include "GAS_PreProToolsStyle.h"
#include "Modules/ModuleManager.h"
#include "GAS_PreProToolsEditorModule.h"
#include "GAS_SceneNumberResolver.h"
#include "GAS_ImportNumberingTypes.h"
#include "ScriptModel.h"
#include "GAS_ShotListSelection.h"
#include "GAS_ShotListRules.h"

#include "ScriptLayoutEngine.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Containers/Ticker.h"

// =============================================================
//  Constants
// =============================================================

static constexpr float ShotPillWidth = 48.f;
static constexpr float ShotPillHeight = 18.f;

// Shot drag (ALT + LMB)
bool bIsDraggingShot = false;
FString DraggingShotMarkerId;
FVector2D ShotDragOffset;
float ShotDragGhostX = -1.f;
float ShotDragGhostY = -1.f;

// Shot tail resize (ALT + LMB on tail)
bool bIsResizingShotTail = false;
FString ResizingShotMarkerId;
float ShotTailGhostY = -1.f;
static float ShotTailResizeStartY = -1.f;

static TArray<TSharedPtr<EGASShotType>> ShotTypeOptions = {
    MakeShared<EGASShotType>(EGASShotType::ECU),
    MakeShared<EGASShotType>(EGASShotType::CU),
    MakeShared<EGASShotType>(EGASShotType::MCU),
    MakeShared<EGASShotType>(EGASShotType::MS),
    MakeShared<EGASShotType>(EGASShotType::MLS),
    MakeShared<EGASShotType>(EGASShotType::WS),
    MakeShared<EGASShotType>(EGASShotType::EWS)
};

static TArray<TSharedPtr<EGASFramingBias>> FramingOptions = {
    MakeShared<EGASFramingBias>(EGASFramingBias::Center),
    MakeShared<EGASFramingBias>(EGASFramingBias::Left),
    MakeShared<EGASFramingBias>(EGASFramingBias::Right),
    MakeShared<EGASFramingBias>(EGASFramingBias::High),
    MakeShared<EGASFramingBias>(EGASFramingBias::Low)
};

static TArray<TSharedPtr<EGASLensIntent>> LensOptions = {
    MakeShared<EGASLensIntent>(EGASLensIntent::L24),
    MakeShared<EGASLensIntent>(EGASLensIntent::L35),
    MakeShared<EGASLensIntent>(EGASLensIntent::L50),
    MakeShared<EGASLensIntent>(EGASLensIntent::L85),
    MakeShared<EGASLensIntent>(EGASLensIntent::L100)
};

static TArray<TSharedPtr<EGASCameraBehavior>> CameraBehaviorOptions = {
    MakeShared<EGASCameraBehavior>(EGASCameraBehavior::Static),
    MakeShared<EGASCameraBehavior>(EGASCameraBehavior::Pan),
    MakeShared<EGASCameraBehavior>(EGASCameraBehavior::Tilt),
    MakeShared<EGASCameraBehavior>(EGASCameraBehavior::Push),
    MakeShared<EGASCameraBehavior>(EGASCameraBehavior::Pull),
    MakeShared<EGASCameraBehavior>(EGASCameraBehavior::Handheld)
};

static float LensToFOV(EGASLensIntent Lens)
{
    switch (Lens)
    {
    case EGASLensIntent::L24:  return 84.f;
    case EGASLensIntent::L35:  return 63.f;
    case EGASLensIntent::L50:  return 47.f;
    case EGASLensIntent::L85:  return 28.f;
    case EGASLensIntent::L100: return 24.f;
    default:                  return 47.f;
    }
}

static float FOVToScale(float FOV)
{
    // 50mm ≈ 47° is neutral
    constexpr float NeutralFOV = 47.f;
    return FOV / NeutralFOV;
}



// =============================================================
//  Helpers part 1
// =============================================================

static void GatherScriptCharacters(
    const FGASScript* Script,
    TSet<FString>& OutCharacters)
{
    if (!Script)
        return;

    for (const FGASBlock& Block : Script->Blocks)
    {
        if (Block.Type == EGASBlockType::Character)
        {
            FString Name = Block.Text.TrimStartAndEnd();
            if (!Name.IsEmpty())
            {
                OutCharacters.Add(Name);
            }
        }
    }
}

void SGASScriptPanel::ModifyShotMarkerMetadata(
    const FGuid& ShotId,
    const FString& Key,
    const FString& Value)
{
    if (!SourceScript)
    {
        return;
    }

    const FString ShotIdString =
        ShotId.ToString(EGuidFormats::Digits);

    for (FGASMarker& Marker : SourceScript->Markers)
    {
        if (Marker.Id == ShotIdString)
        {
            Marker.Metadata.Add(Key, Value);
            RebuildLayout();
            return;
        }
    }
}


static void SyncShotCastWithScript(
    FGASScript* Script,
    const TSet<FString>& ScriptCharacters)
{
    if (!Script)
        return;

    // Build lookup of existing cast members
    TMap<FString, FGASShotCastMember*> Existing;
    for (FGASShotCastMember& Member : Script->ShotCast)
    {
        Existing.Add(Member.CharacterName, &Member);
    }

    // Add new characters
    for (const FString& Name : ScriptCharacters)
    {
        if (!Existing.Contains(Name))
        {
            FGASShotCastMember NewMember;
            NewMember.CharacterName = Name;
            NewMember.bEnabled = true;
            Script->ShotCast.Add(NewMember);
        }
    }

    // DO NOT remove missing characters yet
    // (user may want to keep disabled legacy entries)
}


class SGASShotCastList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGASShotCastList) {}
        SLATE_ARGUMENT(FGASScript*, Script)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Script = InArgs._Script;

        ChildSlot
            [
                SNew(SBorder)
                    .Padding(6.f)
                    .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                    [
                        SAssignNew(ListBox, SVerticalBox)
                    ]
            ];

        Rebuild();
    }

    void Rebuild()
    {
        if (!Script || !ListBox.IsValid())
            return;

        ListBox->ClearChildren();

        for (FGASShotCastMember& Member : Script->ShotCast)
        {
            ListBox->AddSlot()
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
                        .OnCheckStateChanged_Lambda([&Member](ECheckBoxState NewState)
                            {
                                Member.bEnabled = (NewState == ECheckBoxState::Checked);
                            })
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Member.CharacterName))
                        ]
                ];
        }
    }

private:
    FGASScript* Script = nullptr;
    TSharedPtr<SVerticalBox> ListBox;
};



// =============================================================
// Shot Intent Preview Widget (Lightweight, Procedural)
// =============================================================
class SGASShotPreview : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGASShotPreview) {}
        SLATE_ARGUMENT(FGASShotIntent*, ShotIntent)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ShotIntent = InArgs._ShotIntent;

        ChildSlot
            [
                SNew(SBorder)
                    .Padding(8.f)
                    .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                    [
                        SNew(SOverlay)
                    ]
            ];
    }

    virtual bool ComputeVolatility() const override
    {
        return true;
    }



    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override
    {
        bParentEnabled; // force Slate to treat as volatile draw


        if (!ShotIntent)
            return LayerId;

        const FVector2D Size = AllottedGeometry.GetLocalSize();
        const FVector2D Center = Size * 0.5f;

        FVector2D SubjectOffset = FVector2D::ZeroVector;
        float SubjectScale = 1.0f;


        const float BaseSubjectScale =
            ShotIntent->ShotType == EGASShotType::ECU ? 0.65f :
            ShotIntent->ShotType == EGASShotType::CU ? 0.55f :
            ShotIntent->ShotType == EGASShotType::MS ? 0.40f :
            0.25f;

        // -------------------------------------------------
        // Lens → FOV scaling (wider = smaller subject)
        // -------------------------------------------------
        const float FOV = LensToFOV(ShotIntent->Lens);
        const float ReferenceFOV = 47.f; // 50mm
        SubjectScale *= ReferenceFOV / FOV;

        const float Time = (float)FSlateApplication::Get().GetCurrentTime();

        // -------------------------------------------------
        // Subject silhouette by Shot Type
        // -------------------------------------------------
        const FLinearColor SilhouetteColor(0.75f, 0.75f, 0.75f, 1.f);

        // Base body proportions
        const float HeadRadius = Size.X * 0.06f * SubjectScale;
        const float TorsoWidth = Size.X * 0.18f * SubjectScale;
        const float TorsoHeight = Size.Y * 0.28f * SubjectScale;
        const float LegHeight = Size.Y * 0.22f * SubjectScale;

        FVector2D HeadCenter = Center + SubjectOffset;


        FVector2D TorsoSize = FVector2D(TorsoWidth, TorsoHeight);
        FVector2D TorsoPos = FVector2D(
            HeadCenter.X - TorsoSize.X * 0.5f,
            HeadCenter.Y - TorsoSize.Y * 0.5f
        );


        // -------------------------------------------------
        // Camera Behavior animation (illustrative only)
        // -------------------------------------------------
        switch (ShotIntent->CameraBehavior)
        {
        case EGASCameraBehavior::Pan:
            SubjectOffset.X += FMath::Sin(Time * 1.2f) * 20.f;
            break;

        case EGASCameraBehavior::Tilt:
            SubjectOffset.Y += FMath::Sin(Time * 1.2f) * 20.f;
            break;

        case EGASCameraBehavior::Push:
            SubjectScale *= 1.f + FMath::Sin(Time) * 0.05f;
            break;

        case EGASCameraBehavior::Pull:
            SubjectScale *= 1.f - FMath::Sin(Time) * 0.05f;
            break;

        case EGASCameraBehavior::Handheld:
            SubjectOffset.X += FMath::PerlinNoise1D(Time * 3.f) * 8.f;
            SubjectOffset.Y += FMath::PerlinNoise1D(Time * 3.7f) * 8.f;
            break;

        default:
            break;
        }

        if (ShotIntent->Framing == EGASFramingBias::Left)  SubjectOffset.X -= 30.f;
        if (ShotIntent->Framing == EGASFramingBias::Right) SubjectOffset.X += 30.f;
        if (ShotIntent->Framing == EGASFramingBias::High)  SubjectOffset.Y -= 30.f;
        if (ShotIntent->Framing == EGASFramingBias::Low)   SubjectOffset.Y += 30.f;

        // -----------------------------
        // ECU / CU — HEAD ONLY
        // -----------------------------
        if (ShotIntent->ShotType == EGASShotType::ECU ||
            ShotIntent->ShotType == EGASShotType::CU)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId++,
                AllottedGeometry.ToPaintGeometry(
                    HeadCenter - FVector2D(HeadRadius, HeadRadius),
                    FVector2D(HeadRadius * 2.f, HeadRadius * 2.f)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SilhouetteColor
            );
        }

        // -----------------------------
        // MCU / MS — HEAD + TORSO
        // -----------------------------
        else if (ShotIntent->ShotType == EGASShotType::MCU ||
            ShotIntent->ShotType == EGASShotType::MS)
        {
            // Head
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId++,
                AllottedGeometry.ToPaintGeometry(
                    HeadCenter - FVector2D(HeadRadius, HeadRadius + TorsoHeight * 0.6f),
                    FVector2D(HeadRadius * 2.f, HeadRadius * 2.f)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SilhouetteColor
            );

            // Torso
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId++,
                AllottedGeometry.ToPaintGeometry(
                    TorsoPos,
                    TorsoSize
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SilhouetteColor
            );
        }

        // -----------------------------
        // MLS / WS / EWS — FULL BODY
        // -----------------------------
        else
        {
            // Head
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId++,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(HeadCenter.X - HeadRadius, Center.Y - TorsoHeight * 0.7f),
                    FVector2D(HeadRadius * 2.f, HeadRadius * 2.f)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SilhouetteColor
            );

            // Torso
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId++,
                AllottedGeometry.ToPaintGeometry(
                    TorsoPos,
                    TorsoSize
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SilhouetteColor
            );

            // Legs
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId++,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(
                        HeadCenter.X - TorsoWidth * 0.25f,
                        TorsoPos.Y + TorsoHeight
                    ),
                    FVector2D(
                        TorsoWidth * 0.5f,
                        LegHeight
                    )
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SilhouetteColor
            );
        }





        // -------------------------------------------------
        // Draw frame
        // -------------------------------------------------
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId++,
            AllottedGeometry.ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor::Black
        );

        // force continuous repaint for animation
        const_cast<SGASShotPreview*>(this)->Invalidate(EInvalidateWidget::Paint);

        return LayerId;
    }

private:
    FGASShotIntent* ShotIntent = nullptr;

    mutable bool bDirty = true;

};



// =============================================================
//  Construct
// =============================================================
void SGASScriptPanel::Construct(const FArguments& InArgs)
{
    OnParagraphClicked = InArgs._OnParagraphClicked;

    SetCanTick(true);

    ShotSelectionState = NewObject<UGASShotListSelectionState>();

    bNeedsRedraw = true;

    ChildSlot
        [
            SNew(SBorder)
                .Padding(0)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                .Clipping(EWidgetClipping::ClipToBoundsAlways)
        ];

}

void SGASScriptPanel::RebuildLayout()
{
    if (!SourceScript)
        return;

    // Panel width: fall back to something sane if geometry is tiny at startup
    float PanelWidth = GetCachedGeometry().GetLocalSize().X;
    if (PanelWidth < 200.f)
    {
        PanelWidth = 1100.f;
    }

    TArray<FRenderedParagraph> NewRendered =
        ScriptLayoutEngine::LayoutScript(
            SourceScript->Blocks,
            PanelWidth,
            SourceScript->UserPageBreaks,
            SourceScript->SceneNumbering
        );



    SetRenderedScript(NewRendered);   // updates RenderedParagraphs + bNeedsRedraw

    // ------------------------------------------------------------
    // Auto-adjust shot tail to paragraph end
    // (keeps tail attached when text grows)
    // ------------------------------------------------------------
    if (SourceScript)
    {
        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.MarkerType != EGASMarkerType::ShotMarker)
                continue;

            if (!RenderedParagraphs.IsValidIndex(M.ParagraphIndex))
                continue;

            // Only adjust tails that are defined
            if (M.ShotLineEndY < 0.f)
                continue;

            const FRenderedParagraph& P =
                RenderedParagraphs[M.ParagraphIndex];

            const float ParagraphEndY =
                P.TopY + P.Height;

            // Clamp tail to paragraph end if it was at or below it
            if (M.ShotLineEndY <= ParagraphEndY + ScriptFormat::LineHeight)
            {
                M.ShotLineEndY = ParagraphEndY;
            }
        }
    }


    if (OnRequestFullRedraw.IsBound())
    {
        OnRequestFullRedraw.Execute();
    }


}

void SGASScriptPanel::SetScript(FGASScript* InScript)
{
    SourceScript = InScript;

    // Fresh script = fresh layout
    RebuildLayout();

    // ------------------------------------------------------------
    // Reset scroll state on script load
    // ------------------------------------------------------------
    ScrollOffsetY = 0.f;
    PendingScrollBlockId.Reset();
    PendingScrollParagraph = INDEX_NONE;

    bIgnoreNextExternalScrollOnSetScript = true;

    bNeedsRedraw = true;

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}



// =============================================================
//  SetRenderedScript
// =============================================================
void SGASScriptPanel::SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs)
{
    RenderedParagraphs = InParagraphs;

    if (SourceScript)
    {
        TSet<FString> ScriptCharacters;
        GatherScriptCharacters(SourceScript, ScriptCharacters);
        SyncShotCastWithScript(SourceScript, ScriptCharacters);

    }


    bNeedsRedraw = true;
}

// =============================================================
//  ComputeDesiredSize
// =============================================================
FVector2D SGASScriptPanel::ComputeDesiredSize(float) const
{
    return FVector2D(1000.f, CachedTotalHeight);
}

// =====================================================
// SCRIPT EDIT PIPELINE
// =====================================================

void SGASScriptPanel::ApplyScriptEdit(const FGASScriptEdit& Edit)
{
    if (!SourceScript)
        return;

    if (Edit.ParagraphIndex == INDEX_NONE)
        return;

    switch (Edit.Type)
    {
    case EGASScriptEditType::SetPageBreak:
    {
        const int32 ParagraphIndex = Edit.ParagraphIndex;

        if (RenderedParagraphs.IsValidIndex(ParagraphIndex))
        {
            const FString& BlockId =
                RenderedParagraphs[ParagraphIndex].BlockId;

            // Prevent duplicate page breaks on the same block
            bool bAlreadyHasBreak = false;
            for (const FGASUserPageBreak& PB : SourceScript->UserPageBreaks)
            {
                if (PB.AfterBlockId == BlockId)
                {
                    bAlreadyHasBreak = true;
                    break;
                }
            }

            if (!bAlreadyHasBreak)
            {
                FGASUserPageBreak NewBreak;
                NewBreak.AfterBlockId = BlockId;

                SourceScript->UserPageBreaks.Add(NewBreak);
            }
        }
        break;
    }





    case EGASScriptEditType::ClearPageBreak:
    {
        const int32 ParagraphIndex = Edit.ParagraphIndex;

        if (RenderedParagraphs.IsValidIndex(ParagraphIndex))
        {
            const FString& BlockId =
                RenderedParagraphs[ParagraphIndex].BlockId;

            SourceScript->UserPageBreaks.RemoveAll(
                [&](const FGASUserPageBreak& PB)
                {
                    return PB.AfterBlockId == BlockId;
                }
            );
        }
        break;
    }




    default:
        break;
    }



    // Always re-layout after edits so visuals match JSON
    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);

}

void SGASScriptPanel::ApplyTextEdit(
    const FString& BlockId,
    const FString& NewText)
{
    if (!SourceScript)
        return;

    for (FGASBlock& Block : SourceScript->Blocks)
    {
        if (Block.Id == BlockId)
        {
            Block.Text = NewText;
            break;
        }
    }

    if (OnScriptMutated.IsBound())
    {
        UE_LOG(LogTemp, Warning, TEXT("SCRIPT MUTATED → notifying owner"));
        OnScriptMutated.Execute();
    }


    RebuildLayout();
}


int32 SGASScriptPanel::HitTestAddGutter(float MouseY) const
{
    if (RenderedParagraphs.Num() < 1)
    {
        return INDEX_NONE;
    }

    constexpr float GutterTolerance = 12.f;

    for (int32 i = 0; i < RenderedParagraphs.Num() - 1; ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        const float GutterY =
            P.TopY + P.Height + (ScriptFormat::LineHeight * 0.25f);

        if (FMath::Abs(MouseY - GutterY) <= GutterTolerance)
        {
            return P.ParagraphIndex; // insert AFTER this paragraph
        }
    }

    return INDEX_NONE;
}

void SGASScriptPanel::TryEditParagraph(int32 BlockIndex)
{
    if (!SourceScript)
        return;

    if (!SourceScript->Blocks.IsValidIndex(BlockIndex))
        return;

    // 🔑 SELECT FIRST (so highlight updates immediately)
    SelectedParagraphIndex = BlockIndex;
    Invalidate(EInvalidateWidget::Paint);

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    switch (Block.Type)
    {
    case EGASBlockType::Action:
        OpenEditActionDialog(BlockIndex);
        break;

    case EGASBlockType::Character:
        OpenEditCharacterDialog(BlockIndex);
        break;

    case EGASBlockType::Dialogue:
        OpenEditDialogueDialog(BlockIndex);
        break;

    default:
        break;
    }
}


void SGASScriptPanel::OpenEditActionDialog(int32 BlockIndex)
{

    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("OpenEditActionDialog: SourceScript null or invalid index %d"), BlockIndex);
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];


    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Action")))
        .ClientSize(FVector2D(600.f, 300.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    TSharedPtr<SMultiLineEditableTextBox> TextBox;

    EditWindow->SetContent(
        SNew(SBorder)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Text box
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                .Padding(8.f)
                [
                    SAssignNew(EditTextBox, SMultiLineEditableTextBox)
                        .Text(FText::FromString(Block.Text))
                        .OnTextChanged_Lambda([this](const FText& NewText)
                            {
                                CachedEditText = NewText;
                            })
                ]

            // ----------------------------------------------------
            // Buttons
            // ----------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(8.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([this, EditWindow]()
                                    {
                                        OnEditCancelled();
                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, EditWindow, BlockIndex]()
                                    {
                                        if (SourceScript && SourceScript->Blocks.IsValidIndex(BlockIndex))
                                        {
                                            SourceScript->CaptureUndoSnapshot();

                                            SourceScript->Blocks[BlockIndex].Text = CachedEditText.ToString();

                                            if (OnScriptMutated.IsBound())
                                            {
                                                OnScriptMutated.Execute();
                                            }

                                            RebuildLayout();
                                            Invalidate(EInvalidateWidget::LayoutAndVolatility);
                                        }

                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]
                ]
        ]
    );


    FSlateApplication::Get().AddModalWindow(
        EditWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );

    FSlateApplication::Get().SetKeyboardFocus(EditTextBox);

}

void SGASScriptPanel::OpenEditCharacterDialog(int32 BlockIndex)
{
    if (!SourceScript  || !SourceScript ->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    FGASBlock& Block = SourceScript ->Blocks[BlockIndex];

    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Character")))
        .ClientSize(FVector2D(400.f, 160.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CachedEditText = FText::FromString(Block.Text);

    EditWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SAssignNew(EditTextBox, SMultiLineEditableTextBox)
                        .Text(CachedEditText)
                        .OnTextChanged_Lambda([this](const FText& NewText)
                            {
                                CachedEditText = NewText;
                            })
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([this, EditWindow]()
                                    {
                                        OnEditCancelled();
                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, EditWindow, BlockIndex]()
                                    {
                                        SourceScript->Blocks[BlockIndex].Text =
                                            CachedEditText.ToString().ToUpper();

                                        FGAS_PreProToolsEditorModule& Module =
                                            FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
                                                "GAS_PreProToolsEditor"
                                            );

                                        Module.MarkToolDirty();


                                        RebuildLayout();
                                        Invalidate(EInvalidateWidget::LayoutAndVolatility);

                                        // ----------------------------------------------------
                                        // If this Character was added with a Dialogue, open it next
                                        // ----------------------------------------------------
                                        if (PendingDialogueAfterCharacterIndex != INDEX_NONE)
                                        {
                                            const int32 DialogueIndex = PendingDialogueAfterCharacterIndex;
                                            PendingDialogueAfterCharacterIndex = INDEX_NONE;

                                            FTSTicker::GetCoreTicker().AddTicker(
                                                FTickerDelegate::CreateLambda(
                                                    [this, DialogueIndex](float)
                                                    {
                                                        OpenEditDialogueDialog(DialogueIndex);
                                                        return false; // one-shot
                                                    }
                                                )
                                            );
                                        }

                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        EditWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );

    FSlateApplication::Get().SetKeyboardFocus(EditTextBox);
}

void SGASScriptPanel::OpenAddBlockDialog(int32 InsertAfterParagraphIndex)
{
    if (!SourceScript)
    {
        return;
    }

    TSharedRef<SWindow> AddWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Add Block")))
        .ClientSize(FVector2D(300.f, 160.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    // 0 = Action, 1 = Character + Dialogue, 2 = Scene Heading
    TSharedPtr<int32> SelectedType = MakeShared<int32>(0);

    AddWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Block type selection
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 6.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Block Type")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([SelectedType]()
                            {
                                return (*SelectedType == 0)
                                    ? ECheckBoxState::Checked
                                    : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged_Lambda([SelectedType](ECheckBoxState)
                            {
                                *SelectedType = 0;
                            })
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Action")))
                        ]
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([SelectedType]()
                            {
                                return (*SelectedType == 1)
                                    ? ECheckBoxState::Checked
                                    : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged_Lambda([SelectedType](ECheckBoxState)
                            {
                                *SelectedType = 1;
                            })
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Character + Dialogue")))
                        ]
                ]
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([SelectedType]()
                            {
                                return (*SelectedType == 2)
                                    ? ECheckBoxState::Checked
                                    : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged_Lambda([SelectedType](ECheckBoxState)
                            {
                                *SelectedType = 2;
                            })
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Scene Heading")))
                        ]
                ]

            // ----------------------------------------------------
            // OK / Cancel
            // ----------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([AddWindow]()
                                    {
                                        AddWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, AddWindow, InsertAfterParagraphIndex, SelectedType]()
                                    {
                                        InsertNewBlock(
                                            InsertAfterParagraphIndex,
                                            *SelectedType
                                        );

                                        AddWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        AddWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );
}

void SGASScriptPanel::OpenDialoguePreviewDialog(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    const FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    TSharedRef<SWindow> PreviewWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Dialogue Preview")))
        .ClientSize(FVector2D(500.f, 260.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    PreviewWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Info label
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 6.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Dialogue editing coming soon")))
                        .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.f))
                ]

                // ----------------------------------------------------
                // Dialogue text (read-only)
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SNew(SMultiLineEditableTextBox)
                        .Text(FText::FromString(Block.Text))
                        .IsReadOnly(true)
                ]

                // ----------------------------------------------------
                // Close button
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Close")))
                        .OnClicked_Lambda([PreviewWindow]()
                            {
                                PreviewWindow->RequestDestroyWindow();
                                return FReply::Handled();
                            })
                ]
        ]
    );


    FSlateApplication::Get().AddModalWindow(
        PreviewWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );
}

void SGASScriptPanel::OpenEditDialogueDialog(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Dialogue")))
        .ClientSize(FVector2D(500.f, 280.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CachedEditText = FText::FromString(Block.Text);

    EditWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Dialogue text
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SAssignNew(EditTextBox, SMultiLineEditableTextBox)
                        .Text(CachedEditText)
                        .OnTextChanged_Lambda([this](const FText& NewText)
                            {
                                CachedEditText = NewText;
                            })
                        .OnKeyDownHandler_Lambda([this, EditWindow, BlockIndex](const FGeometry&, const FKeyEvent& KeyEvent)
                            {
                                // ESC = cancel
                                if (KeyEvent.GetKey() == EKeys::Escape)
                                {
                                    OnEditCancelled();
                                    EditWindow->RequestDestroyWindow();
                                    return FReply::Handled();
                                }


                                // CTRL + ENTER = accept
                                if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsControlDown())
                                {
                                    if (SourceScript && SourceScript->Blocks.IsValidIndex(BlockIndex))
                                    {
                                        SourceScript->Blocks[BlockIndex].Text =
                                            CachedEditText.ToString();

                                        FGAS_PreProToolsEditorModule& Module =
                                            FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
                                                "GAS_PreProToolsEditor"
                                            );

                                        Module.MarkToolDirty();


                                        RebuildLayout();
                                        Invalidate(EInvalidateWidget::LayoutAndVolatility);
                                    }

                                    EditWindow->RequestDestroyWindow();
                                    return FReply::Handled();
                                }

                                return FReply::Unhandled();
                            })
                ]

            // ----------------------------------------------------
            // Buttons
            // ----------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([this, EditWindow]()
                                    {
                                        OnEditCancelled();
                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, EditWindow, BlockIndex]()
                                    {
                                        if (SourceScript && SourceScript->Blocks.IsValidIndex(BlockIndex))
                                        {
                                            SourceScript->Blocks[BlockIndex].Text =
                                                CachedEditText.ToString();

                                            FGAS_PreProToolsEditorModule& Module =
                                                FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
                                                    "GAS_PreProToolsEditor"
                                                );

                                            Module.MarkToolDirty();


                                            RebuildLayout();
                                            Invalidate(EInvalidateWidget::LayoutAndVolatility);
                                        }

                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        EditWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );

    FSlateApplication::Get().SetKeyboardFocus(EditTextBox);
}

void SGASScriptPanel::SetEditMode(bool bInEditMode)
{
    bEditMode = bInEditMode;

    if (bEditMode)
    {
        bAddMode = false;

        // 🔒 Clear any stale Add-mode rollback state
        PendingAddBlockCount = 0;
        PendingAddStartIndex = INDEX_NONE;
        PendingEditBlockIndex = INDEX_NONE;
    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SGASScriptPanel::SetAddMode(bool bInAddMode)
{
    bAddMode = bInAddMode;

    // Add and Edit are mutually exclusive
    if (bAddMode)
    {
        bEditMode = false;
    }
}

void SGASScriptPanel::ResetEditorModes()
{
    // Core modes
    bEditMode = false;
    bAddMode = false;

    // Shot add / selection modes
    bShotAddArmed = false;
    bIsShotRangeDragging = false;
    bIsShotSelectMode = false;
    bIsDraggingShot = false;

    // Shot UI state
    HoveredShotMarkerId.Empty();
    SelectedShotMarkerId.Empty();
    HoveredShotParagraphIndex = INDEX_NONE;

    // Pending add/edit rollback safety
    PendingAddBlockCount = 0;
    PendingAddStartIndex = INDEX_NONE;
    PendingEditBlockIndex = INDEX_NONE;

    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);
}


void SGASScriptPanel::InsertNewBlock(
    int32 InsertAfterParagraphIndex,
    int32 BlockTypeChoice
)
{
    if (!SourceScript)
    {
        return;
    }

    // UNDO: capture state BEFORE mutation
    SourceScript->CaptureUndoSnapshot();

    const int32 InsertIndex = InsertAfterParagraphIndex + 1;

    if (BlockTypeChoice == 0)
    {
        FGASBlock NewBlock;
        NewBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        NewBlock.Type = EGASBlockType::Action;
        NewBlock.Text = TEXT("");

        SourceScript->Blocks.Insert(NewBlock, InsertIndex);

        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 1;
        PendingEditBlockIndex = InsertIndex;
    }
    else if (BlockTypeChoice == 2) // Scene Heading
    {
        // Resolve current scene numbers (authoritative) BEFORE insertion
        const TMap<FString, FString> SceneNumberMap =
            FGASSceneNumberResolver::ResolveSceneNumbers(
                SourceScript->Blocks,
                SourceScript->SceneNumbering
            );

        FGASBlock SceneBlock;
        SceneBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        SceneBlock.Type = EGASBlockType::SceneHeading;
        SceneBlock.Text = TEXT("NEW SCENE");

        // Find nearest previous SceneHeading (base)
        FString BaseSceneLabel;

        for (int32 i = InsertAfterParagraphIndex; i >= 0; --i)
        {
            if (SourceScript->Blocks.IsValidIndex(i) &&
                SourceScript->Blocks[i].Type == EGASBlockType::SceneHeading)
            {
                const FString PrevId = SourceScript->Blocks[i].Id;
                BaseSceneLabel = SceneNumberMap.FindRef(PrevId);
                break;
            }
        }

        if (BaseSceneLabel.IsEmpty())
        {
            BaseSceneLabel = TEXT("INS");
        }

        // Count existing inserts under this base label
        int32 InsertCount = 1;
        for (const FGASBlock& Block : SourceScript->Blocks)
        {
            if (Block.Type != EGASBlockType::SceneHeading)
                continue;

            const FString Existing = Block.Metadata.FindRef(TEXT("SceneNumber"));
            if (Existing.StartsWith(BaseSceneLabel + TEXT("X")))
            {
                InsertCount++;
            }
        }

        const FString InsertLabel =
            FString::Printf(TEXT("%sX%d"), *BaseSceneLabel, InsertCount);

        // Lock this number so it never renumbers anything
        SceneBlock.Metadata.Add(TEXT("SceneNumber"), InsertLabel);
        SceneBlock.Metadata.Add(TEXT("SceneLocked"), TEXT("1"));

        SourceScript->Blocks.Insert(SceneBlock, InsertIndex);

        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 1;
        PendingEditBlockIndex = InsertIndex;
    }


    else
    {
        FGASBlock CharacterBlock;
        CharacterBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        CharacterBlock.Type = EGASBlockType::Character;
        CharacterBlock.Text = TEXT("");

        FGASBlock DialogueBlock;
        DialogueBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        DialogueBlock.Type = EGASBlockType::Dialogue;
        DialogueBlock.Text = TEXT("");

        SourceScript->Blocks.Insert(CharacterBlock, InsertIndex);
        SourceScript->Blocks.Insert(DialogueBlock, InsertIndex + 1);

        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 2;
        PendingEditBlockIndex = InsertIndex;
        PendingDialogueAfterCharacterIndex = InsertIndex + 1;
    }


    // Mark dirty
    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();


    RebuildLayout();
    if (OnScriptMutated.IsBound())
    {
        OnScriptMutated.Execute();
    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);

    // Immediately open edit dialog for the newly inserted block
    if (PendingEditBlockIndex != INDEX_NONE)
    {
        TryEditParagraph(PendingEditBlockIndex);
        PendingEditBlockIndex = INDEX_NONE;
    }
}

void SGASScriptPanel::OnEditCancelled()
{
    if (!SourceScript)
    {
        return;
    }

    // 🔒 ONLY rollback if we are CURRENTLY in Add Mode
    if (bAddMode &&
        PendingAddBlockCount > 0 &&
        PendingAddStartIndex != INDEX_NONE &&
        SourceScript->Blocks.IsValidIndex(PendingAddStartIndex))
    {
        SourceScript->Blocks.RemoveAt(
            PendingAddStartIndex,
            PendingAddBlockCount,
            /*bAllowShrinking=*/false
        );
    }

    PendingAddBlockCount = 0;
    PendingAddStartIndex = INDEX_NONE;
    PendingEditBlockIndex = INDEX_NONE;

    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


void SGASScriptPanel::OpenDeleteBlockDialog(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    FString BlockLabel;
    switch (Block.Type)
    {
    case EGASBlockType::Action:     BlockLabel = TEXT("Action"); break;
    case EGASBlockType::Character:  BlockLabel = TEXT("Character"); break;
    case EGASBlockType::Dialogue:   BlockLabel = TEXT("Dialogue"); break;
    default:                        BlockLabel = TEXT("Block"); break;
    }

    TSharedRef<SWindow> ConfirmWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Delete Block")))
        .ClientSize(FVector2D(360.f, 140.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    ConfirmWindow->SetContent(
        SNew(SBorder)
        .Padding(12.f)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 10.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(
                            FString::Printf(TEXT("Delete this %s block?"), *BlockLabel)
                        ))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([ConfirmWindow]()
                                    {
                                        ConfirmWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Delete")))
                                .OnClicked_Lambda([this, ConfirmWindow, BlockIndex]()
                                    {
                                        DeleteBlock(BlockIndex);
                                        ConfirmWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        ConfirmWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );
}

void SGASScriptPanel::DeleteBlock(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    // UNDO: capture state BEFORE deletion
    SourceScript->CaptureUndoSnapshot();


    // If deleting a Character, also delete its Dialogue if present
    if (SourceScript->Blocks[BlockIndex].Type == EGASBlockType::Character)
    {
        if (SourceScript->Blocks.IsValidIndex(BlockIndex + 1) &&
            SourceScript->Blocks[BlockIndex + 1].Type == EGASBlockType::Dialogue)
        {
            SourceScript->Blocks.RemoveAt(BlockIndex, 2);
        }
        else
        {
            SourceScript->Blocks.RemoveAt(BlockIndex);
        }
    }
    // If deleting Dialogue, check if previous is Character
    else if (SourceScript->Blocks[BlockIndex].Type == EGASBlockType::Dialogue)
    {
        if (BlockIndex > 0 &&
            SourceScript->Blocks[BlockIndex - 1].Type == EGASBlockType::Character)
        {
            SourceScript->Blocks.RemoveAt(BlockIndex - 1, 2);
        }
        else
        {
            SourceScript->Blocks.RemoveAt(BlockIndex);
        }
    }
    else
    {
        // Action or other block
        SourceScript->Blocks.RemoveAt(BlockIndex);
    }

    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();


    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

FReply SGASScriptPanel::OnKeyDown(
    const FGeometry& MyGeometry,
    const FKeyEvent& InKeyEvent
)
{
    // CTRL + Z → Undo
    if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::Z)
    {
        if (SourceScript && SourceScript->CanUndo())
        {
            SourceScript->Undo();
            RebuildLayout();
            Invalidate(EInvalidateWidget::LayoutAndVolatility);
            return FReply::Handled();
        }
    }

    // CTRL + Y → Redo
    if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::Y)
    {
        if (SourceScript && SourceScript->CanRedo())
        {
            SourceScript->Redo();
            RebuildLayout();
            Invalidate(EInvalidateWidget::LayoutAndVolatility);
            return FReply::Handled();
        }
    }

    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SGASScriptPanel::ScrollToParagraph(int32 ParagraphIndex)
{
    if (!RenderedParagraphs.IsValidIndex(ParagraphIndex))
    {
        return;
    }

    PendingScrollParagraph = ParagraphIndex;
    bPendingScrollAfterLayout = true;

    Invalidate(EInvalidateWidget::Paint);
}




void SGASScriptPanel::ScrollToBlockId(const FString& BlockId)
{
    if (!OnRequestExternalScroll.IsBound())
    {
        return;
    }

    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        if (P.BlockId == BlockId)
        {
            OnRequestExternalScroll.Execute(P.TopY);
            return;
        }
    }
}






FString SGASScriptPanel::GetSelectedBlockId() const
{
    if (!RenderedParagraphs.IsValidIndex(SelectedParagraphIndex))
    {
        return FString();
    }

    return RenderedParagraphs[SelectedParagraphIndex].BlockId;
}




// =============================================================
//  PAINT — Draw text, page breaks, preview line
// =============================================================
int32 SGASScriptPanel::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    const float PaintScrollOffsetY = 0.f;


    LastKnownViewportHeight = MyCullingRect.GetSize().Y;

    // Track total height
    CachedTotalHeight = 0.f;
    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        CachedTotalHeight = FMath::Max(
            CachedTotalHeight,
            P.TopY + P.Height
        );
    }

    const FSlateFontInfo FontInfo =
        FGAS_PreProToolsStyle::Get().GetFontStyle("GAS.ScriptFont");

    const TSharedRef<FSlateFontMeasure> FontMeasureService =
        FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    ShotHitRects.Reset();


    // ------------------------------------------------------------
    // Build Scene → Shot stack map DONE
    // ------------------------------------------------------------
    TMap<FString, TArray<const FGASMarker*>> SceneShotMap;

    if (SourceScript)
    {
        for (const FGASMarker& Marker : SourceScript->Markers)
        {
            if (Marker.MarkerType != EGASMarkerType::ShotMarker)
                continue;

            SceneShotMap
                .FindOrAdd(Marker.BlockId)
                .Add(&Marker);
        }

        for (auto& Pair : SceneShotMap)
        {
            Pair.Value.Sort(
                [](const FGASMarker& A, const FGASMarker& B)
                {
                    if (A.ParagraphIndex != B.ParagraphIndex)
                        return A.ParagraphIndex < B.ParagraphIndex;

                    return A.LineIndex < B.LineIndex;
                }
            );

        }
    }

    // ------------------------------------------------------------
    // Shot Range Drag Preview DONE
    // ------------------------------------------------------------
    if (bIsShotRangeDragging &&
        RenderedParagraphs.IsValidIndex(ShotRangeStartParagraph) &&
        RenderedParagraphs.IsValidIndex(ShotRangeCurrentParagraph))
    {
        const int32 Start =
            FMath::Min(ShotRangeStartParagraph, ShotRangeCurrentParagraph);

        const int32 End =
            FMath::Max(ShotRangeStartParagraph, ShotRangeCurrentParagraph);

        const float TopY =
            RenderedParagraphs[Start].TopY;

        const float BottomY =
            RenderedParagraphs[End].TopY +
            RenderedParagraphs[End].Height;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 30,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(0.f, TopY),
                FVector2D(
                    AllottedGeometry.GetLocalSize().X,
                    BottomY - TopY
                )
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor(0.25f, 0.6f, 1.f, 0.18f) // translucent blue
        );
    }

    for (int32 i = 0; i < RenderedParagraphs.Num(); i++)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];
        const float PaintY = P.TopY;


        // ------------------------------------------------------------
        // Draw selection highlight (Edit Mode only)
        // ------------------------------------------------------------
        if (bEditMode && i == SelectedParagraphIndex)
        {
            const float HighlightY = P.TopY;

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(0.f, HighlightY),
                    FVector2D(AllottedGeometry.GetLocalSize().X, P.Height)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.15f, 0.35f, 0.6f, 0.25f)
            );
        }



        // ------------------------------------------------------------
        // Draw SCENE NUMBER (optional, margin only — no layout impact)
        // ------------------------------------------------------------
        if (bShowSceneNumbers &&
            P.BlockType == EGASBlockType::SceneHeading)


        {
            if (!P.SceneLabel.IsEmpty())
            {
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(
                            ScriptFormat::PageLeft + P.IndentLeft - 50.f,
                            PaintY
                        ),
                        FVector2D::UnitVector
                    ),
                    FText::FromString(P.SceneLabel),
                    FontInfo,
                    ESlateDrawEffect::None,
                    FLinearColor(1.f, 0.f, 0.f, 1.f)
                );
            }

        }

        // ------------------------------------------------------------
        // Draw stacked SHOT MARKERS under Scene Heading
        // ------------------------------------------------------------
        if (P.BlockType == EGASBlockType::SceneHeading &&
            SourceScript)
        {
            const TArray<const FGASMarker*>* Shots =
                SceneShotMap.Find(P.BlockId);

            if (Shots && Shots->Num() > 0)
            {
                for (const FGASMarker* Shot : *Shots)
                {
                    int32 ShotParaIndex = Shot->ParagraphIndex;

                    // Fallback for legacy / derived shots
                    if (!RenderedParagraphs.IsValidIndex(ShotParaIndex))
                    {
                        for (int32 p = 0; p < RenderedParagraphs.Num(); ++p)
                        {
                            if (RenderedParagraphs[p].BlockId == Shot->BlockId)
                            {
                                ShotParaIndex = p;
                                break;
                            }
                        }
                    }

                    // ------------------------------------------------------------
                    // TEST 10B — Visual Y comes ONLY from stored ScreenY PILL
                    // ------------------------------------------------------------
                    if (Shot->ScreenY < 0.f)
                    {
                        continue;
                    }

                    const float PillY =
                        Shot->ScreenY;

                    const float PillX =
                        Shot->ScreenX >= 0.f
                        ? Shot->ScreenX
                        : (ScriptFormat::PageLeft + P.IndentLeft);

                    const bool bIsHovered =
                        (HoveredShotMarkerId == Shot->Id);

                    const bool bIsSelected =
                        (SelectedShotMarkerId == Shot->Id);

                    const FString* ShotType =
                        Shot->Metadata.Find(TEXT("Type"));

                    const bool bIsShotDefined =
                        (ShotType && !ShotType->IsEmpty() && *ShotType != TEXT("—"));

                    const FLinearColor ShotLineColor =
                        bIsShotDefined
                        ? FLinearColor(0.25f, 0.6f, 1.f, 0.9f)      // BLUE
                        : FLinearColor(0.95f, 0.85f, 0.25f, 0.95f); // YELLOW


                    // ------------------------------------------------------------
                    // Draw PERMANENT shot range line (stored)
                    // ------------------------------------------------------------
                    if (Shot->ShotLineStartY >= 0.f && Shot->ShotLineEndY >= 0.f)
                    {
                        const float LineX =
                            Shot->ScreenX + (ShotPillWidth * 0.5f);


                        // Line starts at bottom of pill
                        const float LineStartY =
                            Shot->ShotLineStartY;

                        const float LineEndY =
                            Shot->ShotLineEndY;


                        const float TopY =
                            FMath::Min(LineStartY, LineEndY);

                        const float BottomY =
                            FMath::Max(LineStartY, LineEndY);



                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 1, // below pill, above text
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(LineX - 1.f, TopY),
                                FVector2D(2.f, BottomY - TopY)
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            ShotLineColor
                        );

                        // Horizontal end-cap (≈ 4 characters wide)
                        const float CharWidth =
                            FontMeasureService->Measure(TEXT("M"), FontInfo).X;

                        const float CapHalfWidth = (CharWidth * 4.f) * 0.5f;

                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 1,
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(LineX - CapHalfWidth, BottomY - 1.f),
                                FVector2D(CharWidth * 4.f, 2.f)
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            ShotLineColor
                        );


                    }

                    // ------------------------------------------------------------
                    // Shot tail resize ghost (ALT + drag)
                    // ------------------------------------------------------------
                    if (bIsResizingShotTail &&
                        ResizingShotMarkerId == Shot->Id &&
                        ShotTailGhostY >= 0.f)
                    {
                        const float LineX =
                            Shot->ScreenX + (ShotPillWidth * 0.5f);

                        const float TopY = FMath::Min(ShotTailResizeStartY, ShotTailGhostY);
                        const float BottomY = FMath::Max(ShotTailResizeStartY, ShotTailGhostY);


                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 5,
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(LineX - 1.f, TopY),
                                FVector2D(2.f, BottomY - TopY)
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            FLinearColor(0.25f, 0.6f, 1.f, 0.55f)
                        );
                    }


                    // ------------------------------------------------------------
                    // Shot tail resize ghost HANDLE
                    // ------------------------------------------------------------
                    if (bIsResizingShotTail &&
                        ResizingShotMarkerId == Shot->Id &&
                        ShotTailGhostY >= 0.f)
                    {
                        const float LineX =
                            Shot->ScreenX + (ShotPillWidth * 0.5f);

                        constexpr float HandleHalfWidth = 8.f;
                        constexpr float HandleHalfHeight = 3.f;

                        const float HandleY =
                            ShotTailGhostY;

                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 6,
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(
                                    LineX - HandleHalfWidth,
                                    HandleY - HandleHalfHeight
                                ),
                                FVector2D(
                                    HandleHalfWidth * 2.f,
                                    HandleHalfHeight * 2.f
                                )
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            FLinearColor(0.25f, 0.6f, 1.f, 0.9f)
                        );
                    }


                    // Background pill
                    const FLinearColor ShotFillColor =
                        bIsSelected
                        ? FLinearColor(0.2f, 0.8f, 0.35f, 1.0f)   // 🟢 SELECTED
                        : bIsHovered
                        ? ShotLineColor.CopyWithNewOpacity(0.85f)
                        : ShotLineColor.CopyWithNewOpacity(1.0f);

                    FSlateDrawElement::MakeBox(
                        OutDrawElements,
                        LayerId + 2,
                        AllottedGeometry.ToPaintGeometry(
                            FVector2D(PillX, PillY),
                            FVector2D(48.f, 18.f)
                        ),
                        FCoreStyle::Get().GetBrush("WhiteBrush"),
                        ESlateDrawEffect::None,
                        ShotFillColor
                    );



                    // ------------------------------------------------------------
                    // Shot label — computed dynamically (NO cached ShotId)
                    // ------------------------------------------------------------
                    const FString ShotLabel =
                        FString::FromInt(Shot->ShotIndex);



                    const FVector2D TextSize =
                        FontMeasureService->Measure(ShotLabel, FontInfo);

                    FSlateDrawElement::MakeText(
                        OutDrawElements,
                        LayerId + 3,
                        AllottedGeometry.ToPaintGeometry(
                            FVector2D(
                                PillX + (ShotPillWidth - TextSize.X) * 0.5f,
                                PillY + (ShotPillHeight - TextSize.Y) * 0.5f
                            ),
                            FVector2D::UnitVector
                        ),
                        ShotLabel,
                        FontInfo,
                        ESlateDrawEffect::None,
                        bIsShotDefined
                        ? FLinearColor(1.f, 1.f, 1.f, 1.f)   // WHITE on blue
                        : FLinearColor(0.f, 0.f, 0.f, 1.f)   // BLACK on yellow

                    );



                    // Hit rect
                    FShotHitRect Hit;
                    Hit.MarkerId = Shot->Id;
                    Hit.Rect = FSlateRect(
                        PillX,
                        PillY,
                        PillX + 48.f,
                        PillY + 18.f
                    );

                    ShotHitRects.Add(Hit);
                }

            }
        }



        // ------------------------------------------------------------
        // Shot drag ghost
        // ------------------------------------------------------------
        if (bIsDraggingShot && ShotDragGhostX >= 0.f && ShotDragGhostY >= 0.f)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 50,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(
                        ShotDragGhostX,
                        ShotDragGhostY
                    ),
                    FVector2D(ShotPillWidth, ShotPillHeight)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.25f, 0.6f, 1.f, 0.35f)
            );
        }



        // ------------------------------------------------------------
        // Draw PAGE BREAK marker (derived, visual only) DONE
        // ------------------------------------------------------------

        if (P.bStartsPage)
        {
            const FString PageText =
                FString::Printf(TEXT("PAGE %d"), P.PageNumber);

            const float MarkerY = PaintY;
            const float MarkerX = 10.f; // force "PAGE" to be visible (no clipping)

            //const float MarkerX = ScriptFormat::PageLeft * 0.35f;

            const bool bIsHovered =
                bEditMode &&
                (HoveredPageBreakIndex != INDEX_NONE) &&
                SourceScript &&
                SourceScript->UserPageBreaks.IsValidIndex(HoveredPageBreakIndex) &&
                SourceScript->UserPageBreaks[HoveredPageBreakIndex].AfterBlockId
                == P.BlockId;

            const FLinearColor TextColor =
                bIsHovered
                ? FLinearColor(1.0f, 1.0f, 0.6f, 1.f)
                : FLinearColor(1.0f, 0.85f, 0.2f, 1.f);

            if (bIsHovered)
            {
                const float RuleThickness = 3.0f;

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    LayerId + 0,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(0.f, MarkerY + ScriptFormat::LineHeight * 0.4f),
                        FVector2D(AllottedGeometry.GetLocalSize().X, RuleThickness)
                    ),
                    FCoreStyle::Get().GetBrush("WhiteBrush"),
                    ESlateDrawEffect::None,
                    FLinearColor(1.0f, 0.9f, 0.3f, 0.8f)
                );
            }

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(MarkerX, MarkerY),
                    FVector2D::UnitVector
                ),
                PageText,
                FontInfo,
                ESlateDrawEffect::None,
                TextColor
            );
        }

        // ----------------------------------------------
        // Draw lines at TRUE layout coordinates
        // ----------------------------------------------
        float LineY = PaintY;


        for (const FString& Ln : P.Lines)
        {

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(ScriptFormat::PageLeft + P.IndentLeft, LineY),
                    FVector2D::UnitVector
                ),
                FText::FromString(Ln),
                FontInfo,
                ESlateDrawEffect::None,
                FLinearColor(0.35f, 0.35f, 0.35f, 1.f)
            );

            LineY += ScriptFormat::LineHeight;
        }

        // ------------------------------------------------------------
        // Draw stacked shots for this SceneHeading
        // ------------------------------------------------------------
        if (P.BlockType == EGASBlockType::SceneHeading)
        {
            const TArray<const FGASMarker*>* ShotsForScene =
                SceneShotMap.Find(P.BlockId);

            if (ShotsForScene)
            {
                const float ShotRowHeight = 18.f;
                float ShotY = LineY + 4.f; // below last scene line

                for (const FGASMarker* Shot : *ShotsForScene)
                {

                    ShotY += ShotRowHeight;
                }
            }
        }


    }

    // ------------------------------------------------------------
    // Draw page break drag preview DONE
    // ------------------------------------------------------------
    if ((bIsDraggingPageBreak || bAnimatingPageBreak) && bEditMode)
    {
        const float LineThickness = 2.0f;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 50,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(0.f, DragPreviewY - ScrollOffsetY),
                FVector2D(AllottedGeometry.GetLocalSize().X, LineThickness)
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor(1.0f, 0.85f, 0.2f, 0.9f)
        );
    }

    // ------------------------------------------------------------
    // Shot Add Cursor Icon + LIVE PREVIEW LINE DONE
    // ------------------------------------------------------------
    if (bShotAddArmed)
    {
        const float IconY = ShotGhostY - ScrollOffsetY;

        const FVector2D IconPos(
            10.f,          // left gutter
            IconY - 10.f   // center icon on cursor
        );

        // Camera icon
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 20,
            AllottedGeometry.ToPaintGeometry(
                IconPos,
                FVector2D(20.f, 20.f)
            ),
            FGAS_PreProToolsStyle::Get().GetBrush("GAS.CameraIcon"),
            ESlateDrawEffect::None,
            FLinearColor(0.25f, 0.6f, 1.f, 0.9f)
        );

        // --------------------------------------------------------
        // Live shot line preview (while LMB held) DONE
        // --------------------------------------------------------
        if (bIsShotRangeDragging)
        {
            const float LineX = ShotRangeStartX;

            const float StartY = ShotRangeStartY - ScrollOffsetY;
            const float EndY = ShotGhostY - ScrollOffsetY;

            const float TopY =
                FMath::Min(StartY, EndY);

            const float BottomY =
                FMath::Max(StartY, EndY);


            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 19,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(LineX - 1.f, TopY),
                    FVector2D(2.f, BottomY - TopY)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.25f, 0.6f, 1.f, 0.6f)
            );
        }
    }




    bNeedsRedraw = false;

    return LayerId + 10;
}


int32 SGASScriptPanel::HitTestParagraph(float ScriptY) const
{
    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (ScriptY >= P.TopY && ScriptY < (P.TopY + P.Height))
        {
            return i;
        }
    }

    return INDEX_NONE;
}


int32 SGASScriptPanel::ResolveParagraphForShot(float LocalY) const
{
    // IMPORTANT:
    // LocalY is already in the same coordinate space as RenderedParagraphs TopY
    // (ScrollBox is applying the scroll transform), so DO NOT add ScrollOffsetY here.
    const float ScriptY = LocalY;

    int32 ParaIndex = HitTestParagraph(ScriptY);
    if (ParaIndex != INDEX_NONE)
    {
        return ParaIndex;
    }

    // Clicked between blocks → snap UP to nearest paragraph
    for (int32 i = RenderedParagraphs.Num() - 1; i >= 0; --i)
    {
        if (RenderedParagraphs[i].TopY <= ScriptY)
        {
            return i;
        }
    }

    return INDEX_NONE;
}




void SGASScriptPanel::CommitShotAtParagraph(
    int32 SceneParaIndex,
    int32 InsertIndex,
    float ShotStartScriptY,
    float ShotEndScriptY,
    float CommitX
)
{
    if (!SourceScript)
        return;

    if (!RenderedParagraphs.IsValidIndex(SceneParaIndex))
        return;

    const FRenderedParagraph& ScenePara =
        RenderedParagraphs[SceneParaIndex];

    if (ScenePara.BlockType != EGASBlockType::SceneHeading)
        return;

    const FString& SceneBlockId = ScenePara.BlockId;


    FGASMarker NewMarker;
    NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    NewMarker.MarkerType = EGASMarkerType::ShotMarker;
    NewMarker.BlockId = SceneBlockId;
    NewMarker.ShotOrigin = EGASShotOrigin::User;

    // --------------------------------------------------
    // Resolve FINAL shot number (authoritative)
    // --------------------------------------------------

    // Count existing shots in this scene
    int32 ExistingShotCount = 0;
    for (const FGASMarker& Marker : SourceScript->Markers)
    {
        if (Marker.MarkerType == EGASMarkerType::ShotMarker &&
            Marker.BlockId == SceneBlockId)
        {
            ++ExistingShotCount;
        }
    }


    // Resolve numbering policy (authoritative, non-destructive)
    int32 DisplayShotNumber = 0;

    switch (SourceScript->ShotNumberingPolicy)
    {
    case EGASShotNumberingPolicy::Numeric_10s:
        DisplayShotNumber = (InsertIndex + 1) * 10;
        break;

    case EGASShotNumberingPolicy::Numeric_1s:
        DisplayShotNumber = InsertIndex + 1;
        break;

    default:
        DisplayShotNumber = InsertIndex + 1;
        break;
    }


    // Store FINAL number on marker
    NewMarker.ShotIndex = DisplayShotNumber;


    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[ShotMarker] Created shot %d in scene %s"),
        NewMarker.ShotIndex,
        *SceneBlockId
    );


    // Position (SCRIPT SPACE ONLY)
    NewMarker.ScreenX = CommitX - (ShotPillWidth * 0.5f);

    NewMarker.ScreenY = ShotStartScriptY - ScrollOffsetY;
    NewMarker.ShotLineStartY = ShotStartScriptY - ScrollOffsetY;
    NewMarker.ShotLineEndY = ShotEndScriptY - ScrollOffsetY;



    const int32 ParaIndex = HitTestParagraph(ShotStartScriptY);
    NewMarker.ParagraphIndex = ParaIndex;

    int32 LineIndex = 0;
    if (RenderedParagraphs.IsValidIndex(ParaIndex))
    {
        const FRenderedParagraph& P = RenderedParagraphs[ParaIndex];
        LineIndex = FMath::Clamp(
            FMath::FloorToInt((ShotStartScriptY - P.TopY) / ScriptFormat::LineHeight),
            0,
            FMath::Max(0, P.Lines.Num() - 1)
        );
    }

    NewMarker.LineIndex = LineIndex;

    SourceScript->Markers.Add(NewMarker);

    // ------------------------------------------------------------
    // Auto-create Shot Intent (default)
    // ------------------------------------------------------------
    if (SourceScript)
    {
        FGASShotIntent NewIntent;
        NewIntent.MarkerId = NewMarker.Id;

        // Defaults are already set in struct,
        // but this is explicit and future-safe
        NewIntent.ShotType = EGASShotType::MS;
        NewIntent.Lens = EGASLensIntent::L50;
        NewIntent.CameraBehavior = EGASCameraBehavior::Static;
        NewIntent.Framing = EGASFramingBias::Center;
        NewIntent.SubjectId = TEXT("");

        SourceScript->ShotIntents.Add(NewMarker.Id, NewIntent);
    }

    // ------------------------------------------------------------
    // Open Shot Intent popup AFTER final line is defined
    // ------------------------------------------------------------
    OpenShotIntentPopup(NewMarker.Id);


    OnScriptMutated.ExecuteIfBound();
    bNeedsRedraw = true;
}




FString SGASScriptPanel::GetSourceTextForBlock(const FString& BlockId) const
{
    if (!SourceScript)
        return FString();

    for (const FGASBlock& Block : SourceScript->Blocks)
    {
        if (Block.Id == BlockId)
        {
            return Block.Text;
        }
    }

    return FString();
}


void SGASScriptPanel::SetShowSceneNumbers(bool bInShow)
{
    bShowSceneNumbers = bInShow;

    // Scene numbers are visual-only but depend on layout
    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


void SGASScriptPanel::SetShotMarkers(const TArray<FGASMarker>& Markers)
{
    ShotMarkers = Markers;
    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);

}


// ------------------------------------------------------------
// MOUSE CONTROLS
// ------------------------------------------------------------

FReply SGASScriptPanel::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    const bool bLeftMouse =
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float MouseY = LocalPos.Y;

    // ============================================================
    // SHOT DELETE (RMB on pill) — CONFIRM
    // ============================================================
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        const FString HitShotId = HitTestShot(LocalPos);
        if (!HitShotId.IsEmpty() && SourceScript)
        {
            // Capture info up front
            FString SceneBlockId;
            int32 DeletedShotIndex = INDEX_NONE;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitShotId &&
                    M.MarkerType == EGASMarkerType::ShotMarker)
                {
                    SceneBlockId = M.BlockId;
                    DeletedShotIndex = M.ShotIndex;
                    break;
                }
            }



            TSharedRef<SWindow> ConfirmWindow =
                SNew(SWindow)
                .Title(FText::FromString(TEXT("Delete Shot")))
                .ClientSize(FVector2D(360.f, 140.f))
                .SupportsMinimize(false)
                .SupportsMaximize(false);

            ConfirmWindow->SetContent(
                SNew(SBorder)
                .Padding(12.f)
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 0.f, 0.f, 10.f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Delete this shot marker?")))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Right)
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(4.f)
                                [
                                    SNew(SButton)
                                        .Text(FText::FromString(TEXT("Cancel")))
                                        .OnClicked_Lambda([ConfirmWindow]()
                                            {
                                                ConfirmWindow->RequestDestroyWindow();
                                                return FReply::Handled();
                                            })
                                ]

                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(4.f)
                                [
                                    SNew(SButton)
                                        .Text(FText::FromString(TEXT("Delete")))
                                        .OnClicked_Lambda([this, ConfirmWindow, HitShotId, SceneBlockId, DeletedShotIndex]()
                                            {
                                                // UNDO SNAPSHOT — before mutation
                                                SourceScript->CaptureUndoSnapshot();

                                                // ------------------------------------------------------------
                                                // Remove Shot Marker
                                                // ------------------------------------------------------------
                                                SourceScript->Markers.RemoveAll(
                                                    [&](const FGASMarker& M)
                                                    {
                                                        return M.Id == HitShotId;
                                                    }
                                                );

                                                // ------------------------------------------------------------
                                                // Remove Shot Intent (paired by MarkerId)
                                                // ------------------------------------------------------------
                                                SourceScript->ShotIntents.Remove(HitShotId);

                                                SelectedShotMarkerId.Empty();
                                                HoveredShotMarkerId.Empty();

                                                OnScriptMutated.ExecuteIfBound();
                                                bNeedsRedraw = true;

                                                ConfirmWindow->RequestDestroyWindow();
                                                return FReply::Handled();
                                            })

                                ]
                        ]
                ]
            );

            FSlateApplication::Get().AddModalWindow(
                ConfirmWindow,
                FSlateApplication::Get().GetActiveTopLevelWindow()
            );

            return FReply::Handled();
        }
    }



    // ============================================================
    // SHOT TAIL RESIZE START (ALT + LMB)
    // ============================================================
    if (bLeftMouse && MouseEvent.IsAltDown())
    {
        const FString HitTailId = HitTestShotTail(LocalPos);
        if (!HitTailId.IsEmpty())
        {
            bIsResizingShotTail = true;
            ResizingShotMarkerId = HitTailId;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitTailId)
                {
                    ShotTailGhostY = LocalPos.Y;
                    ShotTailResizeStartY = M.ShotLineStartY;
                    break;
                }
            }

            return FReply::Handled().CaptureMouse(AsShared());
        }
    }


    // ============================================================
    // 1️⃣ SHOT ICON HIT TEST (ALT + LMB DRAG)
    // ============================================================
    if (bLeftMouse && MouseEvent.IsAltDown())

    {
        const FString HitShotId = HitTestShot(LocalPos);

        if (!HitShotId.IsEmpty() && ShotSelectionState)
        {
            const int32 ShotMarkerIndex = ShotMarkers.IndexOfByPredicate(
                [&](const FGASMarker& M)
                {
                    return M.MarkerType == EGASMarkerType::ShotMarker
                        && M.Id == HitShotId;
                }
            );

            if (ShotMarkerIndex != INDEX_NONE)
            {
                const FGASMarker& ShotMarker = ShotMarkers[ShotMarkerIndex];

                const int32 SceneParaIndex =
                    FindOwningSceneParagraphIndex(ShotMarker.LineIndex);

                ShotSelectionState->SelectShot(
                    ShotMarker.BlockId,
                    SceneParaIndex,
                    ShotMarker.Id,
                    ShotMarkerIndex
                );
            }
        }

        if (!HitShotId.IsEmpty())
        {
            bIsDraggingShot = true;
            DraggingShotMarkerId = HitShotId;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitShotId)
                {
                    ShotDragOffset = FVector2D(
                        LocalPos.X - M.ScreenX,
                        LocalPos.Y - (M.ScreenY - ScrollOffsetY)
                    );
                    ShotDragGhostX = M.ScreenX;
                    ShotDragGhostY = LocalPos.Y;
                    break;
                }
            }

            return FReply::Handled().CaptureMouse(AsShared());
        }
    }




    // ------------------------------------------------------------
    // Shot Range Drag START
    // ------------------------------------------------------------
    if (bShotAddArmed &&
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const float LocalY = LocalPos.Y;
        const float ScriptY = LocalPos.Y + ScrollOffsetY;

        const int32 ParaIndex = ResolveParagraphForShot(LocalPos.Y);

        if (ParaIndex != INDEX_NONE)
        {
            bIsShotRangeDragging = true;
            ShotRangeStartParagraph = ParaIndex;
            ShotRangeStartX = LocalPos.X;
            ShotRangeStartY = ScriptY;


            ShotRangeCurrentParagraph = ParaIndex;

            return FReply::Handled().CaptureMouse(AsShared());
        }
    }





    // ============================================================
    // SAFETY
    // ============================================================
    if (!SourceScript || RenderedParagraphs.Num() == 0)
    {
        return FReply::Unhandled();
    }

    // ============================================================
    // 3️⃣ PAGE BREAK DRAG (ALT + LMB)
    // ============================================================
    if (bEditMode && MouseEvent.IsAltDown() && bLeftMouse)
    {
        constexpr float PageBreakHitTolerance = 40.f;

        for (int32 BreakIndex = 0;
            BreakIndex < SourceScript->UserPageBreaks.Num();
            ++BreakIndex)
        {
            const FString& AfterBlockId =
                SourceScript->UserPageBreaks[BreakIndex].AfterBlockId;

            for (const FRenderedParagraph& P : RenderedParagraphs)
            {
                if (!P.bStartsPage)
                    continue;

                const int32 PrevIndex = P.ParagraphIndex - 1;
                if (!RenderedParagraphs.IsValidIndex(PrevIndex))
                    continue;

                if (RenderedParagraphs[PrevIndex].BlockId != AfterBlockId)
                    continue;

                if (FMath::Abs(MouseY - P.TopY) <= PageBreakHitTolerance)
                {
                    bIsDraggingPageBreak = true;
                    DraggedPageBreakIndex = BreakIndex;
                    DragPreviewY = P.TopY;

                    return FReply::Handled()
                        .CaptureMouse(AsShared());
                }
            }
        }

        return FReply::Handled();
    }

    // ============================================================
    // 4️⃣ ADD MODE (BLOCK INSERT)
    // ============================================================
    if (bAddMode && bLeftMouse)
    {
        const int32 InsertAfter = HitTestAddGutter(MouseY);
        if (InsertAfter != INDEX_NONE)
        {
            OpenAddBlockDialog(InsertAfter);
        }
        return FReply::Handled();
    }

    // ============================================================
    // 5️⃣ PARAGRAPH SELECTION + EDIT ROUTING
    // ============================================================
    const float ScriptY = LocalPos.Y;

    const int32 HitParagraphIndex = HitTestParagraph(ScriptY);

    if (HitParagraphIndex == INDEX_NONE)
    {
        return FReply::Handled();
    }

    SelectedParagraphIndex = HitParagraphIndex;
    Invalidate(EInvalidateWidget::Paint);


    // Defer edit popup one tick so selection highlight paints first
    if (bEditMode && bLeftMouse && OnParagraphClicked.IsBound())
    {
        const int32 BlockIndex =
            RenderedParagraphs[HitParagraphIndex].ParagraphIndex;

        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda(
                [this, BlockIndex](float)
                {
                    OnParagraphClicked.Execute(BlockIndex);
                    return false;
                }
            )
        );

        return FReply::Handled();
    }



    return FReply::Handled();
}



FReply SGASScriptPanel::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    // ------------------------------------------------------------
    // Shot tail resize UPDATE
    // ------------------------------------------------------------
    if (bIsResizingShotTail)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        ShotTailGhostY = LocalPos.Y;

        bNeedsRedraw = true;
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // Shot Marker Drag UPDATE
    // ------------------------------------------------------------
    if (bIsDraggingShot)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        ShotDragGhostX = LocalPos.X - ShotDragOffset.X;
        ShotDragGhostY = LocalPos.Y;

        bNeedsRedraw = true;
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // Shot Range Drag UPDATE
    // ------------------------------------------------------------
    if (bIsShotRangeDragging)
    {
        const FVector2D DragLocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float ScriptY = DragLocalPos.Y + ScrollOffsetY;

        // 🔑 KEEP GHOST Y UPDATED FOR LIVE LINE DRAW (SCRIPT SPACE)
        ShotGhostY = ScriptY;

        const int32 ParaIndex = HitTestParagraph(DragLocalPos.Y);

        if (ParaIndex != INDEX_NONE &&
            ParaIndex != ShotRangeCurrentParagraph)
        {
            ShotRangeCurrentParagraph = ParaIndex;
        }

        bNeedsRedraw = true;
        return FReply::Handled();
    }




    // ------------------------------------------------------------
    // Shot Add Hover Preview (works OVER pills + between pills)
    // ------------------------------------------------------------
    if (bShotAddArmed)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y + ScrollOffsetY;

        // Find owning SceneHeading for this Y (even if mouse is in shot stack area)
        const int32 SceneParaIndex = ResolveSceneIndexAtY(LocalPos.Y);

        int32 NewHoverIndex = INDEX_NONE;

        if (RenderedParagraphs.IsValidIndex(SceneParaIndex) &&
            RenderedParagraphs[SceneParaIndex].BlockType == EGASBlockType::SceneHeading)
        {
            NewHoverIndex = SceneParaIndex;
        }

        HoveredShotParagraphIndex = NewHoverIndex;

        // Cursor-follow Y in script space (NO snapping)
        ShotGhostY = ScriptY;
        bNeedsRedraw = true;


        return FReply::Handled();
    }



    
    // ------------------------------------------------------------
    // ADD MODE: no mouse-move behavior yet
    // ------------------------------------------------------------
    if (bAddMode)
    {
        return FReply::Unhandled();
    }

    // ------------------------------------------------------------
    // Safety guards
    // ------------------------------------------------------------
    if (!SourceScript || RenderedParagraphs.Num() == 0)
    {
        return FReply::Unhandled();
    }

    // ------------------------------------------------------------
    // Shot hover (selection preview)
    // ------------------------------------------------------------
    if (!bShotAddArmed)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y;


        const FString NewHoverShotId = HitTestShot(LocalPos);

        if (HoveredShotMarkerId != NewHoverShotId)
        {
            HoveredShotMarkerId = NewHoverShotId;
            bNeedsRedraw = true;
        }

    }

    if (bIsDraggingPageBreak && bEditMode)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        DragPreviewY = LocalPos.Y;

        Invalidate(EInvalidateWidget::Paint);

        return FReply::Handled();
    }



    // ------------------------------------------------------------
    // Existing hover logic (if any)
    // ------------------------------------------------------------
    return FReply::Unhandled();
}


FReply SGASScriptPanel::OnMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    // ------------------------------------------------------------
    // Shot Range Drag END → COMMIT SINGLE SHOT (position-safe)
    // ------------------------------------------------------------
    if (bIsShotRangeDragging)
    {
        bIsShotRangeDragging = false;

        const float EndScriptY =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).Y + ScrollOffsetY;

        // Anchor paragraph selection to START Y (already proven correct visually)
        const int32 ParaIndex = ResolveParagraphForShot(ShotRangeStartY);


        if (ParaIndex != INDEX_NONE)
        {
            const int32 SceneParaIndex =
                FindOwningSceneParagraphIndex(ParaIndex);

            if (SceneParaIndex != INDEX_NONE)
            {
                const int32 InsertIndex =
                    CountShotsForScene(RenderedParagraphs[SceneParaIndex].BlockId);

                CommitShotAtParagraph(
                    SceneParaIndex,
                    InsertIndex,
                    ShotRangeStartY,
                    EndScriptY,
                    ShotRangeStartX
                );

                OnScriptMutated.ExecuteIfBound();
            }
        }


        ShotRangeStartParagraph = INDEX_NONE;
        ShotRangeCurrentParagraph = INDEX_NONE;

        bNeedsRedraw = true;
        return FReply::Handled().ReleaseMouseCapture();
    }
    
    // Convert mouse position to local Y
    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float ReleaseY = LocalPos.Y;

    // ------------------------------------------------------------
    // Shot tail resize COMMIT
    // ------------------------------------------------------------
    if (bIsResizingShotTail)
    {
        //// UNDO SNAPSHOT — before mutation
        SourceScript->CaptureUndoSnapshot();

        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.Id == ResizingShotMarkerId)
            {
                M.ShotLineEndY = ShotTailGhostY;
                break;
            }
        }

        bIsResizingShotTail = false;

        ShotTailResizeStartY = -1.f;
        ShotTailGhostY = -1.f;
        ResizingShotMarkerId.Empty();


        OnScriptMutated.ExecuteIfBound();
        bNeedsRedraw = true;

        return FReply::Handled().ReleaseMouseCapture();

    }




    // ------------------------------------------------------------
    // Shot Marker Drag COMMIT
    // ------------------------------------------------------------
    if (bIsDraggingShot)
    {
        // UNDO SNAPSHOT — before mutation
        SourceScript->CaptureUndoSnapshot();

        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.Id == DraggingShotMarkerId)
            {
                M.ScreenX = ShotDragGhostX;
                M.ScreenY = ShotDragGhostY;
                M.ShotLineStartY = M.ScreenY + ShotPillHeight;



                // update paragraph + line
                const int32 ParaIndex = HitTestParagraph(M.ScreenY);
                M.ParagraphIndex = ParaIndex;

                if (RenderedParagraphs.IsValidIndex(ParaIndex))
                {
                    const FRenderedParagraph& P = RenderedParagraphs[ParaIndex];
                    M.LineIndex = FMath::Clamp(
                        FMath::FloorToInt((M.ScreenY - P.TopY) / ScriptFormat::LineHeight),
                        0,
                        FMath::Max(0, P.Lines.Num() - 1)
                    );
                }
                break;
            }
        }

        bIsDraggingShot = false;
        DraggingShotMarkerId.Empty();
        ShotDragGhostX = ShotDragGhostY = -1.f;

        OnScriptMutated.ExecuteIfBound();
        bNeedsRedraw = true;

        return FReply::Handled().ReleaseMouseCapture();
    }






    // ------------------------------------------------------------
    // PAGE BREAK DRAG COMMIT (ONLY if dragging)
    // ------------------------------------------------------------
    if (bIsDraggingPageBreak)
    {
        bIsDraggingPageBreak = false;

        int32 NewAfterParagraph = INDEX_NONE;

        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            const FRenderedParagraph& P = RenderedParagraphs[i];

            if (P.ParagraphIndex != INDEX_NONE)
            {
                if (ReleaseY < P.TopY)
                {
                    NewAfterParagraph = P.ParagraphIndex - 1;
                    break;
                }
            }
        }

        if (NewAfterParagraph == INDEX_NONE && RenderedParagraphs.Num() > 0)
        {
            NewAfterParagraph = RenderedParagraphs.Last().ParagraphIndex;
        }

        if (SourceScript &&
            SourceScript->UserPageBreaks.IsValidIndex(DraggedPageBreakIndex) &&
            RenderedParagraphs.IsValidIndex(NewAfterParagraph))
        {
            const FString& NewAfterBlockId =
                RenderedParagraphs[NewAfterParagraph].BlockId;

            SourceScript->UserPageBreaks[DraggedPageBreakIndex]
                .AfterBlockId = NewAfterBlockId;
        }

        SnapStartY = DragPreviewY;
        SnapTargetY = DragPreviewY;

        for (const FRenderedParagraph& P : RenderedParagraphs)
        {
            if (P.ParagraphIndex == NewAfterParagraph + 1 && P.bStartsPage)
            {
                SnapTargetY = P.TopY - (ScriptFormat::LineHeight * 2.0f);
                break;
            }
        }

        bAnimatingPageBreak = true;
        SnapAnimAlpha = 0.f;

        DraggedPageBreakIndex = INDEX_NONE;

        RebuildLayout();

        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

FReply SGASScriptPanel::OnMouseWheel(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    if (bSuppressScrollInput)
    {
        return FReply::Handled();
    }

    return FReply::Unhandled();
}


FCursorReply SGASScriptPanel::OnCursorQuery(
    const FGeometry& MyGeometry,
    const FPointerEvent& CursorEvent
) const
{
    // Page break hover takes priority
    if (HoveredPageBreakIndex != INDEX_NONE)
    {
        return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
    }

    // Shot add mode cursor logic
    if (bShotAddArmed && RenderedParagraphs.Num() > 0)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y + ScrollOffsetY;

        const int32 SceneParaIndex = ResolveSceneIndexAtY(LocalPos.Y);

        if (RenderedParagraphs.IsValidIndex(SceneParaIndex))
        {
            const FRenderedParagraph& ScenePara =
                RenderedParagraphs[SceneParaIndex];

            // 🚫 Dead zone directly under Scene Heading → normal cursor
            if (ScriptY <= ScenePara.TopY + ScenePara.Height + ScriptFormat::LineHeight)
            {
                return FCursorReply::Cursor(EMouseCursor::Default);
            }

            // ✅ Valid shot area → camera cursor
            return FCursorReply::Cursor(EMouseCursor::Crosshairs);
        }
    }

    return FCursorReply::Unhandled();
}



void SGASScriptPanel::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{

    if (bPendingScrollAfterLayout &&
        !PendingScrollBlockId.IsEmpty() &&
        CachedTotalHeight > LastKnownViewportHeight)
    {
        bPendingScrollAfterLayout = false;

        int32 TargetPara = INDEX_NONE;

        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            if (RenderedParagraphs[i].BlockId == PendingScrollBlockId)
            {
                TargetPara = i;
                break;
            }
        }

        if (TargetPara != INDEX_NONE)
        {
            const FRenderedParagraph& Para = RenderedParagraphs[TargetPara];

            const float MaxScroll =
                CachedTotalHeight - LastKnownViewportHeight;

            if (OnRequestExternalScroll.IsBound())
            {
                OnRequestExternalScroll.Execute(Para.TopY);
            }




            UE_LOG(
                LogTemp,
                Warning,
                TEXT("SCROLL JUMP FINAL → Para=%d TopY=%.1f Offset=%.1f"),
                TargetPara,
                Para.TopY,
                ScrollOffsetY
            );
        }

        PendingScrollBlockId.Empty();
    }



    // Recompute total height before Slate asks for desired size
    CachedTotalHeight = 0.f;

    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        CachedTotalHeight = FMath::Max(
            CachedTotalHeight,
            P.TopY + P.Height
        );
    }

    Invalidate(EInvalidateWidget::Paint);

    // ------------------------------------------------------------
    // Page break snap animation
    // ------------------------------------------------------------
    if (bAnimatingPageBreak)
    {
        constexpr float SnapSpeed = 8.0f; // higher = snappier

        SnapAnimAlpha = FMath::Min(1.f, SnapAnimAlpha + InDeltaTime * SnapSpeed);

        // Ease-out (feels better than linear)
        const float T = 1.f - FMath::Pow(1.f - SnapAnimAlpha, 3);

        DragPreviewY = FMath::Lerp(SnapStartY, SnapTargetY, T);

        if (SnapAnimAlpha >= 1.f)
        {
            bAnimatingPageBreak = false;
        }

        Invalidate(EInvalidateWidget::Paint);

        if (PendingEditBlockIndex != INDEX_NONE)
        {
            const int32 ParagraphIndexToEdit = PendingEditBlockIndex;
            PendingEditBlockIndex = INDEX_NONE;

            // Exit Add Mode before opening editor
            const bool bWasAddMode = bAddMode;
            bAddMode = false;

            TryEditParagraph(ParagraphIndexToEdit);

            // (Optional) restore if you want
            // bAddMode = bWasAddMode;
        }



    }

}


void SGASScriptPanel::SetExternalScrollOffset(float InOffset)
{
    if (bIgnoreNextExternalScrollOnSetScript)
    {
        bIgnoreNextExternalScrollOnSetScript = false;
        return;
    }

    ScrollOffsetY = FMath::Max(0.f, InOffset);
    PendingScrollParagraph = INDEX_NONE;

    Invalidate(EInvalidateWidget::Paint);

}





// =========================================================
// Shot Selection
// =========================================================

void SGASScriptPanel::EnterShotSelectionMode()
{
    bIsShotSelectMode = true;
    bIsDraggingShot = false;

    ShotStartBlockId.Empty();
    ShotEndBlockId.Empty();

    ShotStartParagraphIndex = INDEX_NONE;
    ShotCurrentParagraphIndex = INDEX_NONE;

    UE_LOG(LogTemp, Warning, TEXT("[SHOT] Shot selection mode ARMED"));
}


int32 SGASScriptPanel::ResolveSceneIndexAtY(float InY) const
{
    // InY may be LOCAL space — convert to SCRIPT space
    const float ScriptY = InY + ScrollOffsetY;

    for (int32 i = RenderedParagraphs.Num() - 1; i >= 0; --i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];
        if (P.BlockType == EGASBlockType::SceneHeading &&
            P.TopY <= ScriptY)
        {
            return i;
        }
    }

    return INDEX_NONE;
}


int32 SGASScriptPanel::CountShotsForScene(const FString& SceneBlockId) const
{
    int32 Count = 0;

    if (!SourceScript)
        return 0;

    for (const FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType == EGASMarkerType::ShotMarker &&
            M.BlockId == SceneBlockId)
        {
            ++Count;
        }
    }

    return Count;
}



void SGASScriptPanel::SetAddShotArmed(bool bArmed)
{
    bShotAddArmed = bArmed;
    HoveredShotParagraphIndex = INDEX_NONE;
    bNeedsRedraw = true;
}

void SGASScriptPanel::SetShotAddArmed(bool bInArmed)
{
    bShotAddArmed = bInArmed;

    if (!bShotAddArmed)
    {
        HoveredShotParagraphIndex = INDEX_NONE;
        HoveredShotMarkerId.Empty();
        SelectedShotMarkerId.Empty();
    }

    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);
}

int32 SGASScriptPanel::FindOwningSceneParagraphIndex(int32 StartParagraphIndex) const
{
    if (!RenderedParagraphs.IsValidIndex(StartParagraphIndex))
        return INDEX_NONE;

    // Walk UP until we hit a scene heading
    for (int32 i = StartParagraphIndex; i >= 0; --i)
    {
        if (RenderedParagraphs[i].BlockType == EGASBlockType::SceneHeading)
        {
            return i;
        }
    }

    return INDEX_NONE;
}



FString SGASScriptPanel::HitTestShot(const FVector2D& LocalPos) const
{
    constexpr float HitPadding = 6.f;

    for (const FShotHitRect& HR : ShotHitRects)
    {
        FSlateRect Expanded =
            FSlateRect(
                HR.Rect.Left - HitPadding,
                HR.Rect.Top - HitPadding,
                HR.Rect.Right + HitPadding,
                HR.Rect.Bottom + HitPadding
            );

        if (Expanded.ContainsPoint(LocalPos))
        {
            return HR.MarkerId;
        }
    }

    return FString();
}

FString SGASScriptPanel::HitTestShotTail(const FVector2D& LocalPos) const
{
    constexpr float TailHitHeight = ScriptFormat::LineHeight;

    constexpr float TailHitWidth = 12.f;

    for (const FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType != EGASMarkerType::ShotMarker)
            continue;

        if (M.ShotLineEndY < 0.f)
            continue;

        const float TailX = M.ScreenX + (ShotPillWidth * 0.5f);
        const float TailY = M.ShotLineEndY;


        const FSlateRect TailRect(
            TailX - TailHitWidth * 0.5f,
            TailY - TailHitHeight * 0.5f,
            TailX + TailHitWidth * 0.5f,
            TailY + TailHitHeight * 0.5f
        );


        if (TailRect.ContainsPoint(LocalPos))
        {
            return M.Id;
        }
    }

    return FString();
}

void SGASScriptPanel::SetEnabledCastNames(
    const TArray<TSharedPtr<FString>>& InNames)
{
    EnabledCastNames = InNames;
    RebuildEnabledCastOptions();
}



void SGASScriptPanel::RebuildEnabledCastOptions()
{
    EnabledCastOptions.Reset();

    for (const TSharedPtr<FString>& Name : EnabledCastNames)
    {
        EnabledCastOptions.Add(Name);
    }

    if (EnabledCastOptions.Num() > 0)
    {
        SelectedShotIntentSubject = EnabledCastOptions[0];
    }
    else
    {
        SelectedShotIntentSubject.Reset();
    }
}


// =============================================================
// Shot Intent Popup (STUB)
// =============================================================
void SGASScriptPanel::OpenShotIntentPopup(const FString& MarkerId)
{

    // -------------------------------------------------
    // Ensure enabled cast options are ready
    // -------------------------------------------------
    if (EnabledCastOptions.Num() == 0)
    {
        RebuildEnabledCastOptions();
    }

    if (!SourceScript)
        return;

    FGASShotIntent* ShotIntent =
        SourceScript->ShotIntents.Find(MarkerId);

    if (!ShotIntent)
        return;

    // -------------------------------------------------
    // Initialize Subject selection from ShotIntent
    // -------------------------------------------------
    SelectedShotIntentSubject.Reset();

    for (const TSharedPtr<FString>& Name : EnabledCastOptions)
    {
        if (Name.IsValid() && *Name == ShotIntent->SubjectId)
        {
            SelectedShotIntentSubject = Name;
            break;
        }
    }


    TSharedRef<SWindow> ShotWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Shot Intent")))
        .ClientSize(FVector2D(420.f, 360.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    ShotWindow->SetOnWindowClosed(
        FOnWindowClosed::CreateLambda(
            [this, MarkerId](const TSharedRef<SWindow>&)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("[ShotListV2] Shot Intent CLOSED for Marker %s"),
                    *MarkerId
                );

                // 1️⃣ Script dirty (you already have this)
                if (OnScriptMutated.IsBound())
                {
                    OnScriptMutated.Execute();
                }

                // 2️⃣ FORCE Shot List V2 rebuild
                if (OnRequestShotListRebuild.IsBound())
                {
                    UE_LOG(LogTemp, Warning, TEXT("[ShotListV2] Rebuild requested after Shot Intent"));
                    OnRequestShotListRebuild.Execute();
                }
            }
        )
    );




    ShotWindow->SetContent(
        SNew(SBorder)
        .Padding(12.f)
        [
            SNew(SVerticalBox)

                // -------------------------------------------------
                // Live Preview  ← STEP 2 GOES HERE
                // -------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 12.f)
                [
                    SNew(SGASShotPreview)
                        .ShotIntent(ShotIntent)
                ]

                // -------------------------------------------------
                // Shot Type
                // -------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 4.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Shot Type")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<EGASShotType>>)
                        .OptionsSource(&ShotTypeOptions)
                        .OnSelectionChanged_Lambda(
                            [ShotIntent](TSharedPtr<EGASShotType> NewValue, ESelectInfo::Type)
                            {
                                if (NewValue.IsValid())
                                {
                                    ShotIntent->ShotType = *NewValue;
                                }
                            }
                        )
                        .OnGenerateWidget_Lambda(
                            [](TSharedPtr<EGASShotType> InItem)
                            {
                                return SNew(STextBlock)
                                    .Text(FText::FromString(
                                        StaticEnum<EGASShotType>()->GetDisplayNameTextByValue(
                                            (int64)*InItem
                                        ).ToString()
                                    ));
                            }
                        )
                        [
                            SNew(STextBlock)
                                .Text_Lambda(
                                    [ShotIntent]()
                                    {
                                        return StaticEnum<EGASShotType>()
                                            ->GetDisplayNameTextByValue(
                                                (int64)ShotIntent->ShotType
                                            );
                                    }
                                )
                        ]
                ]

            // -------------------------------------------------
            // Framing
            // -------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 8.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Framing")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<EGASFramingBias>>)
                        .OptionsSource(&FramingOptions)
                        .OnSelectionChanged_Lambda(
                            [ShotIntent](TSharedPtr<EGASFramingBias> NewValue, ESelectInfo::Type)
                            {
                                if (NewValue.IsValid())
                                {
                                    ShotIntent->Framing = *NewValue;
                                }
                            }
                        )
                        .OnGenerateWidget_Lambda(
                            [](TSharedPtr<EGASFramingBias> InItem)
                            {
                                return SNew(STextBlock)
                                    .Text(StaticEnum<EGASFramingBias>()
                                        ->GetDisplayNameTextByValue((int64)*InItem));
                            }
                        )
                        [
                            SNew(STextBlock)
                                .Text_Lambda(
                                    [ShotIntent]()
                                    {
                                        return StaticEnum<EGASFramingBias>()
                                            ->GetDisplayNameTextByValue((int64)ShotIntent->Framing);
                                    }
                                )
                        ]
                ]

            // -------------------------------------------------
            // Lens
            // -------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 8.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Lens")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<EGASLensIntent>>)
                        .OptionsSource(&LensOptions)
                        .OnSelectionChanged_Lambda(
                            [ShotIntent](TSharedPtr<EGASLensIntent> NewValue, ESelectInfo::Type)
                            {
                                if (NewValue.IsValid())
                                {
                                    ShotIntent->Lens = *NewValue;
                                }
                            }
                        )
                        .OnGenerateWidget_Lambda(
                            [](TSharedPtr<EGASLensIntent> InItem)
                            {
                                return SNew(STextBlock)
                                    .Text(StaticEnum<EGASLensIntent>()
                                        ->GetDisplayNameTextByValue((int64)*InItem));
                            }
                        )
                        [
                            SNew(STextBlock)
                                .Text_Lambda(
                                    [ShotIntent]()
                                    {
                                        return StaticEnum<EGASLensIntent>()
                                            ->GetDisplayNameTextByValue((int64)ShotIntent->Lens);
                                    }
                                )
                        ]
                ]

            // -------------------------------------------------
            // Camera Behavior
            // -------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 8.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Camera Behavior")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<EGASCameraBehavior>>)
                        .OptionsSource(&CameraBehaviorOptions)
                        .OnSelectionChanged_Lambda(
                            [ShotIntent](TSharedPtr<EGASCameraBehavior> NewValue, ESelectInfo::Type)
                            {
                                if (NewValue.IsValid())
                                {
                                    ShotIntent->CameraBehavior = *NewValue;
                                }
                            }
                        )
                        .OnGenerateWidget_Lambda(
                            [](TSharedPtr<EGASCameraBehavior> InItem)
                            {
                                return SNew(STextBlock)
                                    .Text(
                                        StaticEnum<EGASCameraBehavior>()
                                        ->GetDisplayNameTextByValue((int64)*InItem)
                                    );
                            }
                        )
                        [
                            SNew(STextBlock)
                                .Text_Lambda(
                                    [ShotIntent]()
                                    {
                                        return StaticEnum<EGASCameraBehavior>()
                                            ->GetDisplayNameTextByValue((int64)ShotIntent->CameraBehavior);
                                    }
                                )
                        ]
                ]


            // -------------------------------------------------
            // Subject
            // -------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 8.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Subject")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SComboBox<TSharedPtr<FString>>)
                        .OptionsSource(&EnabledCastOptions)
                        .InitiallySelectedItem(SelectedShotIntentSubject)
                        .OnSelectionChanged_Lambda(
                            [this, ShotIntent](TSharedPtr<FString> NewValue, ESelectInfo::Type)
                            {
                                SelectedShotIntentSubject = NewValue;
                                ShotIntent->SubjectId = NewValue.IsValid() ? *NewValue : FString();
                            }
                        )
                        .OnGenerateWidget_Lambda(
                            [](TSharedPtr<FString> InItem)
                            {
                                return SNew(STextBlock)
                                    .Text(FText::FromString(InItem.IsValid() ? *InItem : TEXT("")));
                            }
                        )
                        [
                            SNew(STextBlock)
                                .Text_Lambda(
                                    [this]()
                                    {
                                        return SelectedShotIntentSubject.IsValid()
                                            ? FText::FromString(*SelectedShotIntentSubject)
                                            : FText::FromString(TEXT("Select Character"));
                                    }
                                )
                        ]
                ]

            // -------------------------------------------------
            // Close
            // -------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 12.f, 0.f, 0.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Close")))
                        .OnClicked_Lambda(
                            [this, ShotWindow, ShotIntent]()
                            {
                                ShotIntent->SubjectId = SelectedShotIntentSubject.IsValid()
                                    ? *SelectedShotIntentSubject
                                    : FString();
                                // -------------------------------------------------
                                // Persist Shot Intent → Marker metadata
                                // -------------------------------------------------
                                for (FGASMarker& Marker : SourceScript->Markers)
                                {
                                    if (Marker.Id == ShotIntent->MarkerId)
                                    {
                                        Marker.Metadata.Add(
                                            TEXT("ShotType"),
                                            StaticEnum<EGASShotType>()->GetNameStringByValue(
                                                (int64)ShotIntent->ShotType
                                            )
                                        );

                                        Marker.Metadata.Add(
                                            TEXT("Lens"),
                                            StaticEnum<EGASLensIntent>()->GetNameStringByValue(
                                                (int64)ShotIntent->Lens
                                            )
                                        );

                                        Marker.Metadata.Add(
                                            TEXT("Camera"),
                                            StaticEnum<EGASCameraBehavior>()->GetNameStringByValue(
                                                (int64)ShotIntent->CameraBehavior
                                            )
                                        );

                                        Marker.Metadata.Add(
                                            TEXT("Framing"),
                                            StaticEnum<EGASFramingBias>()->GetNameStringByValue(
                                                (int64)ShotIntent->Framing
                                            )
                                        );

                                        Marker.Metadata.Add(TEXT("Subject"), ShotIntent->SubjectId);
                                        break;
                                    }
                                }



                                ShotWindow->RequestDestroyWindow();
                                return FReply::Handled();
                            }
                        )
                ]

        ]
    );

    FSlateApplication::Get().AddWindow(ShotWindow);
}



