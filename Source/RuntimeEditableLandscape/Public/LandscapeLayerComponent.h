// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LandscapeLayerComponent.generated.h"


class ARuntimeLandscape;

USTRUCT()
struct FLandscapeLayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.0f))
	/** The distance in which the layer effect fades out (extends the Owner's bounds) */
	float SmoothingDistance = 200.0f;
	UPROPERTY(EditAnywhere)
	bool bApplyHeight = false;
	UPROPERTY(EditAnywhere)
	float HeightValue = 0.0f;
	UPROPERTY(EditAnywhere)
	bool bApplyVertexColor = false;
	UPROPERTY(EditAnywhere)
	FColor VertexColor = FColor::Red;
	UPROPERTY(EditAnywhere)
	/** Custom tags that can be applied to the landscape for querying (i.E. fertility) */
	TArray<FName> LandscapeTags;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeLayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	/** If true, the layer will be applied after apply to ApplyToLandscape(), otherwise it will be applied on construction */
	bool bWaitForActivation;
	UPROPERTY(EditAnywhere)
	FLandscapeLayerData LayerData;

	void ApplyToLandscape() const;
	void ApplyLayerData(const FVector2D VertexLocation, float& OutHeightValue, FColor& OutVertexColorValue) const;
	const TArray<FName>& GetLandscapeTags() const { return LayerData.LandscapeTags; }

	FBox2D GetAffectedArea(bool bIncludeSmoothedArea) const
	{
		FVector Origin;
		FVector Extent;
		GetOwner()->GetActorBounds(false, Origin, Extent);
		FBox2D Result(FVector2D(Origin.X - Extent.X, Origin.Y - Extent.Y) - FVector2D(LayerData.SmoothingDistance),
		              FVector2D(Origin.X + Extent.X, Origin.Y + Extent.Y) + FVector2D(LayerData.SmoothingDistance));

		if (bIncludeSmoothedArea)
		{
			Result.Min -= FVector2D(LayerData.SmoothingDistance);
			Result.Max += FVector2D(LayerData.SmoothingDistance);
		}

		return Result;
	}

protected:
	virtual void BeginPlay() override;
};
