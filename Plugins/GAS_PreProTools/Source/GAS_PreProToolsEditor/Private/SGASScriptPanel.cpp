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
#include "GAS_ShotIntentTypes.h"
#include "ScriptLayoutEngine.h"
#include "GASPreProLog.h"
#include "GAS_StandInActor.h"
#include "SGAS_ShotIntentWindow.h"

#include "Rendering/DrawElements.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SComboBox.h"

#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Containers/Ticker.h"
#include "Misc/Optional.h"

#include "CineCameraActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Editor.h"
#include "CineCameraComponent.h"

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

static ACineCameraActor* GLastCreatedCamera = nullptr;

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

#if WITH_EDITOR

static void ValidateShotMarkerInvariants(
    const TArray<FGASMarker>& Markers,
    float InShotPillHeight
)
{
    for (const FGASMarker& M : Markers)
    {
        if (M.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        ensureMsgf(
            M.ScreenY >= 0.f,
            TEXT("ShotMarker %s has invalid ScreenY (%f)"),
            *M.Id,
            M.ScreenY
        );

        ensureMsgf(
            FMath::IsNearlyEqual(
                M.ShotLineStartY,
                M.ScreenY + InShotPillHeight,
                0.5f
            ),
            TEXT("ShotMarker %s line start drifted: StartY=%f Expected=%f"),
            *M.Id,
            M.ShotLineStartY,
            M.ScreenY + InShotPillHeight
        );

        ensureMsgf(
            M.ShotLineEndY >= M.ShotLineStartY,
            TEXT("ShotMarker %s has inverted line (StartY=%f EndY=%f)"),
            *M.Id,
            M.ShotLineStartY,
            M.ShotLineEndY
        );
    }
}

#endif


// =============================================================
//  Helpers part 1
// =============================================================


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


class SGASShotCastList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGASShotCastList) {}
        SLATE_ARGUMENT(FGASScript*, Script)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Script = InArgs._Script;

        if (Script)
        {
            Script->PostScriptMutation(); // 🔹 FORCE SYNC ON OPEN
        }

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

    void Refresh()
    {
        Rebuild();
    }

    void Rebuild()
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("CAST REBUILD | Script=%p Cast=%d"),
            Script,
            Script ? Script->Cast.Num() : -1
        );

        if (!Script || !ListBox.IsValid())
            return;

        ListBox->ClearChildren();

        for (const FGASCharacterDefinition& Character : Script->Cast)
        {
            ListBox->AddSlot()
                .AutoHeight()
                .Padding(2.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Character.CharacterName))
                ];
        }
    }



private:
    FGASScript* Script = nullptr;
    TSharedPtr<SVerticalBox> ListBox;
};

struct FGASShotIntentWorkingCopy
{
    EGASShotType ShotType;
    EGASFramingBias Framing;
    EGASLensIntent Lens;
    EGASCameraBehavior CameraBehavior;
    FString SubjectId;
};


// =============================================================
// Shot Intent Preview Widget (Lightweight, Procedural)
// Local to SGASScriptPanel (not reusable yet)
// =============================================================
class SGASShotPreview : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGASShotPreview) {}
        SLATE_ARGUMENT(UTextureRenderTarget2D*, RenderTarget)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        RenderTarget = InArgs._RenderTarget;

        const float AspectRatio = 2.39f;
        const float Width = 400.f;
        const float Height = Width / AspectRatio;

        ChildSlot
            [
                SNew(SBorder)
                    .Padding(4)
                    [
                        SNew(SBox)
                            .WidthOverride(Width)
                            .HeightOverride(Height)
                            [
                                SNew(SImage)
                                    .Image(this, &SGASShotPreview::GetPreviewBrush)
                            ]
                    ]
            ];
    }

private:
    mutable FSlateBrush PreviewBrush;

    const FSlateBrush* GetPreviewBrush() const
    {
        if (!RenderTarget || !RenderTarget->IsValidLowLevel())
            return nullptr;

        PreviewBrush.SetResourceObject(RenderTarget);
        PreviewBrush.ImageSize = FVector2D(640.f, 360.f);

        return &PreviewBrush;
    }
    UTextureRenderTarget2D* RenderTarget = nullptr;
};


// =============================================================
//  Construct
// =============================================================
void SGASScriptPanel::Construct(const FArguments& InArgs)
{
    OnParagraphClicked = InArgs._OnParagraphClicked;

    ScriptTab = InArgs._ScriptTab;
    OwnerScriptTab = InArgs._OwnerScriptTab;

    SetCanTick(true);

    ShotSelectionState = NewObject<UGASShotListSelectionState>();

    bNeedsRedraw = true;

    ChildSlot
        [
            SNew(SOverlay)

                // =========================================================
                // Script surface root (existing content is injected here)
                // =========================================================
                +SOverlay::Slot()
                [
                    SAssignNew(ScriptRootBorder, SBorder)
                        .Padding(0)
                        .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                        .Clipping(EWidgetClipping::ClipToBoundsAlways)
                ]

                // =========================================================
                // Shot pill tooltip (lightweight, non-modal)
                // =========================================================
                +SOverlay::Slot()
                [
                    SAssignNew(ShotTooltipWidget, SBox)
                        .Visibility_Lambda([this]()
                            {
                                return HoveredShotMarkerId.IsEmpty()
                                    ? EVisibility::Collapsed
                                    : EVisibility::HitTestInvisible;
                            })
                        .Padding_Lambda([this]()
                            {
                                FVector2D PillPos;

                                if (!GetShotPillLocalPos(HoveredShotMarkerId, PillPos))
                                {
                                    return FMargin(0.f);
                                }

                                return FMargin(
                                    PillPos.X + ShotPillWidth + 6.f,
                                    PillPos.Y - 6.f,
                                    0.f,
                                    0.f
                                );
                            })

                        [
                            SNew(SBorder)
                                .Padding(FMargin(6.f, 2.f))
                                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
                                [
                                    SNew(STextBlock)
                                        .Text(this, &SGASScriptPanel::GetHoveredShotTooltipText)
                                        .Font(FAppStyle::GetFontStyle("SmallFont"))
                                        .ColorAndOpacity(FLinearColor::White)
                                ]
                        ]
                ]
        ];




}

void SGASScriptPanel::RebuildLayout()
{
    if (!SourceScript)
        return;

    // 🔹 EMPTY SCRIPT STATE
    if (SourceScript->Blocks.Num() == 0)
    {
        ScriptRootBorder->SetContent(
            SNew(SBorder)
            .Padding(60.f)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("No Script Loaded\n\nImport a script to begin.")))
                    .Justification(ETextJustify::Center)
                    .Font(FAppStyle::GetFontStyle("NormalFont"))
            ]
        );

        RenderedParagraphs.Empty();
        bNeedsRedraw = true;

        return;
    }

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

    SetRenderedScript(NewRendered);

    // ------------------------------------------------------------
    // Auto-adjust shot tail to paragraph end
    // ------------------------------------------------------------
    for (FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType != EGASMarkerType::ShotMarker)
            continue;

        int32 ValidParaIndex = INDEX_NONE;

        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            if (RenderedParagraphs[i].BlockId == M.BlockId)
            {
                ValidParaIndex = i;
                break;
            }
        }

        if (ValidParaIndex == INDEX_NONE)
            continue;

        const FRenderedParagraph& P = RenderedParagraphs[ValidParaIndex];

        if (M.ShotLineEndY < 0.f)
            continue;

        const float ParagraphEndY =
            P.TopY + P.Height;

        if (M.ShotLineEndY <= ParagraphEndY + ScriptFormat::LineHeight)
        {
            M.ShotLineEndY = ParagraphEndY;
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

    if (!SourceScript || SourceScript->Blocks.Num() == 0)
    {
        ScriptRootBorder->SetContent(
            SNew(SBorder)
            .Padding(60.f)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("No Script Loaded\n\nImport a script to begin.")))
                    .Justification(ETextJustify::Center)
                    .Font(FAppStyle::GetFontStyle("NormalFont"))
            ]
        );

        RenderedParagraphs.Empty();
        bNeedsRedraw = true;
        return;
    }

    RebuildLayout();

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

void SGASScriptPanel::ApplyTextEdit(const FString& BlockId,const FString& NewText)
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
    case EGASBlockType::SceneHeading:
        OpenEditActionDialog(
            BlockIndex,
            FText::FromString(TEXT("Edit Scene Heading"))
        );
        break;

    case EGASBlockType::Action:
        OpenEditActionDialog(
            BlockIndex,
            FText::FromString(TEXT("Edit Action"))
        );
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

