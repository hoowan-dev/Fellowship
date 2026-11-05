// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSWeaponStance.h"
#include "FSEquipmentComponent.generated.h"

class UStaticMeshComponent;

/**
 * Shows what the player's hands hold on the character, and whether those weapons are drawn or sheathed on the back.
 * The items themselves belong to the inventory, so this only ever follows it.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSEquipmentComponent();

	// The stance used while sheathed, when nothing is held, or when no rule matches what's held. Should use the NoWeapon animations.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UFSWeaponStance> UnarmedStance;

	// Which stance each pairing of weapons uses while drawn. The first match wins.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<FFSWeaponStanceRule> StanceRules;

	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName RightHandSocket = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName LeftHandSocket = TEXT("weapon_l");

	// On the back, for what the right hand holds
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName RightSheathSocket = TEXT("BackPack");

	// On the back, for what the left hand holds
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName LeftSheathSocket = TEXT("BackPack");

	// Where a mesh's grip sits relative to the right sheath socket, whose axes match the mesh's: +X is the character's left, +Y its front and +Z up. Meshes point along +Z from their grip.
	// The default rests the grip behind the right shoulder with the blade running down across the back. An item's Sheathed Transform is applied on top of this.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FTransform RightSheathOffset = FTransform(FRotator(-130.0, 0.0, 0.0), FVector(-16.0, -14.0, 18.0));

	// The same for the left sheath socket. The default mirrors the right, a little further out so that two weapons don't intersect.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FTransform LeftSheathOffset = FTransform(FRotator(130.0, 0.0, 0.0), FVector(16.0, -18.0, 18.0));

	// Follows this inventory's hands. Null shows nothing.
	void SetInventory(UFSInventoryComponent* NewInventory);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsDrawn() const { return bDrawn; }

	// Draws the sheathed weapons or sheathes the drawn ones
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void ToggleDrawn();

	// Moves the held weapons to the hands. Returns false, changing nothing, if nothing is held or the character is climbing.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool Draw();

	// Moves the held weapons to the back. They stay there until drawn again.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Sheathe();

	// The stance to animate with right now. Null only if Unarmed Stance isn't set.
	const UFSWeaponStance* GetCurrentStance() const;

	// Hides the weapons without changing where they are, such as while the camera is inside the character's head
	void SetMeshesHidden(bool bHidden);

	// Null until something has been shown in that hand
	FORCEINLINE UStaticMeshComponent* GetHandMesh(EFSHand Hand) const { return HandMeshes[static_cast<int32>(Hand)]; }

private:
	void HandleHandItemsChanged();

	// Matches the meshes to what's held and where it should be
	void RefreshMeshes();
	void RefreshMesh(EFSHand Hand);

	bool IsHoldingAnything() const;
	EFSWeaponType GetHeldWeaponType(EFSHand Hand) const;

	UPROPERTY(Transient)
	TObjectPtr<UFSInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HandMeshes[2];

	bool bDrawn = false;
	bool bMeshesHidden = false;
};
