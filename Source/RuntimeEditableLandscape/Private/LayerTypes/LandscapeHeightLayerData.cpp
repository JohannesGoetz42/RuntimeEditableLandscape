// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeHeightLayerData.h"

#include "LandscapeLayerComponent.h"

void ULandscapeHeightLayerData::ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
                                              const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex,
                                              float& OutHeightValue, FColor& OutVertexColor,
                                              float SmoothingFactor) const
{
	ULandscapeLayerMemoryBase* MyMemory = Cast<ULandscapeLayerMemoryBase>(Memory.Get());
	if (ensure(MyMemory))
	{
		OutHeightValue = FMath::Lerp(HeightValue + LayerComponent->GetOwner()->GetActorLocation().Z, OutHeightValue,
		                             SmoothingFactor);
	}
}

void ULandscapeHeightLayerData::InitializeLayerMemory(const ULandscapeLayerComponent* OwningLayer)
{
	ensure(false);
}