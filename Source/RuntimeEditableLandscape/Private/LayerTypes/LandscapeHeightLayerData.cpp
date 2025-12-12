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
	TArray<FLandscapeVertex> CornerVertices = LandscapeComponent->GetParentLandscape()->GetCornerVerticesOfBox(
		OwningLayer->GetBoundingBox(), BoxAngle);

	// get initial height values
	float TopLeftCornerHeight =
		CornerVertices[0].ContainingComponent->GetInitialHeightValues()[CornerVertices[0].VertexIndex];

	float TopRightCornerHeight =
		CornerVertices[1].ContainingComponent->GetInitialHeightValues()[CornerVertices[1].VertexIndex];

	float BottomRightCornerHeight =
		CornerVertices[2].ContainingComponent->GetInitialHeightValues()[CornerVertices[2].VertexIndex];

	float BottomLeftCornerHeight =
		CornerVertices[3].ContainingComponent->GetInitialHeightValues()[CornerVertices[3].VertexIndex];

	// calculate average heights for box edges
	float AvgHeightTopEdge = (TopLeftCornerHeight + TopRightCornerHeight) * 0.5f;
	float AvgHeightBottomEdge = (BottomLeftCornerHeight + BottomRightCornerHeight) * 0.5f;
	float AvgHeightLeftEdge = (TopLeftCornerHeight + BottomLeftCornerHeight) * 0.5f;
	float AvgHeightRightEdge = (TopRightCornerHeight + BottomRightCornerHeight) * 0.5f;

	FVector TopLeftLocation3D = FVector(
		CornerVertices[0].ContainingComponent->GetRelativeVertexLocation(CornerVertices[0].VertexIndex),
		TopLeftCornerHeight) + CornerVertices[0].ContainingComponent->GetComponentLocation() * FVector(
		1.0f, 1.0f, 0.0f);
	DrawDebugSphere(GetWorld(), TopLeftLocation3D, 50.0f, 8, FColor::Magenta, false, 20.0f);

	FVector TopRightLocation3D = FVector(
		CornerVertices[1].ContainingComponent->GetRelativeVertexLocation(CornerVertices[1].VertexIndex),
		TopRightCornerHeight) + CornerVertices[1].ContainingComponent->GetComponentLocation() * FVector(
		1.0f, 1.0f, 0.0f);
	DrawDebugSphere(GetWorld(), TopRightLocation3D, 50.0f, 8, FColor::Magenta, false, 20.0f);

	FVector BottomRightLocation3D = FVector(
		CornerVertices[2].ContainingComponent->GetRelativeVertexLocation(CornerVertices[2].VertexIndex),
		BottomRightCornerHeight) + CornerVertices[2].ContainingComponent->GetComponentLocation() * FVector(
		1.0f, 1.0f, 0.0f);
	DrawDebugSphere(GetWorld(), BottomRightLocation3D, 50.0f, 8, FColor::Magenta, false, 20.0f);

	FVector BottomLeftLocation3D = FVector(
		CornerVertices[3].ContainingComponent->GetRelativeVertexLocation(CornerVertices[3].VertexIndex),
		BottomLeftCornerHeight) + CornerVertices[3].ContainingComponent->GetComponentLocation() * FVector(
		1.0f, 1.0f, 0.0f);
	DrawDebugSphere(GetWorld(), BottomLeftLocation3D, 50.0f, 8, FColor::Magenta, false, 20.0f);

	HeightDifferenceTopBottom = AvgHeightBottomEdge - AvgHeightTopEdge;
	HeightDifferenceLeftRight = AvgHeightLeftEdge - AvgHeightRightEdge;
}
