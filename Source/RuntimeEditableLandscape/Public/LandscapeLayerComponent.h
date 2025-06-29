// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LandscapeLayerComponent.generated.h"


class URuntimeLandscapeComponent;
class ARuntimeLandscape;

UENUM()
enum ESmoothingDirection : uint8
{
	SD_Inwards UMETA(DisplayName = "Inwards"),
	SD_Outwards UMETA(DisplayName = "Outwards"),
	SD_Center UMETA(DisplayName = "Center")
};

UENUM()
enum ELandscapeLayerType : uint8
{
	LLT_None UMETA(DisplayName = "None"),
	LLT_Height UMETA(DisplayName = "Height"),
	LLT_VertexColor UMETA(DisplayName = "Vertex color"),
	LLT_Hole UMETA(DisplayName = "Hole")
};

UENUM()
enum ELayerShape : uint8
{
	HS_Box UMETA(DisplayName = "Box"),
	HS_Sphere UMETA(DisplayName = "Sphere")
};

USTRUCT()
struct FLandscapeLayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float FloatValue = 0.0f;
	UPROPERTY(EditAnywhere)
	FColor ColorValue = FColor::White;
	UPROPERTY(EditAnywhere)
	bool bBoolValue = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeLayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = 0.0f))
	/** Whether smoothing is applied inwards or outwards*/
	TEnumAsByte<ESmoothingDirection> SmoothingDirection = ESmoothingDirection::SD_Inwards;
	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = 0.0f))
	/** The distance in which the layer effect fades out */
	float SmoothingDistance = 200.0f;
	UPROPERTY(EditDefaultsOnly)
	/** If true, the layer will be applied after apply to ApplyToLandscape(), otherwise it will be applied on construction */
	bool bWaitForActivation;

	FORCEINLINE ELayerShape GetShape() const { return Shape; }
	FORCEINLINE float GetRadius() const { return Radius; }
	FORCEINLINE const FVector& GetExtent() const { return Extent; }
	FORCEINLINE const FBox2D& GetBoundingBox() const { return BoundingBox; }

	void ApplyToLandscape();
	bool IsAffectedByLayer(FVector2D Location) const;
	void ApplyLayerData(int32 VertexIndex, URuntimeLandscapeComponent* LandscapeComponent, float& OutHeightValue,
	                    FColor& OutVertexColorValue) const;

	void SetHeightLayerData(float HeightData)
	{
		FLandscapeLayerData& LayerDataWithType = LayerData.FindOrAdd(ELandscapeLayerType::LLT_Height);
		LayerDataWithType.FloatValue = HeightData;
	}

	void SetVertexColorLayerData(FColor ColorValue)
	{
		FLandscapeLayerData& LayerDataWithType = LayerData.FindOrAdd(ELandscapeLayerType::LLT_VertexColor);
		LayerDataWithType.ColorValue = ColorValue;
	}

	void SetHoleLayerData(bool bIsHole)
	{
		FLandscapeLayerData& LayerDataWithType = LayerData.FindOrAdd(ELandscapeLayerType::LLT_Hole);
		LayerDataWithType.bBoolValue = bIsHole;
	}

	void SetBoundsComponent(UPrimitiveComponent* NewBoundsComponent);

	const TMap<TEnumAsByte<ELandscapeLayerType>, FLandscapeLayerData>& GetLayerData() const { return LayerData; }

protected:
	UPROPERTY(EditAnywhere)
	TSet<TObjectPtr<ARuntimeLandscape>> AffectedLandscapes;
	UPROPERTY(EditAnywhere)
	TMap<TEnumAsByte<ELandscapeLayerType>, FLandscapeLayerData> LayerData;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "BoundsComponent == nullptr"))
	/**
	 * The shape of the layer
	 * Only relevant if no BoundsComponent is set
	 */
	TEnumAsByte<ELayerShape> Shape = ELayerShape::HS_Box;
	UPROPERTY(EditAnywhere,
		meta = (EditCondition = "BoundsComponent == nullptr && Shape == ELayerShape::HS_Sphere", EditConditionHides))
	float Radius = 100.0f;
	UPROPERTY(EditAnywhere,
		meta = (EditCondition = "BoundsComponent == nullptr && Shape == ELayerShape::HS_Box", EditConditionHides))
	FVector Extent = FVector(100.0f);
	UPROPERTY()
	/**
	 * Optional component that defines the affected area.
	 * Overrides the Shape
	 */
	TObjectPtr<UPrimitiveComponent> BoundsComponent;

	/** The axis aligned bounding box */
	FBox2D BoundingBox = FBox2D();
	/** The affected box without smoothing */
	FBox2D InnerBox = FBox2D();
	float BoundsSmoothingOffset = 0.0f;
	float InnerSmoothingOffset = 0.0f;

	/**
	 * Try to calculate the smoothing distance
	 * @param OutSmoothingFactor the resulting smoothing factor
	 * @param Location the location to calculate the distance to  
	 * @return true if the location is affected
	 */
	bool TryCalculateSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location) const;
	bool TryCalculateBoxSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location, FVector2D Origin) const;
	bool TryCalculateSphereSmoothingFactor(float& OutSmoothingFactor, const FVector2D& Location,
	                                       FVector2D Origin) const;

	void HandleBoundsChanged(USceneComponent* SceneComponent, EUpdateTransformFlags UpdateTransformFlags,
	                         ETeleportType Teleport);
	void RemoveFromLandscapes();
	void UpdateShape();

	UFUNCTION()
	void HandleOwnerDestroyed(AActor* DestroyedActor) { DestroyComponent(); }

	virtual void BeginPlay() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

#if WITH_EDITORONLY_DATA
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
