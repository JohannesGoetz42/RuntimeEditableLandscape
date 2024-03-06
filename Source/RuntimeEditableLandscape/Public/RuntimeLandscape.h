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
	FVector2D LandscapeSize = FVector2D(100, 100);
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1, FixedIncrement = 1))
	FVector2D MeshResolution = FVector2D(5, 5);
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1, FixedIncrement = 1))
	FVector2D SectionAmount = FVector2D(10.0f, 10.0f);
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProceduralMeshComponent> LandscapeMesh;

	void PrepareHeightValues(TArray<float>& OutHeightValues) const;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

#if WITH_EDITOR
	inline static const TSet<FString> MeshGenerationAffectingProperties = {"LandscapeSize", "MeshResolution"};
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
	TArray<int32> GetSectionIdsInArea(const FBox2D& Area) const;
	
	void GenerateMesh() const;
};
