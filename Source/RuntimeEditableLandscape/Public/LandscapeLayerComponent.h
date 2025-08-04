// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LandscapeLayerComponent.generated.h"


class ULandscapeLayerDataBase;
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
enum ELayerShape : uint8
{
	HS_Default UMETA(DisplayName = "Default"),
	HS_Box UMETA(DisplayName = "Box"),
	HS_Round UMETA(DisplayName = "Round")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeLayerComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class ARuntimeLandscape;

public:
	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = 0.0f))
	/** Whether smoothing is applied inwards or outwards*/
	TEnumAsByte<ESmoothingDirection> SmoothingDirection = ESmoothingDirection::SD_Inwards;
	UPROPERTY(EditAnywhere, Category = "Smoothing", meta = (ClampMin = 0.0f))
	/** The distance in which the layer effect fades out */
	float SmoothingDistance = 200.0f;
	UPROPERTY(EditDefaultsOnly)
	/** If true, the layer will be applied after apply to ApplyToLandscape(), otherwise it will be applied on construction
	 *
	 * Call @anchor ApplyToLandscape to activate it in your code!
	 */
	bool bWaitForActivation;

	FORCEINLINE ELayerShape GetShape() const { return Shape; }
	FORCEINLINE float GetRadius() const { return Radius; }
	FORCEINLINE const FVector& GetExtent() const { return Extent; }
	FORCEINLINE const FBox2D& GetBoundingBox() const { return BoundingBox; }
	FORCEINLINE const TSet<const ULandscapeLayerDataBase*>& GetLayerData() const { return Layers; }

	void ApplyToLandscape();
	bool IsAffectedByLayer(FVector2D Location) const;
	void ApplyLayerData(int32 VertexIndex, URuntimeLandscapeComponent* LandscapeComponent, float& OutHeightValue,
	                    FColor& OutVertexColorValue) const;
	void SetBoundsComponent(UPrimitiveComponent* NewBoundsComponent);

protected:
	UPROPERTY(EditAnywhere)
	TSet<TObjectPtr<ARuntimeLandscape>> AffectedLandscapes;
	UPROPERTY(EditAnywhere, Instanced)
	TSet<const ULandscapeLayerDataBase*> Layers;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "BoundsComponent == nullptr"))
	/**
	 * The shape of the layer
	 * Only relevant if no BoundsComponent is set
	 */
	TEnumAsByte<ELayerShape> Shape = ELayerShape::HS_Box;
	UPROPERTY(EditAnywhere,
		meta = (EditCondition = "BoundsComponent == nullptr && Shape == ELayerShape::HS_Round", EditConditionHides))
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

	virtual void OnRegister() override
	{
		Super::OnRegister();		
		UpdateShape();
	}
	
#if WITH_EDITORONLY_DATA
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
