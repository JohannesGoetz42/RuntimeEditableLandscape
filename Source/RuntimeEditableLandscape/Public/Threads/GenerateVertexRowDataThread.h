// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeLandscapeComponent.h"
#include "UObject/Object.h"

/**
 * Thread that is used to create data for a single vertex row
*/

class URuntimeLandscapeComponent;
class ARuntimeLandscape;
struct FGrassVariety;

struct FLandscapeGrassVertexData
{
	FGrassVariety GrassVariety;
	TArray<FTransform> InstanceTransforms;
};

struct FLandscapeVertexData
{
	// Vertices
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0Coords;
	TArray<FVector2D> UV1Coords;

	// Additional data
	TArray<FLandscapeGrassVertexData> GrassData;
};

class FGenerateVertexRowDataThread : public FRunnable
{
	friend class URuntimeLandscapeComponent;

public:
	FGenerateVertexRowDataThread(URuntimeLandscapeComponent* LandscapeComponent,
	                             const FVector2D& ComponentResolution,
	                             const FVector2D& UV1Scale, const FVector2D& UV1Offset, float VertexDistance,
	                             float UVIncrement, int32 VertexStartIndex, int32 YCoordinate);

	virtual ~FGenerateVertexRowDataThread() override;

	FORCEINLINE const FLandscapeVertexData& GetVertexData() const { return VertexData; }

private:
	FLandscapeVertexData VertexData;
	URuntimeLandscapeComponent* LandscapeComponent;
	FVector2D ComponentResolution;
	FVector2D UV1Scale;
	FVector2D UV1Offset;
	float VertexDistance;
	float UVIncrement;
	int32 YCoordinate;
	int32 VertexIndex;

	virtual uint32 Run() override;

	void GenerateVertexData(const FVector& VertexLocation, int32 X, int32 Y);
	void GenerateGrassDataForVertex(const FVector& VertexLocation, int32 X, int32 Y);
	void GenerateGrassTransformsAtVertex(const ULandscapeGrassType* SelectedGrass,
	                                     const FVector& VertexRelativeLocation, float Weight);
	void GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation) const;
	void GetRandomGrassLocation(const FVector& VertexRelativeLocation, FVector& OutGrassLocation) const;
	void GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const;

	FRunnableThread* Thread;
};
