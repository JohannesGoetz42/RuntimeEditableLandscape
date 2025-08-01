// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeLayerDataBase.h"
#include "LandscapeHoleLayerData.generated.h"

/**
 * Landscape layer that adds a hole to the landscape
 */
UCLASS(EditInlineNew)
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeHoleLayerData : public ULandscapeLayerDataBase
{
	GENERATED_BODY()

	virtual void ApplyToVertices(URuntimeLandscapeComponent* LandscapeComponent, const ULandscapeLayerComponent* LayerComponent,
	                   int32 VertexIndex, float& OutHeightValue, FColor& OutVertexColor,
	                   float SmoothingFactor) const override;
};
