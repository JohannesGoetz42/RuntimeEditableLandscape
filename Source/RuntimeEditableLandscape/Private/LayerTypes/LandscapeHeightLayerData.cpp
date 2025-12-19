// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeHeightLayerData.h"

#include "LandscapeLayerComponent.h"
#include "Interfaces/ControlsLandscapeHeightLayer.h"

void ULandscapeHeightLayerData::ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
                                              const ULandscapeLayerComponent* LayerComponent,
                                              float& OutHeightValue, FColor& OutVertexColor,
                                              const FLandscapeLayerVertexInfo& VertexInfo) const
{
	float InterpolatedHeightValue = VertexInfo.BoundsDistanceLeft * LayerElevationX
		+ VertexInfo.BoundsDistanceTop * LayerElevationY
		+ TopLeftHeight;

	OutHeightValue = FMath::Lerp(InterpolatedHeightValue + HeightValue, OutHeightValue, VertexInfo.SmoothingFactor);
}

void ULandscapeHeightLayerData::InitializeLayerMemory(const ULandscapeLayerComponent* OwningLayer,
                                                      const URuntimeLandscapeComponent* LandscapeComponent)
{
	if (IControlsLandscapeHeightLayer* HeightController = Cast<IControlsLandscapeHeightLayer>(OwningLayer->GetOwner()))
	{
		float Pitch;
		float Roll;
		HeightController->GetPitchAndRoll(Pitch, Roll);

		FVector2D BoundsSize = OwningLayer->GetBoundingBox().GetExtent();

		float SlopeX = FMath::Tan(FMath::DegreesToRadians(Pitch));
		LayerElevationX = SlopeX * -BoundsSize.X;

		float SlopeY = FMath::Tan(FMath::DegreesToRadians(Roll));
		LayerElevationY = SlopeY * BoundsSize.Y;

		TopLeftHeight = OwningLayer->GetOwner()->GetActorLocation().Z - (LayerElevationX + LayerElevationY) * 0.5f;
	}
	else
	{
		LayerElevationX = 0.0f;
		LayerElevationY = 0.0f;
		TopLeftHeight = OwningLayer->GetOwner()->GetActorLocation().Z;
	}
}