void SGASScriptPanel::OpenEditActionDialog(
    int32 BlockIndex,
    const FText& WindowTitle
)
{

    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("OpenEditActionDialog: SourceScript null or invalid index %d"), BlockIndex);
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];


    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(WindowTitle)
        .ClientSize(FVector2D(550.f, 600.f))
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

                                        SourceScript->PostScriptMutation();   // 🔥 THIS WAS MISSING

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

                                        SourceScript->PostScriptMutation();

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

                                            SourceScript->PostScriptMutation();

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
    if (TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin())
    {
        Tab->bShotAddArmed = false;
    }

    bIsShotRangeDragging = false;
    bIsShotSelectMode = false;
    bIsDraggingShot = false;

    // Shot UI state
    ClearPanelShotSelection();
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


// TEMPORARILY DISABLED:
// Will be reintroduced once cursor + hover logic is fully validated.
// Do not delete.

int32 SGASScriptPanel::ResolveParagraphForShot(float ScriptY) const
{
    // Convert LOCAL → SCRIPT for all paragraph hit testing.
    //const float ScriptY = LocalY + ScrollOffsetY;

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

FString FGASMarker::GetShotLabel(EGASShotNumberingPolicy Policy) const
{
    int32 Index = SceneShotIndex;

    switch (Policy)
    {
    case EGASShotNumberingPolicy::Numeric_1s:
        return FString::FromInt(Index + 1);

    case EGASShotNumberingPolicy::Numeric_10s:
        return FString::FromInt((Index + 1) * 10);

    case EGASShotNumberingPolicy::Alphabetic:
    {
        int32 LetterIndex = Index % 26;
        TCHAR Letter = 'A' + LetterIndex;
        return FString::Chr(Letter);
    }

    default:
        return FString::FromInt(Index + 1);
    }
}

static AGAS_StandInActor* FindStandInByCharacter(UWorld* World, const FString& CharacterId)
{
    if (!World || CharacterId.IsEmpty())
        return nullptr;

    for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
    {
        AGAS_StandInActor* Actor = *It;
        if (Actor && Actor->GAS_CharacterId.Equals(CharacterId, ESearchCase::IgnoreCase))
        {
            return Actor;
        }
    }

    return nullptr;
}

void SGASScriptPanel::CommitShotAtParagraph(
    int32 SceneParaIndex,
    int32 InsertIndex,
    float ShotStartLocalY,
    float ShotEndLocalY,
    float CommitX
)
{
    UE_LOG(LogGASPrePro, Warning, TEXT("RenderedParagraphs.Num = %d"), RenderedParagraphs.Num());

    if (!SourceScript)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("CommitShotAtParagraph: SourceScript NULL"));
        return;
    }

    // UNDO SNAPSHOT — before adding shot
    SourceScript->CaptureUndoSnapshot();

    // --------------------------------------------------
    // Incoming values are ALREADY script-space
    // --------------------------------------------------
    const float ShotStartScriptY = ShotStartLocalY;
    const float ShotEndScriptY = ShotEndLocalY;



    // --------------------------------------------------
    // Resolve scene by SHOT START (not mouse hit test)
    // --------------------------------------------------
    int32 SceneParaIndexResolved = INDEX_NONE;

    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (P.BlockType == EGASBlockType::SceneHeading &&
            P.TopY <= ShotStartScriptY)
        {
            SceneParaIndexResolved = i;
        }
    }

    if (SceneParaIndexResolved == INDEX_NONE)
        return;

    const FRenderedParagraph& ScenePara =
        RenderedParagraphs[SceneParaIndexResolved];

    const FString& SceneBlockId = ScenePara.BlockId;

    // --------------------------------------------------
    // TEMP DEBUG: Verify we are checking the right scene block
    // --------------------------------------------------
    FGASBlock* SceneBlockPtr = nullptr;

    for (FGASBlock& B : SourceScript->Blocks)
    {
        if (B.Id == SceneBlockId && B.Type == EGASBlockType::SceneHeading)
        {
            SceneBlockPtr = &B;
            break;
        }
    }


    // --------------------------------------------------
    // Determine Blocking State (Authoritative)
    // --------------------------------------------------
    const bool bHasBlocking =
        (SceneBlockPtr && !SceneBlockPtr->BlockingLevelPath.IsEmpty());


    FGASMarker NewMarker;
    NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString NewMarkerId = NewMarker.Id;
    NewMarker.MarkerType = EGASMarkerType::ShotMarker;
    NewMarker.BlockId = SceneBlockId;
    NewMarker.SceneShotIndex = InsertIndex;
    UE_LOG(LogTemp, Warning, TEXT("NEW MARKER SceneShotIndex: %d"), NewMarker.SceneShotIndex);
    NewMarker.ShotOrigin = EGASShotOrigin::User;

    // --------------------------------------------------
    // Resolve FINAL shot number (authoritative)
    // --------------------------------------------------
    int32 MaxExistingShotNumber = 0;

    for (const FGASMarker& Marker : SourceScript->Markers)
    {
        if (Marker.MarkerType == EGASMarkerType::ShotMarker &&
            Marker.BlockId == SceneBlockId)
        {
            MaxExistingShotNumber =
                FMath::Max(MaxExistingShotNumber, Marker.ShotIndex);
        }
    }

    switch (SourceScript->ShotNumberingPolicy)
    {
    case EGASShotNumberingPolicy::Numeric_10s:
        NewMarker.ShotIndex = MaxExistingShotNumber + 10;
        break;

    case EGASShotNumberingPolicy::Numeric_1s:
        NewMarker.ShotIndex = MaxExistingShotNumber + 1;
        break;

    case EGASShotNumberingPolicy::Alphabetic:
        NewMarker.ShotIndex = MaxExistingShotNumber + 1;
        break;

    default:
        NewMarker.ShotIndex = MaxExistingShotNumber + 1;
        break;
    }

    // Position (SCRIPT SPACE ONLY)
    NewMarker.ScreenX = CommitX - (ShotPillWidth * 0.5f);

    NewMarker.ScreenY = ShotStartScriptY;
    NewMarker.ShotLineStartY = ShotStartScriptY + ShotPillHeight;
    NewMarker.ShotLineEndY = ShotEndScriptY;

    int32 ParaIndex = HitTestParagraph(ShotStartScriptY);

    if (ParaIndex == INDEX_NONE)
    {
        // Find nearest paragraph ABOVE shot start
        for (int32 i = RenderedParagraphs.Num() - 1; i >= 0; --i)
        {
            if (RenderedParagraphs[i].TopY <= ShotStartScriptY)
            {
                ParaIndex = i;
                break;
            }
        }
    }

    if (ParaIndex == INDEX_NONE)
        return;

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

    // --------------------------------------------------
    // Set blocking resolution state (SpatialState authoritative)
    // --------------------------------------------------

    if (!bHasBlocking)
    {
        NewMarker.SetSpatialState(EGASShotSpatialState::NoBlocking);
    }
    else
    {
        NewMarker.SetSpatialState(EGASShotSpatialState::BlockingReady);
    }

    // TEMP TEST: give this shot a camera
    NewMarker.BindCameraTransform(FTransform::Identity);


    // ------------------------------------------------------------
    // Add marker
    // ------------------------------------------------------------
    SourceScript->Markers.Add(NewMarker);

    // ------------------------------------------------------------
    // Auto-create Shot Intent
    // ------------------------------------------------------------
    FGASShotIntent NewIntent;
    NewIntent.MarkerId = NewMarker.Id;
    NewIntent.ShotType = EGASShotType::MS;
    NewIntent.Lens = EGASLensIntent::L50;
    NewIntent.CameraBehavior = EGASCameraBehavior::Static;
    NewIntent.Framing = EGASFramingBias::Center;
    NewIntent.SubjectId = TEXT("BOY");

    SourceScript->ShotIntents.Add(NewMarker.Id, NewIntent);

    FGASShotIntent* CreatedIntent =
        SourceScript->ShotIntents.Find(NewMarker.Id);

    if (CreatedIntent)
    {
        CreatedIntent->CameraName = TEXT("TEMP");
    }

    if (CreatedIntent)
    {
        UWorld* World = nullptr;

#if WITH_EDITOR
        if (GEditor)
        {
            World = GEditor->GetEditorWorldContext().World();
        }
#endif

        if (World && World->WorldType == EWorldType::Editor)
        {
            FActorSpawnParameters Params;
            Params.Name = MakeUniqueObjectName(
                World,
                ACineCameraActor::StaticClass(),
                FName(*FString::Printf(TEXT("ShotCam_%s"), *NewMarker.Id))
            );
            Params.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            ACineCameraActor* Cam =
                World->SpawnActor<ACineCameraActor>(
                    ACineCameraActor::StaticClass(),
                    FTransform::Identity,
                    Params
                );

            GLastCreatedCamera = Cam;

            if (Cam)
            {
                if (!IsValid(Cam))
                {
                    UE_LOG(LogGASPrePro, Error, TEXT("Shot camera invalid right after spawn"));
                    return;
                }

                GLastCreatedCamera = Cam;
                CreatedIntent->CameraActor = Cam;


                CreatedIntent->CameraName = Cam->GetName();

                // -------------------------------------------------
                // Camera Framing (ShotIntent Driven)
                // -------------------------------------------------

                // Resolve Subject
                AGAS_StandInActor* TargetActor = nullptr;

                if (!CreatedIntent->SubjectId.IsEmpty())
                {
                    TargetActor = FindStandInByCharacter(World, CreatedIntent->SubjectId);
                }

                // -------------------------------------------------
                // DEBUG: Actor orientation
                // -------------------------------------------------

                if (TargetActor)
                {
                    UWorld* DebugWorld = TargetActor->GetWorld();
                    const FVector Loc = TargetActor->GetActorLocation();
                }

                USkeletalMeshComponent* MeshComp = TargetActor
                    ? TargetActor->FindComponentByClass<USkeletalMeshComponent>()
                    : nullptr;

                // Target location (mesh if available)
                FVector TargetLocation = FVector::ZeroVector;

                if (MeshComp)
                {
                    TargetLocation = MeshComp->GetComponentLocation();
                    TargetLocation.Z += 140.f; // eye level
                }
                else if (TargetActor)
                {
                    TargetLocation = TargetActor->GetActorLocation();
                }
                else
                {
                    TargetLocation = FVector(0.f, 0.f, 100.f);
                }

                // -------------------------------------------------
                // Camera Solve (mesh forward)
                // -------------------------------------------------
                // -------------------------------------------------
                // Camera Solve (ABSOLUTE WORLD TEST)
                // -------------------------------------------------

                const float Distance = 400.f;

                // -------------------------------------------------
                // Camera Solve (actor-facing forward)
                // -------------------------------------------------

                FVector Forward = TargetActor
                    ? TargetActor->GetActorForwardVector()
                    : FVector::ForwardVector;

                FVector CameraLocation =
                    TargetLocation + (Forward * Distance);

                // DEBUG
                DrawDebugSphere(
                    World,
                    CameraLocation,
                    25.f,
                    12,
                    FColor::Yellow,
                    false,
                    10.f
                );

                // DEBUG
                DrawDebugSphere(
                    World,
                    CameraLocation,
                    25.f,
                    12,
                    FColor::Red,
                    false,
                    10.f
                );

                DrawDebugSphere(
                    World,
                    CameraLocation,
                    25.f,
                    12,
                    FColor::Red,
                    false,
                    10.f
                );

                UE_LOG(LogTemp, Warning, TEXT("Camera: %s"), *CameraLocation.ToString());
                UE_LOG(LogTemp, Warning, TEXT("Target: %s"), *TargetLocation.ToString());

                // Look at actor
                FVector Direction = (TargetLocation - CameraLocation).GetSafeNormal();
                FRotator LookAtRotation = Direction.Rotation();

                // -------------------------------------------------
                // Apply
                // -------------------------------------------------
                Cam->SetActorLocation(CameraLocation);
                Cam->SetActorRotation(LookAtRotation);

                if (OwnerScriptTab.IsValid())
                {
                    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
                    UE_LOG(LogTemp, Warning, TEXT("FINAL CAM LOC BEFORE PREVIEW: %s"),
                        *Cam->GetActorLocation().ToString());
                    // TEMP CRASH ISOLATION
                    // Tab->InitializeCameraPreview(Cam);
                }

#if WITH_EDITOR
                FString ShotLabel = FString::Printf(
                    TEXT("ShotCam_%s"),
                    *NewMarker.GetShotLabel(SourceScript->ShotNumberingPolicy)
                );

                Cam->SetActorLabel(ShotLabel, true);
#endif

                UE_LOG(LogGASPrePro, Warning, TEXT("Shot Camera Assigned: %s"), *Cam->GetName());

                if (OwnerScriptTab.IsValid())
                {
                    // TEMP CRASH ISOLATION
                    // OwnerScriptTab.Pin()->InitializeCameraPreview(Cam);
                }
            }
        }
    }

    // ------------------------------------------------------------
    // Notify + redraw
    // ------------------------------------------------------------
    if (OnShotActivated.IsBound())
    {
        OnShotActivated.Execute(NewMarker.MarkerGuid);
    }

    OnScriptMutated.ExecuteIfBound();
    bNeedsRedraw = true;

    // ------------------------------------------------------------
    // Open popup AFTER Slate finishes this input event
    // ------------------------------------------------------------
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [WeakThis = TWeakPtr<SGASScriptPanel>(SharedThis(this)),
            NewMarkerId,
            SceneBlockId](float)
            {
                if (WeakThis.IsValid())
                {
                    WeakThis.Pin()->OpenShotIntentPopup(NewMarkerId, SceneBlockId, true);
                }
                return false;
            }
        )
    );
}

