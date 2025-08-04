// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeHoleLayerData.h"

#include "RuntimeLandscapeComponent.h"

void ULandscapeHoleLayerData::ApplyToVertices(URuntimeLandscapeComponent* LandscapeComponent,
                                    const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex, float& OutHeightValue, FColor& OutVertexColor,
                                    float SmoothingFactor) const
{
	if (SmoothingFactor < SmoothingValueThreshold)
	{
		LandscapeComponent->SetHoleFlagForVertex(VertexIndex, true);
	}
}
