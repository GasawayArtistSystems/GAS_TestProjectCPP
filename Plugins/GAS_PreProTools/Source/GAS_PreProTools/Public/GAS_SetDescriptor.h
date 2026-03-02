#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPtr.h"

struct FGASSetDescriptor
{
	FName SetId;                               // Internal ID
	FText DisplayName;                         // User-facing name
	FString LevelPath;                         // Blocking level path
	TSoftObjectPtr<UTexture2D> Thumbnail;      // Set preview image
};
