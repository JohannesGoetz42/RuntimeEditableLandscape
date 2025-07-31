// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeLandscape.generated.h"

class URuntimeLandscapeComponent;
class ULandscapeLayerComponent;
class ALandscape;

class UProceduralMeshComponent;

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
	 * @param bForceRebuild Whether the landscape should be updated immediately (see UpdateStaleSections()). This allows delaying the update for performance reasons
	 */
	void AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd, bool bForceRebuild = true);

	void RemoveLandscapeLayer(ULandscapeLayerComponent* Layer, bool bForceRebuild = true);

	/** Get the amount of vertices in a single component */
	FORCEINLINE int32 GetTotalVertexAmountPerComponent() const
	{
		return VertexAmountPerComponent.X * VertexAmountPerComponent.Y;
	}

	FORCEINLINE const FIntVector2& GetVertexAmountPerComponent() const
	{
		return VertexAmountPerComponent;
	}

	/** Get the size of a single section */
	FORCEINLINE const FVector2D& GetLandscapeSize() const { return LandscapeSize; }
	FORCEINLINE const FVector2D& GetMeshResolution() const { return MeshResolution; }
	FORCEINLINE const FVector2D& GetComponentAmount() const { return ComponentAmount; }
	FORCEINLINE const FVector2D& GetComponentResolution() const { return ComponentResolution; }
	FORCEINLINE float GetQuadSideLength() const { return QuadSideLength; }
	FORCEINLINE float GetParentHeight() const { return ParentHeight; }
	FORCEINLINE const AInstancedFoliageActor* GetFoliageActor() const { return FoliageActor; }

	/**
	 * Get the ids for the sections contained in the specified area
	 * Sections are numbered like this (i.e ComponentAmount = 4x4):
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

	FBox2D GetComponentBounds(int32 SectionIndex) const;

protected:
	UPROPERTY(EditAnywhere)
	/** The base for scaling landscape height (8 bit?) */
	int32 HeightValueBits = 7;
	UPROPERTY(EditAnywhere)
	uint8 bCanEverAffectNavigation : 1 = 1;
	UPROPERTY(EditAnywhere)
	TObjectPtr<AInstancedFoliageActor> FoliageActor;
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

	UFUNCTION()
	void HandleLandscapeLayerOwnerDestroyed(AActor* DestroyedActor);


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
	bool bDrawIndexGreyscales;
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
	UPROPERTY(EditAnywhere)
	float PaintLayerResolution = 0.01f; 
	UPROPERTY(EditAnywhere)
	TMap<FName, UTextureRenderTarget2D*> LanscapePaintLayers;

	UFUNCTION(BlueprintCallable)
	void BakeLandscapeLayers();
	UFUNCTION(BlueprintCallable)
	void InitializeFromLandscape();
	
	virtual void PreInitializeComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
