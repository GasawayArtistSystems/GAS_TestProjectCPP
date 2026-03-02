#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

/**
 * Minimal global gate for editor selection.
 * No subsystems, no registries, no actor plumbing.
 * ScriptTab (or whoever owns “blocking mode”) flips this on/off.
 */
namespace GASBlockingAccess
{
	GAS_PREPROTOOLS_API bool IsBlockingActive();
	GAS_PREPROTOOLS_API void SetBlockingActive(bool bActive);

	GAS_PREPROTOOLS_API void SetActiveSceneId(const FGuid& SceneId);
	GAS_PREPROTOOLS_API FGuid GetActiveSceneId();
}