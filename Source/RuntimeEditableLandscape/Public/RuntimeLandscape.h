// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGroundTypeData.h"
#include "GameFramework/Actor.h"
#include "RuntimeLandscape.generated.h"

class UTextureRenderTarget;
enum ELayerShape : uint8;
class URuntimeLandscapeComponent;
class ULandscapeLayerComponent;
class ALandscape;

class UProceduralMeshComponent;

USTRUCT(Blueprintable)
struct FGroundTypeBrushData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> BrushMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BrushMaterialInstance;
};

USTRUCT(Blueprintable)
/**
 * Caches data for up to 4 landscape ground layers
 */
struct FRuntimeLandscapeGroundTypeLayerSet
{
	GENERATED_BODY()

	FRuntimeLandscapeGroundTypeLayerSet()
	{
		// only allow 4 entries so they can be mapped to the RGBA channels
		GroundTypes = {nullptr, nullptr, nullptr, nullptr};
	}

	UPROPERTY(EditAnywhere)
	/**
	 * Render target used as material input.
	 * Has a higher resolution than the vertex resolution
	 */
	TObjectPtr<UTextureRenderTarget2D> LayerRenderTarget;
	UPROPERTY(EditAnywhere)
	/** Render target that has a pixel for every vertex on the landscape */
	TObjectPtr<UTextureRenderTarget2D> VertexWeightRenderTarget;
	UPROPERTY(EditAnywhere, meta = (EditFixedSize))
	TArray<const ULandscapeGroundTypeData*> GroundTypes;
	UPROPERTY()
	/**
	 * The weights for the layers
	 * each layer is stored in a separate color channel 
	 */
	TArray<FColor> VertexLayerWeights;

	TArray<FName> GetLayerNames() const;

	FLinearColor GetColorChannelForLayer(const ULandscapeGroundTypeData* GroundType) const
	{
		int32 LayerIndex = GroundTypes.IndexOfByKey(GroundType);
		if (ensure(LayerIndex != INDEX_NONE))
		{
			switch (LayerIndex)
			{
			case 0:
				return FLinearColor(1, 0, 0, 0);
			case 1:
				return FLinearColor(0, 1, 0, 0);
			case 2:
				return FLinearColor(0, 0, 1, 0);
			case 3:
				return FLinearColor(0, 0, 0, 1);
			default:
				checkNoEntry();
			}
		}

		return FLinearColor::Black;
	}

	int32 GetPixelIndexForCoordinates(FIntVector2 VertexCoords) const;
};

UCLASS(Blueprintable, BlueprintType)
class RUNTIMEEDITABLELANDSCAPE_API ARuntimeLandscape : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARuntimeLandscape();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision, meta=(ShowOnlyInnerProperties))
	FBodyInstance BodyInstance;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision)
	uint32 bGenerateOverlapEvents : 1;
	UPROPERTY(EditAnywhere, Category = "Performance")
	uint8 bUpdateCollision : 1 = 1;
	UPROPERTY(EditAnywhere, Category = "Performance")
	/**
	 * Whether landscape updates at runtime should affect navigation
	 * NOTE: Requires 'Navigation Mesh->Runtime->Runtime Generation->Dynamic' in the project settings
	 */
	uint8 bUpdateNavigation : 1 = 1;

	/**
	 * Adds a new layer to the landscape
	 * @param LayerToAdd The added landscape layer
	 */
	void AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd);

	void RemoveLandscapeLayer(const ULandscapeLayerComponent* Layer);
	TMap<const ULandscapeGroundTypeData*, float> GetGroundTypeLayerWeightsAtVertexCoordinates(int32 SectionIndex, int32 X, int32 Y) const;

	/** Get the amount of vertices in a single component */
	FORCEINLINE int32 GetTotalVertexAmountPerComponent() const
	{
		return VertexAmountPerComponent.X * VertexAmountPerComponent.Y;
	}

	FORCEINLINE const FIntVector2& GetVertexAmountPerComponent() const
	{
		return VertexAmountPerComponent;
	}

	FORCEINLINE const FVector2D& GetLandscapeSize() const { return LandscapeSize; }
	FORCEINLINE const FVector2D& GetMeshResolution() const { return MeshResolution; }
	FORCEINLINE const FVector2D& GetComponentAmount() const { return ComponentAmount; }
	FORCEINLINE const FVector2D& GetComponentResolution() const { return ComponentResolution; }
	FORCEINLINE float GetQuadSideLength() const { return QuadSideLength; }
	FORCEINLINE float GetParentHeight() const { return ParentHeight; }
	FORCEINLINE float GetAreaPerSquare() const { return AreaPerSquare; }
	FORCEINLINE const AInstancedFoliageActor* GetFoliageActor() const { return FoliageActor; }
	FORCEINLINE const TMap<TEnumAsByte<ELayerShape>, FGroundTypeBrushData>& GetGroundTypeBrushes() const
	{
		return GroundTypeBrushes;
	}

	const FRuntimeLandscapeGroundTypeLayerSet* TryGetLayerSetForGroundType(
		const ULandscapeGroundTypeData* GroundType) const
	{
		for (const FRuntimeLandscapeGroundTypeLayerSet& LayerSet : GroundLayerSets)
		{
			if (LayerSet.GroundTypes.Contains(GroundType))
			{
				return &LayerSet;
			}
		}

		return nullptr;
	}

	/**
	 * Get the ids for the sections contained in the specified area
	 * Sections are numbered like this (i.e. ComponentAmount = 4x4):
	 * 0	1	2	3	4
	 * 5	6	7	8	9
	 * 10	11	12	13	14
	 * 15	16	17	18	19
	 */
	TArray<URuntimeLandscapeComponent*> GetComponentsInArea(const FBox2D& Area) const;

	/**
	 * Get the grid coordinates of the specified component
	 * @param SectionIndex				The id of the component
	 * @param OutCoordinateResult	The coordinate result
	 */
	void GetComponentCoordinates(int32 SectionIndex, FIntVector2& OutCoordinateResult) const;
	/**
	 * Get the coordinates of the specified vertex within it's component
	 * @param VertexIndex				The id of the vertex
	 * @param OutCoordinateResult	The coordinate result
	 */
	void GetVertexCoordinatesWithinComponent(int32 VertexIndex, FIntVector2& OutCoordinateResult) const;
	/**
	 * Get the coordinates of the specified section vertex on the whole landscape
	 * @param SectionIndex				The id of the section
	 * @param SectionVertexX				The vertex X coordinate within the section
	 * @param SectionVertexY				The vertex Y coordinate within the section
	 * @param OutCoordinateResult	The coordinate result
	 */
	void GetVertexCoordinatesWithinLandscape(int32 SectionIndex, int32 SectionVertexX, int32 SectionVertexY,
	                                         FIntVector2& OutCoordinateResult) const;

	FVector GetOriginLocation() const;
	FBox2D GetComponentBounds(int32 SectionIndex) const;

