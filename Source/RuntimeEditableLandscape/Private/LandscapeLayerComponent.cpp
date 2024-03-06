// Fill out your copyright notice in the Description page of Project Settings.


#include "LandscapeLayerComponent.h"

#include "RuntimeLandscape.h"
#include "RuntimeLandscapeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

void ULandscapeLayerComponent::ApplyToLandscape()
{
	if (AffectedLandscapes.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("LandscapeLayerComponent on %s could not find a landscape and can not be applied."),
		       *GetOwner()->GetName());
	}

	for (AActor* LandscapeActor : AffectedLandscapes)
	{
		Cast<ARuntimeLandscape>(LandscapeActor)->AddLandscapeLayer(this);
	}

	if (BoundsComponent)
	{
		BoundsComponent->TransformUpdated.AddUObject(this, &ULandscapeLayerComponent::HandleBoundsChanged);
	}
	else
	{
		GetOwner()->GetRootComponent()->TransformUpdated.AddUObject(
			this, &ULandscapeLayerComponent::HandleBoundsChanged);
	}
}

void ULandscapeLayerComponent::ApplyLayerData(int32 VertexIndex, URuntimeLandscapeComponent* LandscapeComponent,
                                              float& OutHeightValue,
                                              FColor& OutVertexColorValue) const
{
	const FVector2D VertexLocation = LandscapeComponent->GetRelativeVertexLocation(VertexIndex) + FVector2D(
		LandscapeComponent->GetComponentLocation());
	if (!GetBoundingBox().IsInside(VertexLocation))
	{
		return;
	}

	float SmoothingFactor;
	if (TryCalculateSmoothingFactor(SmoothingFactor, VertexLocation))
	{
		for (const auto& Layer : LayerData)
		{
			switch (Layer.Key)
			{
			case ELandscapeLayerType::LLT_Height:
				OutHeightValue = FMath::Lerp(Layer.Value.FloatValue + GetOwner()->GetActorLocation().Z, OutHeightValue,
				                             SmoothingFactor);
				break;
			case ELandscapeLayerType::LLT_VertexColor:
				OutVertexColorValue = FLinearColor::LerpUsingHSV(Layer.Value.ColorValue, OutVertexColorValue,
				                                                 SmoothingFactor).ToFColor(false);
				break;
			case ELandscapeLayerType::LLT_Hole:
				if (Layer.Value.bBoolValue || SmoothingFactor == 0.0f)
				{
					LandscapeComponent->SetHoleFlagForVertex(VertexIndex, true);
				}
				break;
			case ELandscapeLayerType::LLT_None:
			default:
				break;
			}
		}
	}
}

void ULandscapeLayerComponent::SetBoundsComponent(UPrimitiveComponent* NewBoundsComponent)
{
	if (NewBoundsComponent->IsA<USphereComponent>())
	{
		Shape = ELayerShape::HS_Sphere;
	}
	else
	{
		Shape = ELayerShape::HS_Box;
	}

	BoundsComponent = NewBoundsComponent;
	Extent = BoundsComponent->Bounds.BoxExtent;
}

void ULandscapeLayerComponent::UpdateShape()
{
	const FVector Origin = BoundsComponent ? BoundsComponent->GetComponentLocation() : GetOwner()->GetActorLocation();

	float BoundsSmoothingOffset = 0;
	float InnerSmoothingOffset = 0;
	switch (SmoothingDirection)
	{
	case SD_Inwards:
		InnerSmoothingOffset = SmoothingDistance;
		break;
	case SD_Outwards:
		BoundsSmoothingOffset = SmoothingDistance;
		break;
	case SD_Center:
		InnerSmoothingOffset = SmoothingDistance * 0.5f;
		BoundsSmoothingOffset = SmoothingDistance * 0.5f;
		break;
	default:
		checkNoEntry();
	}

	if (Shape == ELayerShape::HS_Sphere)
	{
		BoundingBox = FBox2D(FVector2D(Origin - BoundsSmoothingOffset - Radius),
		                     FVector2D(Origin + BoundsSmoothingOffset + Radius));
		return;
	}

	FBoxSphereBounds BoxSphereBounds(Origin, Extent + BoundsSmoothingOffset, Radius);
	const FTransform Transform = BoundsComponent
		                             ? BoundsComponent->GetComponentTransform()
		                             : GetOwner()->GetActorTransform();
	BoxSphereBounds = BoxSphereBounds.TransformBy(Transform);

	BoundingBox = FBox2D(FVector2D(Origin - BoxSphereBounds.BoxExtent), FVector2D(Origin + BoxSphereBounds.BoxExtent));
	DrawDebugBox(GetWorld(), Origin, FVector(BoundingBox.GetExtent(), Extent.Z), FColor::Green, false, 10.0f);

	InnerBox.Min = FVector2D(Origin - Extent) + InnerSmoothingOffset;
	InnerBox.Max = FVector2D(Origin + Extent) - InnerSmoothingOffset;
	FQuat Rotation = BoundsComponent ? BoundsComponent->GetComponentQuat() : GetOwner()->GetActorQuat();
	DrawDebugBox(GetWorld(), Origin, FVector(InnerBox.GetExtent(), Extent.Z), Rotation, FColor::Blue, false, 10.0f);
	FVector2D Test(Extent + BoundsSmoothingOffset);
	DrawDebugBox(GetWorld(), Origin, FVector(Test, Extent.Z), Rotation, FColor::Cyan, false, 10.0f);
}