void SGASScriptPanel::EndAllDrags()
{
    bIsDraggingShot = false;
    bIsResizingShotTail = false;
    bIsDraggingPageBreak = false;

    DraggingShotMarkerId.Empty();
    ResizingShotMarkerId.Empty();

    CachedDraggingShotIndex = INDEX_NONE;

    FSlateApplication::Get().ReleaseMouseCapture();
}

FText SGASScriptPanel::GetHoveredShotTooltipText() const
{
    if (!SourceScript || HoveredShotMarkerId.IsEmpty())
    {
        return FText::GetEmpty();
    }

    const FGASShotIntent* ShotIntent =
        SourceScript->ShotIntents.Find(HoveredShotMarkerId);

    if (!ShotIntent)
    {
        return FText::GetEmpty();
    }

    TArray<FString> Parts;

    // --------------------------------------------------
    // Shot Type (always)
    // --------------------------------------------------
    Parts.Add(
        StaticEnum<EGASShotType>()
        ->GetNameStringByValue((int64)ShotIntent->ShotType)
    );

    // --------------------------------------------------
    // Lens Intent (optional enum)
    // --------------------------------------------------
    if ((int64)ShotIntent->Lens != 0)
    {
        Parts.Add(
            StaticEnum<EGASLensIntent>()
            ->GetNameStringByValue((int64)ShotIntent->Lens)
        );
    }

    // --------------------------------------------------
    // Camera Behavior (optional enum)
    // --------------------------------------------------
    if ((int64)ShotIntent->CameraBehavior != 0)
    {
        Parts.Add(
            StaticEnum<EGASCameraBehavior>()
            ->GetNameStringByValue((int64)ShotIntent->CameraBehavior)
        );
    }

    return FText::FromString(
        FString::Join(Parts, TEXT(" · "))
    );
}


bool SGASScriptPanel::GetShotPillLocalPos(
    const FString& ShotId,
    FVector2D& OutLocalPos
) const
{
    if (!SourceScript || ShotId.IsEmpty())
    {
        return false;
    }

    const FGASMarker* Marker =
        SourceScript->Markers.FindByPredicate(
            [&](const FGASMarker& M)
            {
                return M.Id == ShotId &&
                    M.MarkerType == EGASMarkerType::ShotMarker;
            }
        );

    if (!Marker)
    {
        return false;
    }

    // Marker coords are script-space; convert to panel-local (scroll-safe)
    OutLocalPos = FVector2D(
        Marker->ScreenX,
        Marker->ScreenY - ScrollOffsetY
    );

    return true;
}



