// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeLayerComponent.h"
#include "LandscapeLayerDataBase.h"
#include "LandscapeVertexColorLayerData.generated.h"

/**
 * Landscape layer that affects vertex colors
 */
UCLASS(EditInlineNew)
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeVertexColorLayerData : public ULandscapeLayerDataBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	FColor VertexColor;

	virtual void ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
	                           const ULandscapeLayerComponent* LayerComponent,
	                           float& OutHeightValue, FColor& OutVertexColor,
	                           const FLandscapeLayerVertexInfo& VertexInfo) const override
	{
		OutVertexColor = FLinearColor::LerpUsingHSV(VertexColor, OutVertexColor, VertexInfo.SmoothingFactor).
			ToFColor(false);
	}
};
