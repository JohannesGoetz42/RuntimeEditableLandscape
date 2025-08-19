// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeGroundTypeLayerData.h"

#include "LandscapeLayerComponent.h"
#include "RuntimeLandscape.h"
#include "Components/BoxComponent.h"

void ULandscapeGroundTypeLayerData::ApplyToLandscape(ARuntimeLandscape* Landscape,
                                                     const ULandscapeLayerComponent* LandscapeLayerComponent) const
{
	if (ensure(GroundType))
	{
		const UBoxComponent* BoundingBox = LandscapeLayerComponent->GetOwner()->GetComponentByClass<
			UBoxComponent>();
		const FVector& BoxExtent = BoundingBox->GetScaledBoxExtent();

		ELayerShape Shape = LandscapeLayerComponent->GetShape();

		const FTransform& WorldTransform = LandscapeLayerComponent->GetOwner()->GetActorTransform();
		
		Landscape->DrawGroundType(GroundType, Shape, WorldTransform, BoxExtent);
	}
}
