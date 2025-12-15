// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeHeightLayerData.h"

#include "LandscapeLayerComponent.h"
#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"

void ULandscapeHeightLayerData::ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
                                              const ULandscapeLayerComponent* LayerComponent,
                                              float& OutHeightValue, FColor& OutVertexColor,
                                              const FLandscapeLayerVertexInfo& VertexInfo) const
{
	float AdjustedHeightValue = HeightValue + LayerComponent->GetOwner()->GetActorLocation().Z
		+ VertexInfo.BoundsDistanceLeft * HeightDifferenceLeftRight
		+ VertexInfo.BoundsDistanceTop * HeightDifferenceTopBottom;

	OutHeightValue = FMath::Lerp(AdjustedHeightValue, OutHeightValue, VertexInfo.SmoothingFactor);
}

void ULandscapeHeightLayerData::InitializeLayerMemory(const ULandscapeLayerComponent* OwningLayer,
                                                      const URuntimeLandscapeComponent* LandscapeComponent)
{
	float BoxAngle = OwningLayer->GetBoundsComponent()->GetComponentRotation().Yaw;
	if (!LandscapeComponent->GetParentLandscape()->TryCalculateElevationInBoxDirections(
		OwningLayer->GetBoundingBox(), BoxAngle, ElevationXDirection, ElevationYDirection))
	{
		ElevationXDirection = 0;
		ElevationYDirection = 0;
		ensureAlways(false);
	}
}
