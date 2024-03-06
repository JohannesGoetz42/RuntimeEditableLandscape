// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeLandscape.generated.h"

class UProceduralMeshComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API ARuntimeLandscape : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARuntimeLandscape();

protected:
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1))
	FVector2D LandscapeSize = FVector2D(1000, 1000);
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1, FixedIncrement = 1))
	FVector2D MeshResolution = FVector2D(10, 10);
	//TODO: Ensure MeshResolution is a multiple of SectionAmount
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1, FixedIncrement = 1))
	FVector2D SectionAmount = FVector2D(2.0f, 2.0f);
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProceduralMeshComponent> LandscapeMesh;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebug;
	UPROPERTY(EditAnywhere, Category = "Debug")
	FColor DebugColor1 = FColor::Blue;
	UPROPERTY(EditAnywhere, Category = "Debug")
	FColor DebugColor2 = FColor::Emerald;
	UPROPERTY(EditAnywhere, Category = "Debug")
	TObjectPtr<UMaterial> DebugMaterial;
#endif

	void PrepareHeightValues(TArray<float>& OutHeightValues) const;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Get the ids for the secions contained in the specified area
	 * Sections are numbered like this (i.e SectionAmount = 4x4):
	 * 0	1	2	3	4
	 * 5	6	7	8	9
	 * 10	11	12	13	14
	 * 15	16	17	18	19
	 */
	TArray<int32> GetSectionsInArea(const FBox2D& Area) const;
	
	/**
	 * Get the grid coordinates of the 
	 * @param SectionId				The id of the section
	 * @param OutCoordinateResult	The coordinate result
	 */
	void GetSectionCoordinates(int32 SectionId, FIntVector2& OutCoordinateResult) const;

	FBox2D GetSectionBounds(int32 SectionIndex) const;
	void GenerateMesh() const;
	void GenerateSection(int32 SectionIndex) const;
};
