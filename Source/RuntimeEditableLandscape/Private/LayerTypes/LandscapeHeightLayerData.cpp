// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeHeightLayerData.h"

#include "LandscapeLayerComponent.h"

void ULandscapeHeightLayerData::ApplyToVertices(URuntimeLandscapeComponent* LandscapeComponent, const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex,
                                      float& OutHeightValue, FColor& VertexColor, float SmoothingFactor) const
{
	OutHeightValue = FMath::Lerp(HeightValue + LayerComponent->GetOwner()->GetActorLocation().Z, OutHeightValue,
								 SmoothingFactor);
}
