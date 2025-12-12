// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeHoleLayerData.h"

#include "LandscapeLayerComponent.h"
#include "RuntimeLandscapeComponent.h"

void ULandscapeHoleLayerData::ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
                                            const ULandscapeLayerComponent* LayerComponent, float& OutHeightValue,
                                            FColor& OutVertexColor,
                                            const FLandscapeLayerVertexInfo& VertexInfo) const
{
	if (VertexInfo.SmoothingFactor < SmoothingValueThreshold)
	{
		LandscapeComponent->SetHoleFlagForVertex(VertexInfo.VertexIndex, true);
	}
}
