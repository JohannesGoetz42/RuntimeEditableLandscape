// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerComponent.h"

#include "RuntimeLandscape.h"
#include "Kismet/GameplayStatics.h"

void ULandscapeLayerComponent::ApplyToLandscape() const
{
	TArray<AActor*> LandscapeActors;

	// TODO: add a more elegant way to find the target landscape
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARuntimeLandscape::StaticClass(), LandscapeActors);

	if (LandscapeActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("LandscapeLayerComponent on %s could not find a landscape and can not be applied."),
		       *GetOwner()->GetName());
	}

	for (AActor* LandscapeActor : LandscapeActors)
	{
		Cast<ARuntimeLandscape>(LandscapeActor)->AddLandscapeLayer(this);
	}
}

void ULandscapeLayerComponent::ApplyLayerData(const FVector2D VertexLocation, float& OutHeightValue,
                                              FColor& OutVertexColorValue) const
{
	if (!GetAffectedArea(true).IsInside(VertexLocation))
	{
		return;
	}

	float SmoothingFactor = 0.0f;
	if (LayerData.SmoothingDistance > 0.0f)
	{
		const float SquaredDistance = GetAffectedArea(false).ComputeSquaredDistanceToPoint(VertexLocation);
		SmoothingFactor = SquaredDistance / FMath::Square(LayerData.SmoothingDistance);
		SmoothingFactor = FMath::Clamp(SmoothingFactor, 0.0f, 1.0f);
	}

	if (LayerData.bApplyHeight)
	{
		OutHeightValue = FMath::Lerp(LayerData.HeightValue, OutHeightValue, SmoothingFactor);
	}

	if (LayerData.bApplyVertexColor)
	{
		OutVertexColorValue = FLinearColor::LerpUsingHSV(LayerData.VertexColor, OutVertexColorValue, SmoothingFactor).
			ToFColor(false);
	}
}

FBox2D ULandscapeLayerComponent::GetAffectedArea(bool bIncludeSmoothing) const
{
	FVector Origin;
	FVector Extent;
	if (LayerData.BoundsOverride)
	{
		Origin = LayerData.BoundsOverride->GetComponentLocation();
		Extent = LayerData.BoundsOverride->Bounds.BoxExtent;
	}
	else
	{
		GetOwner()->GetActorBounds(false, Origin, Extent);
	}

	float SmoothingOffset = 0;
	if (bIncludeSmoothing)
	{
		if (LayerData.SmoothingDirection != ESmoothingDirection::SD_Inwards)
		{
			SmoothingOffset = LayerData.SmoothingDistance;
		}
	}
	else if (LayerData.SmoothingDirection != ESmoothingDirection::SD_Outwards)
	{
		SmoothingOffset = -LayerData.SmoothingDistance;
	}

	if (LayerData.SmoothingDirection == SD_Center)
	{
		SmoothingOffset *= 0.5f;
	}

	return FBox2D(
		FVector2D(Origin.X - Extent.X - SmoothingOffset, Origin.Y - Extent.Y - SmoothingOffset),
		FVector2D(Origin.X + Extent.X + SmoothingOffset, Origin.Y + Extent.Y + SmoothingOffset));
}

// Called when the game starts
void ULandscapeLayerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	if (!bWaitForActivation)
	{
		ApplyToLandscape();
	}
}
