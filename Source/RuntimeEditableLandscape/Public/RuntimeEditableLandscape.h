 // Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_STATS_GROUP(TEXT("Stats for the runtime editable landscape"), STATGROUP_RuntimeLandscape, STATCAT_Advanced)
DECLARE_CYCLE_STAT(TEXT("Update runtime landscape"), STAT_UpdateRuntimeLandscape, STATGROUP_RuntimeLandscape)
DECLARE_CYCLE_STAT(TEXT("Add landscape layer"), STAT_AddLandscapeLayer, STATGROUP_RuntimeLandscape)

class FRuntimeEditableLandscapeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
