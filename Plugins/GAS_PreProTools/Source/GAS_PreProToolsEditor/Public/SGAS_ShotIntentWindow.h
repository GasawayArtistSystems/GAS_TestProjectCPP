#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "ScriptModel.h"
#include "SGAS_ScriptTab.h"

DECLARE_DELEGATE_ThreeParams(
    FOnConfirmShotIntent,
    FString,        // MarkerId
    EGASShotType,   // ShotType
    FString         // SubjectId
);


class SGAS_ShotIntentWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGAS_ShotIntentWindow) {}
        SLATE_ARGUMENT(FGASScript*, Script)
        SLATE_ARGUMENT(FString, SceneId)
        SLATE_ARGUMENT(FString, MarkerId)
        SLATE_ARGUMENT(int32, ShotNumber)
        SLATE_ARGUMENT(FString, ShotLabel)
        SLATE_ARGUMENT(TArray<TSharedPtr<FString>>, CastOptions)
        SLATE_ARGUMENT(TWeakPtr<SGAS_ScriptTab>, OwnerScriptTab)
        SLATE_EVENT(FOnConfirmShotIntent, OnConfirm)
        SLATE_ARGUMENT(FGASShotIntent, ShotIntent)
        SLATE_ARGUMENT(TWeakPtr<SWindow>, OwnerWindow)

    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    FString ShotLabel;

    virtual ~SGAS_ShotIntentWindow();

private:
    FGASScript* Script = nullptr;
    FString SceneId;
    FString MarkerId;
    TArray<TSharedPtr<FString>> CastOptions;
    TSharedPtr<FString> SelectedCharacter;
    TArray<TSharedPtr<EGASShotType>> ShotTypeOptions;
    TSharedPtr<EGASShotType> SelectedShotType;

    TSharedPtr<SImage> ShotPreviewImage;
    TWeakPtr<class SGAS_ScriptTab> OwnerScriptTab;

    FText GetShotNumberText() const;
    FGASShotIntent Intent;
    int32 ShotNumber = -1;
    FOnConfirmShotIntent OnConfirm;
    const FSlateBrush* GetPreviewBrush();

    TWeakPtr<SWindow> OwnerWindow;
    TWeakObjectPtr<ACineCameraActor> PreviewCameraActor;

    void InitializeCameraPreview();

    USceneCaptureComponent2D* SceneCaptureComponent = nullptr;
    UTextureRenderTarget2D* PreviewRenderTarget = nullptr;
    TSharedPtr<FSlateBrush> PreviewBrush;

    // ------------------------------------------------------------
    // Preview Camera State (TEMP - does NOT touch ShotIntent)
    // ------------------------------------------------------------
    struct FGASPreviewCameraState
    {
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        float FocalLength = 50.f;
    };

    void SetSourceCamera(ACineCameraActor* InCamera);

    FGASPreviewCameraState PreviewState;

    void InitializePreviewState();
    void ApplyPreviewState();

    // ------------------------------------------------------------
    // Preview Camera Nudges
    // ------------------------------------------------------------

    FReply OnDollyForward();



};