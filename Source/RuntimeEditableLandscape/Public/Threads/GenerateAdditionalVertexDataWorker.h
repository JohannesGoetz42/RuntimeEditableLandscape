// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeLandscapeRebuildManager.h"

struct FGrassVariety;
class ULandscapeGrassType;
class URuntimeLandscapeRebuildManager;
/**
 * Runner that generates additional vertex info
 * run when all vertices are generated in the RLRS_BuildAdditionalData stage
 */
class RUNTIMEEDITABLELANDSCAPE_API FGenerateAdditionalVertexDataWorker : public IQueuedWork
{
	friend class URuntimeLandscapeRebuildManager;

public:
	FGenerateAdditionalVertexDataWorker(URuntimeLandscapeRebuildManager* RebuildManager);
	~FGenerateAdditionalVertexDataWorker();

private:
	int32 YCoordinate = 0;
	int32 StartIndex = 0;
	FVector2D UV1Offset = FVector2D();
	TObjectPtr<URuntimeLandscapeRebuildManager> RebuildManager;

	void GenerateGrassDataForVertex(const int32 VertexIndex, int32 X);
	void GenerateGrassTransformsAtVertex(const FGrassTypeSettings* SelectedGrass, const int32 VertexIndex,
	                                        float Weight) const;
	void GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation) const;
	void GetRandomGrassLocation(const FVector& VertexRelativeLocation, FVector& OutGrassLocation) const;
	void GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const;

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
};