protected:
	UPROPERTY(EditAnywhere)
	/** The base for scaling landscape height (8 bit?) */
	int32 HeightValueBits = 7;
	UPROPERTY(EditAnywhere)
	uint8 bCanEverAffectNavigation : 1 = 1;
	UPROPERTY(EditAnywhere)
	TObjectPtr<AInstancedFoliageActor> FoliageActor;
	UPROPERTY(EditAnywhere)
	/**
	* RenderTargets for the ground layers.
	* Stores each layer in a separate color channel
	* Since there are 4 channels per target (RGBA), 4 layers can be stored per render target
	*/
	TArray<FRuntimeLandscapeGroundTypeLayerSet> GroundLayerSets;
	UPROPERTY(EditAnywhere)
	float PaintLayerResolution = 0.01f;
	UPROPERTY(EditAnywhere)
	TMap<TEnumAsByte<ELayerShape>, FGroundTypeBrushData> GroundTypeBrushes;
	UPROPERTY(EditAnywhere)
	bool bBakeLayersOnBeginPlay = true;
	UPROPERTY()
	/** The area a single square occupies */
	float AreaPerSquare;
	UPROPERTY()
	FVector2D LandscapeSize = FVector2D(1000, 1000);
	UPROPERTY()
	FVector2D MeshResolution = FVector2D(10, 10);
	//TODO: Ensure MeshResolution is a multiple of ComponentAmount
	UPROPERTY()
	FVector2D ComponentAmount = FVector2D(2.0f, 2.0f);
	UPROPERTY()
	FVector2D ComponentResolution;
	UPROPERTY()
	TArray<TObjectPtr<URuntimeLandscapeComponent>> LandscapeComponents;
	UPROPERTY()
	float HeightScale = 1.0f;
	UPROPERTY()
	/** The side length of a single component in units (components are always squares) */
	float ComponentSize;
	UPROPERTY()
	FIntVector2 VertexAmountPerComponent;
	UPROPERTY()
	float QuadSideLength;
	UPROPERTY()
	float ParentHeight;

	bool bIsRebuilding;

	UFUNCTION(BlueprintCallable)
	void BakeLandscapeLayers();

	UFUNCTION()
	void HandleLandscapeLayerOwnerDestroyed(AActor* DestroyedActor);

	virtual void PostLoad() override;
	virtual void BeginPlay() override;

#if WITH_EDITORONLY_DATA

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<ALandscape> ParentLandscape;
	UPROPERTY(EditAnywhere, Category = "Lighting")
	uint8 bCastShadow : 1 = 1;
	UPROPERTY(EditAnywhere, Category = "Lighting", meta = (EditCondition = "bCastShadow"))
	uint8 bAffectDistanceFieldLighting : 1 = 1;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> LandscapeMaterial;
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebug;
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bEnableDebug", EditConditionHides))
	bool bDrawDebugCheckerBoard;
	UPROPERTY(EditAnywhere, Category = "Debug",
		meta = (EditCondition="bEnableDebug && !bDrawDebugCheckerBoard", EditConditionHides))
	bool bDrawIndexGreyScales;
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bEnableDebug", EditConditionHides))
	bool bShowComponentsWithHole;
	UPROPERTY(EditAnywhere, Category = "Debug",
		meta = (EditCondition = "bEnableDebug && bDrawDebugCheckerBoard", EditConditionHides))
	FColor DebugColor1 = FColor::Blue;
	UPROPERTY(EditAnywhere, Category = "Debug",
		meta = (EditCondition = "bEnableDebug && bDrawDebugCheckerBoard", EditConditionHides))
	FColor DebugColor2 = FColor::Emerald;
	UPROPERTY(EditAnywhere, Category = "Debug")
	TObjectPtr<UMaterial> DebugMaterial;

	UFUNCTION(BlueprintCallable)
	void InitializeFromLandscape();

	virtual void PreInitializeComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		if (EndPlayReason == EEndPlayReason::Type::EndPlayInEditor)
		{
			BakeLandscapeLayers();
		}

		Super::EndPlay(EndPlayReason);
	}

#endif
};
