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
