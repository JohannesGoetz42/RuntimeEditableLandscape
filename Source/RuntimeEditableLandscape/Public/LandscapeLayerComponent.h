// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LandscapeLayerComponent.generated.h"


class ARuntimeLandscape;

UENUM()
enum ESmoothingDirection
{
	SD_Inwards UMETA(DisplayName = "Inwards"),
	SD_Outwards UMETA(DisplayName = "Outwards"),
	SD_Center UMETA(DisplayName = "Center")
};

USTRUCT()
struct FLandscapeLayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = 0.0f))
	/** Whether smoothing is applied inwards or outwards*/
	TEnumAsByte<ESmoothingDirection> SmoothingDirection = SD_Inwards;
	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = 0.0f))
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
	UPROPERTY()
	/** Optional component that overrides the bounds for the affected area */
	TObjectPtr<UPrimitiveComponent> BoundsOverride;
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

	FORCEINLINE const TArray<FName>& GetLandscapeTags() const { return LayerData.LandscapeTags; }

	/** Get the affected area */
	FBox2D GetAffectedArea(bool bIncludeSmoothing) const;

	void ApplyToLandscape() const;
	void ApplyLayerData(const FVector2D VertexLocation, float& OutHeightValue, FColor& OutVertexColorValue) const;

protected:
	virtual void BeginPlay() override;
};
