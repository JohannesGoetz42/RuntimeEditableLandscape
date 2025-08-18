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

class FGenerateVerticesWorker : public IQueuedWork
{
	friend class URuntimeLandscapeRebuildManager;

public:
	FGenerateVerticesWorker(URuntimeLandscapeRebuildManager* RebuildManager);
	virtual ~FGenerateVerticesWorker() override;

private:
	TObjectPtr<URuntimeLandscapeRebuildManager> RebuildManager;
	FVector2D UV1Offset;

	void QueueWork(FVector2D InUV1Offset)
	{
		UV1Offset = InUV1Offset;
		RebuildManager->ThreadPool->AddQueuedWork(this);
	}

	virtual void DoThreadedWork() override;

	virtual void Abandon() override
	{
		RebuildManager->CancelRebuild();
	}
};
