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

	float LayerElevationX;
	float LayerElevationY;
	float TopLeftHeight;
	
	virtual void ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
	                           const ULandscapeLayerComponent* LayerComponent, float& OutHeightValue,
	                           FColor& OutVertexColor, const FLandscapeLayerVertexInfo& VertexInfo) const override;
	virtual void InitializeLayerMemory(const ULandscapeLayerComponent* OwningLayer,
	                                   const URuntimeLandscapeComponent* LandscapeComponent) override;
};
