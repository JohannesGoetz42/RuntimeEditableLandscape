// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	virtual void Apply(URuntimeLandscapeComponent* LandscapeComponent, const ULandscapeLayerComponent* LayerComponent,
	                   int32 VertexIndex, float& OutHeightValue, FColor& OutVertexColor,
	                   float SmoothingFactor) const override
	{
		OutVertexColor = FLinearColor::LerpUsingHSV(VertexColor, OutVertexColor, SmoothingFactor).ToFColor(false);
	}
};