// STOPPED CHECKING ALL CODE RIGHT HERE!! /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    // EMPTY SCRIPT STATE
    // ------------------------------------------------------------
    if (SourceScript && SourceScript->Blocks.Num() == 0)
    {


        const FString Message = TEXT("No Script Loaded\n\nImport a script to begin.");

        const FVector2D TextSize =
            FSlateApplication::Get()
            .GetRenderer()
            ->GetFontMeasureService()
            ->Measure(Message, FontInfo);

        const FVector2D CenterPos(
            (AllottedGeometry.GetLocalSize().X - TextSize.X) * 0.5f,
            (AllottedGeometry.GetLocalSize().Y - TextSize.Y) * 0.5f
        );

        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId,
            AllottedGeometry.ToPaintGeometry(
                TextSize,
                FSlateLayoutTransform(CenterPos)
            ),
            FText::FromString(Message),
            FontInfo,
            ESlateDrawEffect::None,
            FLinearColor(0.45f, 0.45f, 0.45f, 1.f)
        );

        return LayerId + 1;
    }

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

                    const FLinearColor ShotLineColor = Shot->Color;


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
                        Shot->GetShotLabel(SourceScript->ShotNumberingPolicy);



                    const FVector2D TextSize =
                        FontMeasureService->Measure(ShotLabel, FontInfo);

                    const float Luminance =
                        (Shot->Color.R * 0.299f) +
                        (Shot->Color.G * 0.587f) +
                        (Shot->Color.B * 0.114f);

                    const FLinearColor TextColor =
                        (Luminance > 0.6f)
                        ? FLinearColor::Black
                        : FLinearColor::White;

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
                        TextColor


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

            const bool bIsProtectedPage =
                (SourceScript &&
                    SourceScript->UserPageBreaks.IsValidIndex(HoveredPageBreakIndex) &&
                    HoveredPageBreakIndex == 0);

            const FLinearColor TextColor =
                bIsProtectedPage
                ? FLinearColor(0.6f, 0.6f, 0.6f, 1.f)   // muted
                : bIsHovered
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

            // ------------------------------------------------------------
            // Protected PAGE tooltip (PAGE 2 cannot be deleted)
            // ------------------------------------------------------------
            if (bIsProtectedPage && bIsHovered)
            {
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId + 2,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(
                            MarkerX,
                            MarkerY + ScriptFormat::LineHeight
                        ),
                        FVector2D::UnitVector
                    ),
                    FText::FromString(TEXT("PAGE 2 cannot be deleted")),
                    FontInfo,
                    ESlateDrawEffect::None,
                    FLinearColor(0.8f, 0.8f, 0.8f, 0.9f)
                );
            }
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
                FVector2D(0.f, DragPreviewY),
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
    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
    const bool bShotModeActive = Tab.IsValid() && Tab->bShotAddArmed;

    if (bShotModeActive && ShotGhostY >= 0.f)
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
        if (bIsShotRangeDragging && ShotGhostY >= 0.f)
        {
            const float LineX = ShotRangeStartX;

            const float StartY = ShotRangeStartY - ScrollOffsetY;
            const float EndY = ShotGhostY;

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

#if WITH_EDITOR
    // Editor-only sanity check — non-mutating
    if (SourceScript)
    {
        ValidateShotMarkerInvariants(
            SourceScript->Markers,
            18.f
        );

    }
#endif

    return LayerId + 10;
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
    UE_LOG(LogGASPrePro, Warning, TEXT("[ShotInput][Down] FIRED"));

    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
    const bool bShotModeActive = Tab.IsValid() && Tab->bShotAddArmed;

    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

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
                                                if (SourceScript)
                                                {
                                                    SourceScript->CaptureUndoSnapshot();
                                                }

                                                if (TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin())
                                                {
                                                    Tab->RequestDeleteShotMarker(HitShotId);
                                                }

                                                ClearPanelShotSelection();

                                                if (OnScriptMutated.IsBound())
                                                {
                                                    OnScriptMutated.Execute();
                                                }

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

    // ------------------------------------------------------------
    // Right-click: Change Block Type (Edit Mode only)
    // ------------------------------------------------------------
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (!bEditMode || !SourceScript)
        {
            return FReply::Unhandled();
        }

        const float ScriptY = LocalPos.Y;
        const int32 ParagraphIndex = HitTestParagraph(ScriptY);

        if (ParagraphIndex != INDEX_NONE)
        {
            OpenChangeBlockTypeMenu(ParagraphIndex, MyGeometry, MouseEvent);
            return FReply::Handled();
        }

        return FReply::Unhandled();
    }


    // ------------------------------------------------------------
    // Guard: do not start a new interaction while dragging
    // ------------------------------------------------------------
    if (bIsDraggingShot || bIsResizingShotTail || bIsDraggingPageBreak ||
        bIsShotRangeDragging)
    {
        return FReply::Handled();
    }

    // ------------------------------------------------------------
    // ALT + LMB is reserved for shot drag — do NOT process here
    // ------------------------------------------------------------

    const bool bLeftMouse =
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

    //const FVector2D LocalPos =
    //    MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float MouseY = LocalPos.Y;


    // ============================================================
    // PAGE BREAK DRAG (ALT + LMB)
    // ============================================================
    if (bEditMode && MouseEvent.IsAltDown() && bLeftMouse && SourceScript)
    {
        constexpr float PageBreakHitTolerance = 40.f;

        const float MouseDocY = LocalPos.Y; // your layout appears already doc space

        for (int32 BreakIndex = 0; BreakIndex < SourceScript->UserPageBreaks.Num(); ++BreakIndex)
        {
            const FString& AfterBlockId = SourceScript->UserPageBreaks[BreakIndex].AfterBlockId;

            for (const FRenderedParagraph& P : RenderedParagraphs)
            {
                if (!P.bStartsPage)
                    continue;

                const int32 PrevIndex = P.ParagraphIndex - 1;
                if (!RenderedParagraphs.IsValidIndex(PrevIndex))
                    continue;

                if (RenderedParagraphs[PrevIndex].BlockId != AfterBlockId)
                    continue;

                if (FMath::Abs(MouseDocY - P.TopY) <= PageBreakHitTolerance)
                {
                    bIsDraggingPageBreak = true;
                    DraggedPageBreakIndex = BreakIndex;
                    DragPreviewY = P.TopY;

                    return FReply::Handled().CaptureMouse(AsShared());
                }
            }
        }

        return FReply::Handled();
    }


    // ============================================================
    // 1️⃣ SHOT ICON HIT TEST (ALT + LMB DRAG)
    // ============================================================
    if (bLeftMouse && MouseEvent.IsAltDown())

    {
        const FString HitShotId = HitTestShot(LocalPos);

        if (!HitShotId.IsEmpty())
        {
            bIsDraggingShot = true;
            DraggingShotMarkerId = HitShotId;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitShotId)
                {
                    CachedDraggingShotIndex = M.ShotIndex;
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


    // ============================================================
    // SHOT TAIL RESIZE START (ALT + LMB)
    // ============================================================
    if (bLeftMouse && MouseEvent.IsAltDown())
    {
        const FString HitTailId = HitTestShotTail(LocalPos);
        if (!HitTailId.IsEmpty())
        {
            const FGASMarker* Marker =
                SourceScript->Markers.FindByPredicate(
                    [&](const FGASMarker& M)
                    {
                        return M.Id == HitTailId;
                    }
                );

            // Only allow tail resize if cursor is BELOW the pill
            if (Marker && LocalPos.Y > Marker->ScreenY + ShotPillHeight)
            {
                bIsResizingShotTail = true;
                ResizingShotMarkerId = HitTailId;

                ShotTailGhostY = LocalPos.Y;
                ShotTailResizeStartY = Marker->ShotLineStartY;

                return FReply::Handled().CaptureMouse(AsShared());
            }
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
    // SHOT RANGE DRAG START (NORMAL LMB)
    // ============================================================
    if (!Tab.IsValid())
    {
        return FReply::Unhandled();
    }

    if (bShotModeActive &&
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const float ScriptY = LocalPos.Y + ScrollOffsetY;
        const int32 ParaIndex = ResolveParagraphForShot(ScriptY);

        if (ParaIndex == INDEX_NONE)
        {
            return FReply::Handled();
        }

        if (bShotIntentPopupOpen)
        {
            return FReply::Handled();
        }

        bIsShotRangeDragging = true;
        ShotRangeStartParagraph = ParaIndex;
        ShotRangeCurrentParagraph = ParaIndex;

        ShotRangeStartY = ScriptY;
        ShotRangeStartLocalY = LocalPos.Y;
        ShotGhostY = ScriptY;
        ShotRangeStartX = LocalPos.X;

        UE_LOG(LogGASPrePro, Warning,
            TEXT("[ShotInput][DragStart] Para=%d Y=%.2f"),
            ParaIndex,
            ScriptY);

        return FReply::Handled().CaptureMouse(AsShared());
    }

    // ============================================================
    //  PAGE BREAK ADD (SHIFT + LMB)
    // ============================================================
    if (bEditMode &&
        MouseEvent.IsShiftDown() &&
        bLeftMouse &&
        SourceScript)
    {
        // Resolve document-space Y
        const float MouseDocY = LocalPos.Y;

        int32 TargetRenderIndex = INDEX_NONE;

        // Find first paragraph BELOW the click
        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            if (MouseDocY < RenderedParagraphs[i].TopY)
            {
                TargetRenderIndex = i;
                break;
            }
        }

        // Clamp to end
        if (TargetRenderIndex == INDEX_NONE && RenderedParagraphs.Num() > 0)
        {
            TargetRenderIndex = RenderedParagraphs.Num() - 1;
        }

        // Must be AFTER a real paragraph
        if (RenderedParagraphs.IsValidIndex(TargetRenderIndex) &&
            TargetRenderIndex > 0)
        {
            const int32 AfterRenderIndex = TargetRenderIndex - 1;

            const FString& AfterBlockId =
                RenderedParagraphs[AfterRenderIndex].BlockId;

            // Prevent duplicates
            for (const FGASUserPageBreak& PB : SourceScript->UserPageBreaks)
            {
                if (PB.AfterBlockId == AfterBlockId)
                {
                    return FReply::Handled();
                }
            }

            // UNDO SNAPSHOT
            SourceScript->CaptureUndoSnapshot();

            FGASUserPageBreak NewBreak;
            NewBreak.AfterBlockId = AfterBlockId;

            SourceScript->UserPageBreaks.Add(NewBreak);

            OnScriptMutated.ExecuteIfBound();
            RebuildLayout();

            // HARD RESET interaction state
            bIsDraggingPageBreak = false;
            bPageBreakDidDrag = false;
            PendingDeletePageBreakIndex = INDEX_NONE;
            DragPreviewY = -1.f;

            return FReply::Handled();
        }

        return FReply::Handled();
    }

    // ============================================================
    // ADD MODE (BLOCK INSERT)
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
    // PARAGRAPH SELECTION + EDIT ROUTING
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

FReply SGASScriptPanel::OnMouseButtonDoubleClick(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton ||
        MouseEvent.IsAltDown())
    {
        return FReply::Unhandled();
    }

    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const FString HitShotId = HitTestShot(LocalPos);
    if (HitShotId.IsEmpty() || !SourceScript)
    {
        return FReply::Unhandled();
    }

    const FGASMarker* Marker =
        SourceScript->Markers.FindByPredicate(
            [&](const FGASMarker& M)
            {
                return M.Id == HitShotId &&
                    M.MarkerType == EGASMarkerType::ShotMarker;
            }
        );

    if (!Marker)
    {
        return FReply::Unhandled();
    }

    OpenShotIntentPopup(Marker->Id, Marker->BlockId, false);
    return FReply::Handled();
}


FReply SGASScriptPanel::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{

    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
    const bool bShotModeActive = Tab.IsValid() && Tab->bShotAddArmed;

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

        UE_LOG(LogTemp, Warning,
            TEXT("[VERIFY MOVE] ShotMode=%d Dragging=%d Capture=%d"),
            bShotModeActive ? 1 : 0,
            bIsShotRangeDragging ? 1 : 0,
            HasMouseCapture() ? 1 : 0);

        bNeedsRedraw = true;
        return FReply::Handled();
    }

    // ------------------------------------------------------------
    // 🔥 KILL STALE DRAG (ONLY OUTSIDE SHOT MODE)
    // ------------------------------------------------------------

    if (!bShotModeActive && bIsShotRangeDragging && !HasMouseCapture())
    {
        bIsShotRangeDragging = false;

        ShotRangeStartParagraph = INDEX_NONE;
        ShotRangeCurrentParagraph = INDEX_NONE;

        UE_LOG(LogGASPrePro, Warning,
            TEXT("[ShotInput][Move] STALE DRAG KILL"));

        ShotGhostY = -1.f;
        ShotRangeStartLocalY = -1.f;

        bNeedsRedraw = true;

        return FReply::Handled();
    }

    // ------------------------------------------------------------
    // Shot Range Drag UPDATE
    // ------------------------------------------------------------
    if (bIsShotRangeDragging && HasMouseCapture())
    {
        const FVector2D DragLocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        // 🔑 Ghost Y is LOCAL space (authoritative for commit)
        ShotGhostY = DragLocalPos.Y;

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
    if (bShotModeActive && !bIsShotRangeDragging && bAllowShotHoverPreview)
    {

        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        // Find owning SceneHeading for this Y (even if mouse is in shot stack area)
        const float ScriptY = LocalPos.Y + ScrollOffsetY;
        const int32 SceneParaIndex = ResolveSceneIndexAtY(ScriptY);


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

    if (!bShotModeActive)
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


        // ------------------------------------------------------------
        // Shot pill tooltip window (explicit, pill-anchored)
        // ------------------------------------------------------------

        if (bShotIntentPopupOpen || bShotModeActive || bIsDraggingShot || bIsResizingShotTail || bIsShotRangeDragging)
        {
            // Hard suppress tooltip during other interactions
            ShotHoverTooltipShotId.Empty();

            if (ShotHoverTooltipWindow.IsValid())
            {
                ShotHoverTooltipWindow->RequestDestroyWindow();
                ShotHoverTooltipWindow.Reset();
            }

            return FReply::Handled();
        }

        if (HoveredShotMarkerId.IsEmpty())
        {
            ShotHoverTooltipShotId.Empty();

            if (ShotHoverTooltipWindow.IsValid())
            {
                ShotHoverTooltipWindow->RequestDestroyWindow();
                ShotHoverTooltipWindow.Reset();
            }

            return FReply::Handled();
        }

        // Only update when hover target changes
        if (ShotHoverTooltipShotId != HoveredShotMarkerId)
        {
            ShotHoverTooltipShotId = HoveredShotMarkerId;

            if (ShotHoverTooltipWindow.IsValid())
            {
                ShotHoverTooltipWindow->RequestDestroyWindow();
                ShotHoverTooltipWindow.Reset();
            }

            ShotHoverTooltipWindow =
                SNew(SWindow)
                .SizingRule(ESizingRule::Autosized)
                .SupportsMaximize(false)
                .SupportsMinimize(false)
                .HasCloseButton(false)
                .IsTopmostWindow(true)
                .FocusWhenFirstShown(false);

            ShotHoverTooltipWindow->SetContent(
                SNew(SBorder)
                .Padding(FMargin(6.f, 2.f))
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.85f))
                [
                    SNew(STextBlock)
                        .Text(this, &SGASScriptPanel::GetHoveredShotTooltipText)
                        .Font(FAppStyle::GetFontStyle("SmallFont"))
                        .ColorAndOpacity(FLinearColor::White)
                ]
            );

            FSlateApplication::Get().AddWindow(ShotHoverTooltipWindow.ToSharedRef(), /*bShowImmediately=*/true);
        }

        // Reposition every move while hovering so it stays pill-anchored during scroll/drag
        if (ShotHoverTooltipWindow.IsValid())
        {
            FVector2D PillLocal;
            if (GetShotPillLocalPos(ShotHoverTooltipShotId, PillLocal))
            {
                // Anchor to pill: right side, slight up
                const FVector2D TooltipLocalPos(
                    PillLocal.X + ShotPillWidth + 8.f,
                    PillLocal.Y - 6.f
                );

                const FVector2D TooltipScreenPos =
                    MyGeometry.LocalToAbsolute(TooltipLocalPos);

                ShotHoverTooltipWindow->MoveWindowTo(TooltipScreenPos);
            }
        }


    }

    if (bIsDraggingPageBreak && bEditMode)
    {

        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float MouseDocY = LocalPos.Y;

        // ------------------------------------------------------------
        // Cancel DELETE intent if user actually starts dragging
        // ------------------------------------------------------------
        if (PendingDeletePageBreakIndex != INDEX_NONE)
        {
            if (FMath::Abs(LocalPos.Y - PageBreakClickStartY) > 4.f)
            {
                PendingDeletePageBreakIndex = INDEX_NONE;
            }
        }


        // ------------------------------------------------------------
        // SNAP to nearest paragraph by INDEX (scroll-safe)
        // ------------------------------------------------------------
        int32 BestRenderIndex = 0; // default to first valid paragraph
        float BestDist = TNumericLimits<float>::Max();

        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            const FRenderedParagraph& P = RenderedParagraphs[i];

            const float Dist = FMath::Abs(MouseDocY - P.TopY);
            if (Dist < BestDist)
            {
                BestDist = Dist;
                BestRenderIndex = i;
            }
        }

        if (BestRenderIndex != INDEX_NONE)
        {
            DraggedPageBreakTargetRenderIndex = BestRenderIndex;
            DragPreviewY = RenderedParagraphs[BestRenderIndex].TopY;
        }

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
    UE_LOG(LogGASPrePro, Warning, TEXT("OnMouseButtonUp fired"));

    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
    const bool bValidTab = Tab.IsValid();

    // ------------------------------------------------------------
    // PAGE BREAK DELETE (ALT + CLICK, NO DRAG)
    // ------------------------------------------------------------
    if (bEditMode &&
        MouseEvent.IsAltDown() &&
        PendingDeletePageBreakIndex != INDEX_NONE &&
        !bPageBreakDidDrag &&
        SourceScript &&
        SourceScript->UserPageBreaks.IsValidIndex(PendingDeletePageBreakIndex))
    {
        // Do NOT allow deleting PAGE 2
        if (PendingDeletePageBreakIndex > 0)
        {
            SourceScript->UserPageBreaks.RemoveAt(PendingDeletePageBreakIndex);
            OnScriptMutated.ExecuteIfBound();
            RebuildLayout();
        }

        // ------------------------------------------------------------
        // HARD TERMINATE page-break interaction (DELETE path)
        // ------------------------------------------------------------
        PendingDeletePageBreakIndex = INDEX_NONE;
        bPageBreakDidDrag = false;

        bIsDraggingPageBreak = false;
        bAnimatingPageBreak = false;

        DraggedPageBreakIndex = INDEX_NONE;
        DragPreviewY = -1.f;
        SnapAnimAlpha = 0.f;


        return FReply::Handled().ReleaseMouseCapture();
    }




    // ------------------------------------------------------------
    // Shot Range Drag END → COMMIT SINGLE SHOT (position-safe)
    // ------------------------------------------------------------
    if (bIsShotRangeDragging)
    {
        bIsShotRangeDragging = false;

        const float EndY = ShotGhostY;
        const int32 ParaIndex = ShotRangeStartParagraph;

        bool bShouldCommit = true;

        FString SceneId;
        int32 SceneParaIndex = INDEX_NONE;

        // --------------------------------------------------
        // VALIDATE PARAGRAPH
        // --------------------------------------------------
        if (ParaIndex == INDEX_NONE)
        {
            bShouldCommit = false;
        }

        // --------------------------------------------------
        // RESOLVE SCENE
        // --------------------------------------------------
        if (bShouldCommit)
        {
            SceneParaIndex = FindOwningSceneParagraphIndex(ParaIndex);

            if (SceneParaIndex == INDEX_NONE)
            {
                UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMarker] No scene found (DRAG)"));
                bShouldCommit = false;
            }
            else
            {
                SceneId = RenderedParagraphs[SceneParaIndex].BlockId;
            }
        }

        // --------------------------------------------------
        // SET ACTIVE SCENE
        // --------------------------------------------------
        if (bShouldCommit && bValidTab && Tab->ActiveCameraModeSceneId.IsEmpty())
        {
            Tab->ActiveCameraModeSceneId = SceneId;
        }

        // --------------------------------------------------
        // SCENE GUARD
        // --------------------------------------------------
        if (bShouldCommit && bValidTab && SceneId != Tab->ActiveCameraModeSceneId)
        {
            UE_LOG(LogGASPrePro, Warning,
                TEXT("[ShotMarker] Different scene clicked (DRAG)"));

            bShouldCommit = false;
        }

        // --------------------------------------------------
        // COMMIT
        // --------------------------------------------------
        if (bShouldCommit)
        {
            const int32 InsertIndex = CountShotsForScene(SceneId);

            CommitShotAtParagraph(
                SceneParaIndex,
                InsertIndex,
                ShotRangeStartLocalY,
                EndY,
                ShotRangeStartX
            );

            OnScriptMutated.ExecuteIfBound();
        }

        // --------------------------------------------------
        // CLEANUP (ALWAYS RUNS)
        // --------------------------------------------------
        ShotRangeStartParagraph = INDEX_NONE;
        ShotRangeCurrentParagraph = INDEX_NONE;

        ShotGhostY = -1.f;
        ShotRangeStartLocalY = -1.f;

        bNeedsRedraw = true;

        return FReply::Handled().ReleaseMouseCapture();
    }
    
    // Convert mouse position to local Y
    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float ReleaseDocY = LocalPos.Y + ScrollOffsetY;

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
        // ⭐️ FORCE shot list eighths to update
        if (OnRequestShotListRebuild.IsBound())
        {
            OnRequestShotListRebuild.Execute();
        }

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

        FString NewSceneBlockId;

        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.Id == DraggingShotMarkerId)
            {
                M.ScreenX = ShotDragGhostX;
                M.ScreenY = ShotDragGhostY;
                M.ShotLineStartY = M.ScreenY + ShotPillHeight;

                // update paragraph + line
                const int32 ParaIndex = HitTestParagraph(M.ScreenY);

                if (RenderedParagraphs.IsValidIndex(ParaIndex))
                {
                    const FRenderedParagraph& P = RenderedParagraphs[ParaIndex];
                    M.LineIndex = FMath::Clamp(
                        FMath::FloorToInt((M.ScreenY - P.TopY) / ScriptFormat::LineHeight),
                        0,
                        FMath::Max(0, P.Lines.Num() - 1)
                    );
                }

                // ✅ Update owning Scene BlockId (authoritative for shot list grouping)
                int32 SceneParaIndex = FindOwningSceneParagraphIndex(ParaIndex);

                // Fallback: if still invalid, walk upward to find a SceneHeading
                if (SceneParaIndex == INDEX_NONE)
                {
                    for (int32 i = ParaIndex; i >= 0; --i)
                    {
                        if (RenderedParagraphs[i].BlockType == EGASBlockType::SceneHeading)
                        {
                            SceneParaIndex = i;
                            break;
                        }
                    }
                }

                if (RenderedParagraphs.IsValidIndex(SceneParaIndex))
                {
                    NewSceneBlockId = RenderedParagraphs[SceneParaIndex].BlockId;
                    M.BlockId = NewSceneBlockId;
                }

                break;
            }
        }

        bIsDraggingShot = false;
        DraggingShotMarkerId.Empty();
        ShotDragGhostX = ShotDragGhostY = -1.f;

        OnScriptMutated.ExecuteIfBound();
        if (OnRequestShotListRebuild.IsBound())
        {
            OnRequestShotListRebuild.Execute();
        }

        bNeedsRedraw = true;

        return FReply::Handled().ReleaseMouseCapture();
    }

    // ------------------------------------------------------------
    // PAGE BREAK DRAG COMMIT (ONLY if dragging)
    // ------------------------------------------------------------
    if (bIsDraggingPageBreak)
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("PB COMMIT CHECK | DragIdx=%d"),
            DraggedPageBreakIndex);

        int32 TargetRenderIndex = INDEX_NONE;

        float BestDist = TNumericLimits<float>::Max();

        for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
        {
            const float Dist = FMath::Abs(ReleaseDocY - RenderedParagraphs[i].TopY);

            if (Dist < BestDist)
            {
                BestDist = Dist;
                TargetRenderIndex = i;
            }
        }
        // ------------------------------------------------------------
        // Clamp PAGE BREAK ordering (prevent crossing neighbors)
        // ------------------------------------------------------------
        int32 MaxRenderIndex = RenderedParagraphs.Num() - 1;

        if (SourceScript &&
            SourceScript->UserPageBreaks.IsValidIndex(DraggedPageBreakIndex + 1))
        {


            const FString& NextAfterBlockId =
                SourceScript->UserPageBreaks[DraggedPageBreakIndex + 1].AfterBlockId;

            for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
            {
                if (RenderedParagraphs[i].BlockId == NextAfterBlockId)
                {
                    MaxRenderIndex = i;
                    break;
                }
            }
        }

        // Clamp target
        if (TargetRenderIndex >= MaxRenderIndex)
        {
            TargetRenderIndex = MaxRenderIndex;
        }


        // Commit: page break is "after" the previous rendered paragraph
        if (SourceScript &&
            SourceScript->UserPageBreaks.IsValidIndex(DraggedPageBreakIndex) &&
            TargetRenderIndex != INDEX_NONE &&
            TargetRenderIndex > 0)
        {
            const int32 AfterRenderIndex = TargetRenderIndex - 1;

            const FString& NewAfterBlockId =
                RenderedParagraphs[AfterRenderIndex].BlockId;

            SourceScript->UserPageBreaks[DraggedPageBreakIndex]
                .AfterBlockId = NewAfterBlockId;
        }

        // ------------------------------------------------------------
        // Animate settle (unchanged behavior)
        // ------------------------------------------------------------
        SnapStartY = DragPreviewY;
        SnapTargetY = DragPreviewY;

        bAnimatingPageBreak = true;
        SnapAnimAlpha = 0.f;

        // ------------------------------------------------------------
        // FINALIZE page-break drag state (ONLY HERE)
        // ------------------------------------------------------------
        bIsDraggingPageBreak = false;
        DraggedPageBreakIndex = INDEX_NONE;
        bPageBreakDidDrag = false;
        PendingDeletePageBreakIndex = INDEX_NONE;
        DragPreviewY = -1.f;

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
    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
    const bool bShotModeActive = Tab.IsValid() && Tab->bShotAddArmed;

    if (bShotModeActive && RenderedParagraphs.Num() > 0)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y;

        const int32 SceneParaIndex = ResolveSceneIndexAtY(ScriptY);


        if (RenderedParagraphs.IsValidIndex(SceneParaIndex))
        {
            const FRenderedParagraph& ScenePara =
                RenderedParagraphs[SceneParaIndex];

            // Dead zone = entire SceneHeading paragraph block
            const int32 SceneIndex = SceneParaIndex;
            const int32 NextParaIndex = SceneIndex + 1;

            if (RenderedParagraphs.IsValidIndex(NextParaIndex))
            {
                const float SceneVisualEndY =
                    RenderedParagraphs[NextParaIndex].TopY;

                if (ScriptY < SceneVisualEndY)
                {
                    return FCursorReply::Cursor(EMouseCursor::Default);
                }
            }
            else
            {
                // Last paragraph in script — disallow shots
                return FCursorReply::Cursor(EMouseCursor::Default);
            }


            // ✅ Valid shot area → camera cursor
            return FCursorReply::Cursor(EMouseCursor::Crosshairs);
        }
    }

    return FCursorReply::Unhandled();
}




