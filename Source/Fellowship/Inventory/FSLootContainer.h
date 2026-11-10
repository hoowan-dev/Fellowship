// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/FSInteractionVolume.h"
#include "FSLootContainer.generated.h"

class AFSItemPickup;
class UAnimSequence;
class UBoxComponent;
class UFSItemDefinition;
class USkeletalMeshComponent;

/**
 * A chest or similar the player can open and take items out of. Interacting with it inside its area opens it: it plays OpenAnimation,
 * puts its Items inside it as pickups hidden from the world and, once the lid is up, opens the player's inventory, where they're in the nearby list to take.
 * When the inventory closes, or the player walks out of its area, whatever wasn't taken goes back into Items and it plays CloseAnimation, ready to be opened again.
 */
UCLASS()
class FELLOWSHIP_API AFSLootContainer : public AFSInteractionVolume
{
	GENERATED_BODY()

public:
	AFSLootContainer();

	// What's inside, which appears in it when it's opened. Taking an item removes it from here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot Container")
	TArray<TObjectPtr<UFSItemDefinition>> Items;

	// Played when it opens. The inventory opens once it has finished.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot Container")
	TObjectPtr<UAnimSequence> OpenAnimation;

	// Played when it closes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot Container")
	TObjectPtr<UAnimSequence> CloseAnimation;

	// What the items appear as while it's open. FSItemPickup itself is used if this isn't set.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot Container")
	TSubclassOf<AFSItemPickup> ItemPickupClass;

	// Whether it's open, from being interacted with until the inventory closes
	UFUNCTION(BlueprintPure, Category = "Loot Container")
	bool IsOpen() const { return bOpen; }

	// Opens it for the player, unless it's already open
	virtual void Interact_Implementation(AFSPlayerController* PlayerController) override;

	// Puts what wasn't taken back into Items and closes the lid. Does nothing if it isn't open.
	UFUNCTION(BlueprintCallable, Category = "Loot Container")
	void Close();

	// The items inside it while it's open
	TArray<AFSItemPickup*> GetItemPickups() const;

	// None while it's open
	virtual FFSItemText GetInteractionLabel() const override;
	virtual FText GetInteractionVerb(const UFSInventoryComponent* Inventory) const override;

	FORCEINLINE USkeletalMeshComponent* GetMesh() const { return Mesh; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

private:
	// Lays each of Items out across the bottom of ContentsArea as a pickup this owns, and empties Items
	void SpawnItemPickups();

	// Opens the player's inventory once the lid is up, closing again if it can't
	void OpenInventory();

	void StopWatchingInventory();

	// The container itself, which blocks the player. Its area is the box around it where interact works.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot Container", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;

	// Where the items lie while it's open: spread in rows across the bottom of this box. Only shown in the editor.
	UPROPERTY(VisibleAnywhere, Category = "Loot Container")
	TObjectPtr<UBoxComponent> ContentsArea;

	// The player it's open for
	TWeakObjectPtr<AFSPlayerController> Looter;
	FDelegateHandle InventoryClosedHandle;
	FTimerHandle OpenInventoryTimer;

	bool bOpen = false;
};
