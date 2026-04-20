#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCompoundWidget.h"

#include "GAS_ShotIntentTypes.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class ACineCameraActor;
class SGAS_ScriptTab;
class SWindow;
struct FGASScript;

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
        SLATE_ARGUMENT(TWeakPtr<SWindow>, OwnerWindow)
        SLATE_ARGUMENT(TSharedPtr<FGASShotIntent>, ShotIntent)
        SLATE_EVENT(FOnTextCommitted, OnConfirm)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    ~SGAS_ShotIntentWindow();

    const FSlateBrush* GetPreviewBrush();
    FText GetShotNumberText() const;

    void InitializeCameraPreview();
    void InitializePreviewState();
    void ApplyPreviewState();

    FReply OnDollyForward();

    void SetSourceCamera(ACineCameraActor* InCamera);

    TSharedPtr<FGASShotIntent> Intent = nullptr;

private:

    FGASScript* Script = nullptr;
    FString SceneId;
    FString MarkerId;
    int32 ShotNumber = 0;
    FString ShotLabel;

    TArray<TSharedPtr<FString>> CastOptions;
    TSharedPtr<FString> SelectedCharacter;

    TArray<TSharedPtr<EGASShotType>> ShotTypeOptions;
    TSharedPtr<EGASShotType> SelectedShotType;

    TWeakPtr<SGAS_ScriptTab> OwnerScriptTab;
    TWeakPtr<SWindow> OwnerWindow;

    FOnTextCommitted OnConfirm;

    TWeakObjectPtr<ACineCameraActor> PreviewCameraActor;
    USceneCaptureComponent2D* SceneCaptureComponent = nullptr;
    UTextureRenderTarget2D* PreviewRenderTarget = nullptr;

    TSharedPtr<FSlateBrush> PreviewBrush;
    TSharedPtr<class SImage> ShotPreviewImage;

    struct FPreviewState
    {
        FVector Location;
        FRotator Rotation;
        float FocalLength = 35.f;
    };

    FPreviewState PreviewState;
};