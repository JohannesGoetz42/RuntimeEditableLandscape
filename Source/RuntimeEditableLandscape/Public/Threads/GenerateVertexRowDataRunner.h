// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeLandscapeRebuildManager.h"
#include "UObject/Object.h"

/**
 * Thread that is used to create data for a single vertex row
*/

class URuntimeLandscapeComponent;
class ARuntimeLandscape;
struct FGrassVariety;

class FGenerateVertexRowDataRunner : public IQueuedWork
{
	friend class URuntimeLandscapeRebuildManager;

public:
	FGenerateVertexRowDataRunner(URuntimeLandscapeRebuildManager* RebuildManager);
	virtual ~FGenerateVertexRowDataRunner() override;

private:
	int32 YCoordinate = 0;
	int32 StartIndex = 0;
	FVector2D UV1Offset = FVector2D();

	TObjectPtr<URuntimeLandscapeRebuildManager> RebuildManager;

	void QueueWork(int32 Y, int32 VertexStartIndex, const FVector2D& InUV1Offset)
	{
		YCoordinate = Y;
		StartIndex = VertexStartIndex;
		UV1Offset = InUV1Offset;
		RebuildManager->ThreadPool->AddQueuedWork(this);
	}

	virtual void DoThreadedWork() override;
	virtual void Abandon() override
	{
		RebuildManager->CancelRebuild();
	}

	void GenerateGrassDataForVertex(const FVector& VertexLocation, int32 X, int32 Y);
	void GenerateGrassTransformsAtVertex(const ULandscapeGrassType* SelectedGrass,
	                                     const FVector& VertexRelativeLocation, float Weight);
	void GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation) const;
	void GetRandomGrassLocation(const FVector& VertexRelativeLocation, FVector& OutGrassLocation) const;
	void GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const;
};