bool SGASScriptPanel::CanChangeBlockType(int32 ParagraphIndex, EGASBlockType NewType) const
{
    if (!SourceScript) return false;
    if (!SourceScript->Blocks.IsValidIndex(ParagraphIndex)) return false;

    const EGASBlockType CurrentType = SourceScript->Blocks[ParagraphIndex].Type;

    // No-op change is allowed to be disabled in menu
    if (CurrentType == NewType)
    {
        return false;
    }

    // Guard: cannot convert the FIRST Scene Heading to a non-SceneHeading
    int32 FirstSceneHeadingIndex = INDEX_NONE;
    for (int32 i = 0; i < SourceScript->Blocks.Num(); ++i)
    {
        if (SourceScript->Blocks[i].Type == EGASBlockType::SceneHeading)
        {
            FirstSceneHeadingIndex = i;
            break;
        }
    }

    if (FirstSceneHeadingIndex != INDEX_NONE &&
        ParagraphIndex == FirstSceneHeadingIndex &&
        NewType != EGASBlockType::SceneHeading)
    {
        return false;
    }

    return true;
}

void SGASScriptPanel::ApplyBlockTypeChange(int32 ParagraphIndex, EGASBlockType NewType)
{
    if (!SourceScript) return;
    if (!SourceScript->Blocks.IsValidIndex(ParagraphIndex)) return;

    if (!CanChangeBlockType(ParagraphIndex, NewType))
    {
        return; // refuse (tooltip handled by disabled menu entries)
    }

    SourceScript->Blocks[ParagraphIndex].Type = NewType;

    RebuildLayout();
    OnScriptMutated.ExecuteIfBound();
}

