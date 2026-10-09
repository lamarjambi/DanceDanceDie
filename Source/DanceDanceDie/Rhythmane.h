// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rhythmane.generated.h"

class USphereComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class ELane : uint8 { Left, Down, Up, Right };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHit, ELane, Lane);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMiss, ELane, Lane, int32, MissCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJumpscare);

UCLASS()
class YOURPROJECT_API ARhythmLane : public AActor   // keep whatever _API name unreal made
{
	GENERATED_BODY()

public:
	ARhythmLane();

	// fill these in bp beginplay w/ make array, order is left down up right
	UPROPERTY(BlueprintReadWrite, Category = "Rhythm")
	TArray<TObjectPtr<USplineComponent>> Lanes;

	UPROPERTY(BlueprintReadWrite, Category = "Rhythm")
	TArray<TObjectPtr<USphereComponent>> HitSpheres;

	// set this to bp_note1 in class defaults
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	TSubclassOf<AActor> NoteClass;

	// how many misses before the jumpscare
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
	int32 MissThreshold = 10;

	UPROPERTY(BlueprintReadOnly, Category = "Rhythm")
	int32 MissCount = 0;

	// teammates bind to these
	UPROPERTY(BlueprintAssignable) FOnHit OnHit;
	UPROPERTY(BlueprintAssignable) FOnMiss OnMiss;
	UPROPERTY(BlueprintAssignable) FOnJumpscare