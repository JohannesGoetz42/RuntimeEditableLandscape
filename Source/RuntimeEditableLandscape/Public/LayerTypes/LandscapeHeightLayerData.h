// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeLayerDataBase.h"
#include "LandscapeHeightLayerData.generated.h"

/**
 * Landscape layer that affects the landscape height
 */
UCLASS(EditInlineNew)
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeHeightLayerData : public ULandscapeLayerDataBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	float HeightValue;
	
	virtual void ApplyToVertices(URuntimeLandscapeComponent* LandscapeComponent, const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex, float& OutHeightValue, FColor& OutVertexColor, float SmoothingFactor) const override;
};