bool ULandscapeLayerComponent::TryCalculateSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location) const
{
	float SmoothingOffset = 0.0f;
	if (SmoothingDirection != ESmoothingDirection::SD_Outwards)
	{
		SmoothingOffset = SmoothingDirection == ESmoothingDirection::SD_Center
			                  ? SmoothingDistance * 0.5f
			                  : SmoothingDistance;
	}

	const FVector2D Origin = FVector2D(BoundsComponent
		                                   ? BoundsComponent->GetComponentLocation()
		                                   : GetOwner()->GetActorLocation());
	switch (Shape)
	{
	case ELayerShape::HS_Box:
		return TryCalculateBoxSmoothingFactor(OutSmoothingFactor, Location, Origin);

	case ELayerShape::HS_Sphere:
		return TryCalculateSphereSmoothingFactor(OutSmoothingFactor, Location, Origin, SmoothingOffset);
	default:
		checkNoEntry();
	}

	return false;
}

bool ULandscapeLayerComponent::TryCalculateBoxSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location,
                                                              FVector2D Origin) const
{
	const FVector RotatedLocation = UKismetMathLibrary::InverseTransformLocation(
		BoundsComponent ? BoundsComponent->GetComponentTransform() : GetOwner()->GetActorTransform(),
		FVector(Location, 0.0f));

	const float DistanceSqr = InnerBox.ComputeSquaredDistanceToPoint(FVector2D(RotatedLocation) + Origin);
	const float SmoothingDistanceSqr = FMath::Square(SmoothingDistance);
	if (DistanceSqr >= SmoothingDistanceSqr)
	{
		return false;
	}

	OutSmoothingFactor = DistanceSqr == 0.0f ? 0.0f : DistanceSqr / SmoothingDistanceSqr;
	return true;
}

bool ULandscapeLayerComponent::TryCalculateSphereSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location,
                                                                 FVector2D Origin, float SmoothingOffset) const
{
	const float OuterRadiusSquared = FMath::Square(Radius + SmoothingOffset);
	const float DistanceSqr = (Origin - Location).SizeSquared();
	if (DistanceSqr >= OuterRadiusSquared)
	{
		return false;
	}

	const float InnerRadiusSqr = FMath::Square(Radius - SmoothingOffset);
	if (DistanceSqr < InnerRadiusSqr)
	{
		OutSmoothingFactor = 0.0f;
	}
	else
	{
		check(SmoothingDistance > 0.0f);
		OutSmoothingFactor = (DistanceSqr - InnerRadiusSqr) / FMath::Square(SmoothingDistance);
	}

	ensure(OutSmoothingFactor >= 0.0f && OutSmoothingFactor <= 1.0f);
	return true;
}

void ULandscapeLayerComponent::HandleBoundsChanged(USceneComponent* SceneComponent,
                                                   EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	UpdateShape();
	for (ARuntimeLandscape* AffectedLandscape : AffectedLandscapes)
	{
		AffectedLandscape->RemoveLandscapeLayer(this, false);
		AffectedLandscape->AddLandscapeLayer(this);
	}
}

void ULandscapeLayerComponent::RemoveFromLandscapes()
{
	for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
	{
		if (Landscape)
		{
			for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(
				     GetBoundingBox()))
			{
				LandscapeComponent->RemoveLandscapeLayer(this, true);
			}
		}
	}
}

// Called when the game starts
void ULandscapeLayerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...	
	if (AffectedLandscapes.IsEmpty())
	{
		TArray<AActor*> LandscapeActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ARuntimeLandscape::StaticClass(), LandscapeActors);
		for (AActor* LandscapeActor : LandscapeActors)
		{
			AffectedLandscapes.Add(Cast<ARuntimeLandscape>(LandscapeActor));
		}
	}

	if (!bWaitForActivation)
	{
		ApplyToLandscape();
	}
}

void ULandscapeLayerComponent::DestroyComponent(bool bPromoteChildren)
{
	RemoveFromLandscapes();
	Super::DestroyComponent(bPromoteChildren);
}

#if WITH_EDITORONLY_DATA
void ULandscapeLayerComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	RemoveFromLandscapes();
}

void ULandscapeLayerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateShape();
	for (TObjectPtr<ARuntimeLandscape> Landscape : AffectedLandscapes)
	{
		if (Landscape)
		{
			for (URuntimeLandscapeComponent* LandscapeComponent : Landscape->GetComponentsInArea(
				     GetBoundingBox()))
			{
				LandscapeComponent->AddLandscapeLayer(this, true);
			}
		}
	}

	if (!AffectedLandscapes.IsEmpty())
	{
		if (BoundsComponent)
		{
			BoundsComponent->TransformUpdated.AddUObject(this, &ULandscapeLayerComponent::HandleBoundsChanged);
		}
		else
		{
			GetOwner()->GetRootComponent()->TransformUpdated.AddUObject(
				this, &ULandscapeLayerComponent::HandleBoundsChanged);
		}
	}
}
#endif
