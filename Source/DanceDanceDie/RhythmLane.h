// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RhythmLane.generated.h"

class USphereComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class ELane : uint8 { Left, Down, Up, Right };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHit, ELane, Lane);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMiss, ELane, Lane, int32, MissCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJumpscare);

UCLASS()
class DANCEDANCEDIE_API ARhythmLane : public AActor
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
	UPROPERTY(BlueprintAssignable) FOnJumpscare OnJumpscare;

	// call on key press, true if a note was in the hit zone
	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	bool Judge(ELane Lane);

	// call when a note slips past the hit zone without being pressed
	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	void RegisterMiss(ELane Lane);

	UFUNCTION(BlueprintCallable, Category = "Rhythm")
	void ResetSong();
};