// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ControlsLandscapeHeightLayer.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UControlsLandscapeHeightLayer : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that control a RuntimeLandscape Layer with height data
 */
class RUNTIMEEDITABLELANDSCAPE_API IControlsLandscapeHeightLayer
{
	GENERATED_BODY()

public:
	virtual void GetPitchAndRoll(float& OutPitch, float& OutRoll) const = 0;
};