void SGASScriptPanel::OpenChangeBlockTypeMenu(
    int32 ParagraphIndex,
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    if (!SourceScript) return;
    if (!SourceScript->Blocks.IsValidIndex(ParagraphIndex)) return;

    const auto MakeAction = [&](EGASBlockType NewType)
        {
            const bool bCan = CanChangeBlockType(ParagraphIndex, NewType);

            FUIAction Action;
            Action.ExecuteAction = FExecuteAction::CreateSP(
                this,
                &SGASScriptPanel::ApplyBlockTypeChange,
                ParagraphIndex,
                NewType
            );
            Action.CanExecuteAction = FCanExecuteAction::CreateLambda([bCan]() { return bCan; });

            return Action;
        };

    const auto MakeTooltip = [&](EGASBlockType NewType) -> FText
        {
            // If blocked by first-scene rule, show required tooltip
            // (We detect it the same way CanChangeBlockType does.)
            int32 FirstSceneHeadingIndex = INDEX_NONE;
            for (int32 i = 0; i < SourceScript->Blocks.Num(); ++i)
            {
                if (SourceScript->Blocks[i].Type == EGASBlockType::SceneHeading)
                {
                    FirstSceneHeadingIndex = i;
                    break;
                }
            }

            if (FirstSceneHeadingIndex != INDEX_NONE &&
                ParagraphIndex == FirstSceneHeadingIndex &&
                NewType != EGASBlockType::SceneHeading)
            {
                return FText::FromString(TEXT("At least one Scene Heading is required"));
            }

            return FText::GetEmpty();
        };

    FMenuBuilder MenuBuilder(true, nullptr);

    MenuBuilder.BeginSection(NAME_None, FText::FromString(TEXT("Change Block Type")));
    {
        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Scene Heading")),
            MakeTooltip(EGASBlockType::SceneHeading),
            FSlateIcon(),
            MakeAction(EGASBlockType::SceneHeading)
        );

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Action")),
            MakeTooltip(EGASBlockType::Action),
            FSlateIcon(),
            MakeAction(EGASBlockType::Action)
        );

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Shot")),
            MakeTooltip(EGASBlockType::Shot),
            FSlateIcon(),
            MakeAction(EGASBlockType::Shot)
        );

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Character")),
            MakeTooltip(EGASBlockType::Character),
            FSlateIcon(),
            MakeAction(EGASBlockType::Character)
        );

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Dialogue")),
            MakeTooltip(EGASBlockType::Dialogue),
            FSlateIcon(),
            MakeAction(EGASBlockType::Dialogue)
        );
    }
    MenuBuilder.EndSection();

    const FVector2D ScreenPos = MouseEvent.GetScreenSpacePosition();

    FSlateApplication::Get().PushMenu(
        AsShared(),
        FWidgetPath(),
        MenuBuilder.MakeWidget(),
        ScreenPos,
        FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
    );
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

}

