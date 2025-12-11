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
	UPROPERTY(EditAnywhere)
	/** 
	 * the maximum angle of the flattened plane 
	 * the required angle is derived from the owner transform 
	 */
	float MaxAngle = 0.0f;

	float InclinationTopBottom = 0.0f;
	float InclinationLeftRight = 0.0f;

	virtual void ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
	                           const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex, float& OutHeightValue,
	                           FColor& OutVertexColor, float SmoothingFactor) const override;
	virtual void InitializeLayerMemory(const ULandscapeLayerComponent* OwningLayer,
	                                   const URuntimeLandscapeComponent* LandscapeComponent) override;
};
