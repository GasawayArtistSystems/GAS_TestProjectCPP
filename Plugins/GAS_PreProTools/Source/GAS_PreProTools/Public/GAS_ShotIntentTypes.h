#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "GAS_ShotIntentTypes.generated.h"

// =====================================================
// Shot Intent Enums (Creative, NOT Technical)
// =====================================================

UENUM(BlueprintType)
enum class EGASShotType : uint8
{
    ECU     UMETA(DisplayName = "Extreme Close-Up"),
    CU      UMETA(DisplayName = "Close-Up"),
    MCU     UMETA(DisplayName = "Medium Close-Up"),
    MS      UMETA(DisplayName = "Medium Shot"),
    MLS     UMETA(DisplayName = "Medium Long Shot"),
    WS      UMETA(DisplayName = "Wide Shot"),
    EWS     UMETA(DisplayName = "Extreme Wide Shot")
};

UENUM(BlueprintType)
enum class EGASCameraBehavior : uint8
{
    Static      UMETA(DisplayName = "Static"),
    Pan         UMETA(DisplayName = "Pan"),
    Tilt        UMETA(DisplayName = "Tilt"),
    Push        UMETA(DisplayName = "Push In"),
    Pull        UMETA(DisplayName = "Pull Out"),
    Handheld    UMETA(DisplayName = "Handheld")
};

UENUM(BlueprintType)
enum class EGASFramingBias : uint8
{
    Center      UMETA(DisplayName = "Center"),
    Left        UMETA(DisplayName = "Left"),
    Right       UMETA(DisplayName = "Right"),
    High        UMETA(DisplayName = "High"),
    Low         UMETA(DisplayName = "Low")
};

UENUM(BlueprintType)
enum class EGASLensIntent : uint8
{
    L24     UMETA(DisplayName = "24mm"),
    L35     UMETA(DisplayName = "35mm"),
    L50     UMETA(DisplayName = "50mm"),
    L85     UMETA(DisplayName = "85mm"),
    L100    UMETA(DisplayName = "100mm")
};

// =====================================================
// Shot Intent Struct (Creative Meaning Only)
// =====================================================

USTRUCT(BlueprintType)
struct FGASShotIntent
{
    GENERATED_BODY()

    // -------------------------------------------------
    // Identity
    // -------------------------------------------------

    // Must match FGASMarker::Id
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString MarkerId;

    // -------------------------------------------------
    // Creative Intent
    // -------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASShotType ShotType = EGASShotType::MS;

    // Character / subject identifier (string for now)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString SubjectId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASLensIntent Lens = EGASLensIntent::L50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASCameraBehavior CameraBehavior = EGASCameraBehavior::Static;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASFramingBias Framing = EGASFramingBias::Center;

    FString CharacterName;

    UPROPERTY()
    TSoftObjectPtr<ACineCameraActor> CameraActor;

    UPROPERTY()
    FString CameraName;

};