int32 SGASScriptPanel::ResolveSceneIndexAtY(float ScriptY) const
{
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
    if (TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin())
    {
        Tab->bShotAddArmed = bArmed;
    }
    HoveredShotParagraphIndex = INDEX_NONE;
    bNeedsRedraw = true;
}

void SGASScriptPanel::SetShotAddArmed(bool bInArmed)
{
    if (TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin())
    {
        Tab->bShotAddArmed = bInArmed;
    }

    TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin();
    const bool bShotModeActive = Tab.IsValid() && Tab->bShotAddArmed;

    if (!bShotModeActive)
    {
        HoveredShotParagraphIndex = INDEX_NONE;
        ClearPanelShotSelection();
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

    UE_LOG(LogTemp, Warning, TEXT("Script Cast Count: %d"), SourceScript->Cast.Num());
}

// =============================================================
// Shot Intent Popup (STUB)
// =============================================================
void SGASScriptPanel::OnEnterShotMarkingMode(const FString& SceneId)
{
    if (ScriptTab.IsValid())
    {
        ScriptTab.Pin()->EnterShotMarkingMode(SceneId);
    }
}


void SGAS_ScriptTab::UpdateShotIntentPreview()
{
    if (!PreviewWorld || !ShotCamera) return;

    // ------------------------------------------------------------
    // 1. Resolve Subject → StandInActor
    // ------------------------------------------------------------
    AGAS_StandInActor* TargetStandIn = nullptr;

    for (TActorIterator<AGAS_StandInActor> It(PreviewWorld); It; ++It)
    {
        AGAS_StandInActor* Actor = Cast<AGAS_StandInActor>(*It);

        if (Actor && SelectedShotIntentSubject.IsValid() && Actor->GAS_CharacterId == *SelectedShotIntentSubject)
        {
            TargetStandIn = Actor;
            break;
        }
    }

    if (!TargetStandIn) return;

    // ------------------------------------------------------------
    // 2. Compute CU Transform
    // ------------------------------------------------------------
    const FVector TargetLocation = TargetStandIn->GetActorLocation();
    const FVector UpOffset = FVector(0.f, 0.f, 160.f); // head height approx
    const FVector FocusPoint = TargetLocation + UpOffset;

    const float Distance = 120.f;

    const FVector CameraLocation =
        FocusPoint
        - TargetStandIn->GetActorForwardVector() * Distance
        + FVector(0.f, 0.f, 10.f);

    const FRotator CameraRotation =
        (FocusPoint - CameraLocation).Rotation();

    // ------------------------------------------------------------
    // 3. Apply to ShotCamera (SNAP)
    // ------------------------------------------------------------
    ShotCamera->SetActorLocation(CameraLocation);
    ShotCamera->SetActorRotation(CameraRotation);

    // ------------------------------------------------------------
    // 4. Force SceneCapture Update
    // ------------------------------------------------------------
    if (PreviewSceneCapture)
    {
        PreviewSceneCapture->SetWorldLocationAndRotation(
            CameraLocation,
            CameraRotation
        );

        PreviewSceneCapture->CaptureScene();
    }
}

void SGASScriptPanel::UpdateShotPreviewCamera(
    const FString& CharacterId,
    EGASShotType ShotType,
    ACineCameraActor* TargetCamera
)
{
    if (!GEditor || CharacterId.IsEmpty())
        return;

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
        return;

    AGAS_StandInActor* Actor =
        FindStandInByCharacter(World, CharacterId);

    if (!Actor)
        return;

    float Distance = 200.f;

    switch (ShotType)
    {
    case EGASShotType::ECU: Distance = 60.f; break;
    case EGASShotType::CU:  Distance = 100.f; break;
    case EGASShotType::MCU: Distance = 140.f; break;
    case EGASShotType::MS:  Distance = 200.f; break;
    case EGASShotType::MLS: Distance = 300.f; break;
    case EGASShotType::WS:  Distance = 500.f; break;
    case EGASShotType::EWS: Distance = 800.f; break;
    }

    FVector TargetLocation = Actor->GetActorLocation();

    // Try to get head height from mesh
    if (USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
    {
        FBox Bounds = Mesh->Bounds.GetBox();

        // Approximate head height (top of bounds)
        TargetLocation.Z = Bounds.Max.Z;
    }

    FVector CameraLocation =
        TargetLocation + FVector(Distance, 0.f, 0.f);

    FRotator LookAtRotation =
        (TargetLocation - CameraLocation).Rotation();

    // -------------------------------------------------
    // Apply to REAL camera
    // -------------------------------------------------
    if (TargetCamera)
    {
        TargetCamera->SetActorLocation(CameraLocation);
        TargetCamera->SetActorRotation(LookAtRotation);

        if (UCineCameraComponent* CineComp =
            TargetCamera->GetCineCameraComponent())
        {
            const float SensorWidth = 36.0f;
            const float SensorHeight = SensorWidth / 2.39f;

            CineComp->Filmback.SensorWidth = SensorWidth;
            CineComp->Filmback.SensorHeight = SensorHeight;

            CineComp->bConstrainAspectRatio = true;

            CineComp->SetFieldOfView(50.f);
        }
    }

    // -------------------------------------------------
    // Apply to PREVIEW (must match real camera)
    // -------------------------------------------------
    if (ShotPreviewCapture)
    {
        ShotPreviewCapture->CaptureScene();
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


FGASBlock* SGASScriptPanel::GetSceneBlockById(const FString& SceneId)
{
    if (!SourceScript)
        return nullptr;

    for (FGASBlock& Block : SourceScript->Blocks)
    {
        UE_LOG(LogTemp, Warning, TEXT("Block Id: %s"), *Block.Id);

        if (Block.Id == SceneId)
        {
            return &Block;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Looking for SceneId: %s"), *SceneId);

    return nullptr;
}

void SGAS_ScriptTab::OnShotIntentChanged()
{
    UpdateShotIntentPreview();
}


void SGASScriptPanel::OpenShotIntentPopup(const FString& MarkerId, const FString& SceneId, bool bHasBlocking)
{
    // -------------------------------------------------
    // GET SCRIPT (✅ CORRECT SOURCE)
    // -------------------------------------------------
    FGASScript* Script = SourceScript;

    EnabledCastNames.Empty();

    if (!Script)
    {
        UE_LOG(LogTemp, Error, TEXT("ShotIntent: Script NULL"));
        return;
    }

    // -------------------------------------------------
    // Get scene block FIRST
    // -------------------------------------------------
    FGASBlock* SceneBlock = GetSceneBlockById(SceneId);

    if (!SceneBlock)
    {
        UE_LOG(LogTemp, Error, TEXT("ShotIntent: SceneBlock NOT FOUND"));
        return;
    }

    // -------------------------------------------------
    // Add NO SUBJECT (ONLY ONCE)
    // -------------------------------------------------
    EnabledCastNames.Add(MakeShared<FString>(TEXT("(No Subject)")));

    // -------------------------------------------------
    // Add ONLY characters in this scene
    // -------------------------------------------------
    for (const FString& CharacterId : SceneBlock->CharactersInScene)
    {
        EnabledCastNames.Add(
            MakeShared<FString>(CharacterId)
        );
    }

    RebuildEnabledCastOptions();
    UE_LOG(LogTemp, Warning, TEXT("Cast Options Count: %d"), EnabledCastNames.Num());

    FString ResolvedShotLabel = TEXT("1");

    if (Script)
    {
        for (const FGASMarker& Marker : Script->Markers)
        {
            if (Marker.MarkerType != EGASMarkerType::ShotMarker)
            {
                continue;
            }

            // MarkerId being passed into this popup is NewMarker.Id
            // NOT MarkerGuid.ToString()
            if (Marker.Id == MarkerId)
            {
                ResolvedShotLabel = Marker.GetShotLabel(Script->ShotNumberingPolicy);

                UE_LOG(LogTemp, Warning, TEXT("RESOLVED SHOT LABEL: %s"), *ResolvedShotLabel);
                UE_LOG(LogTemp, Warning, TEXT("MATCHED MARKER SceneShotIndex: %d"), Marker.SceneShotIndex);

                break;
            }
        }
    }

    // -------------------------------------------------
    // CREATE WINDOW
    // -------------------------------------------------
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString("Shot Intent"))
        .ClientSize(FVector2D(900, 700))
        .SizingRule(ESizingRule::UserSized)
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    // -------------------------------------------------
    // SET CONTENT
    // -------------------------------------------------
    Window->SetContent(
        SNew(SGAS_ShotIntentWindow)
        .Script(Script)
        .SceneId(SceneId)
        .MarkerId(MarkerId)
        .ShotLabel(ResolvedShotLabel)
        .CastOptions(EnabledCastNames)
        .OwnerScriptTab(OwnerScriptTab)
        .OwnerWindow(Window)
        .OnConfirm_Lambda([this](FString MarkerId, EGASShotType ShotType, FString SubjectId)
            {
                if (!SourceScript)
                    return;

                FGASShotIntent* Intent = SourceScript->ShotIntents.Find(MarkerId);
                if (!Intent)
                {
                    UE_LOG(LogTemp, Warning, TEXT("ShotIntent: NOT FOUND"));
                    return;
                }

                Intent->ShotType = ShotType;
                Intent->SubjectId = SubjectId;

                UE_LOG(LogTemp, Warning, TEXT("ShotIntent Updated: %s | %s"),
                    *SubjectId,
                    *UEnum::GetValueAsString(ShotType)
                );

                UpdateShotPreviewCamera(
                    SubjectId,
                    ShotType,
                    Intent->CameraActor.Get()
                );

                if (TSharedPtr<SGAS_ScriptTab> Tab = OwnerScriptTab.Pin())
                {
                    Tab->OnSaveScript();
                }
            })
    );

    FSlateApplication::Get().AddWindow(Window);
}

