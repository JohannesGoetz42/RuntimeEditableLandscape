// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeLayerDataBase.h"
#include "LandscapeGroundTypeLayerData.generated.h"

#define MATERIAL_PARAMETER_GROUND_TYPE_LAYER_COLOR FName("GroundTypeLayerColor")

class ULandscapeGroundTypeData;
/**
 * Layer data that applies a landscape paint layer
 */
UCLASS(EditInlineNew)
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeGroundTypeLayerData : public ULandscapeLayerDataBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<const ULandscapeGroundTypeData> GroundType;

	virtual void ApplyToLandscape(ARuntimeLandscape* Landscape, const ULandscapeLayerComponent* LandscapeLayerComponent) const override;
};
