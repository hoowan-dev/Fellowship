// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSWeaponStance.h"
#include "FSEquipmentComponent.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Shows what the player's hands hold on the character, whether those weapons are drawn or sheathed on the back, the backpack being worn with the quick slots' items hanging off it, and the armor being worn.
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

	// Where a mesh sits relative to the right hand's socket. Meshes point along +Z from their grip, which would run through the palm, so the default rolls the blade around to point out of the fist.
	// An item's Hand Equipped Transform is applied on top of this.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FTransform RightHeldOffset = FTransform(FRotator(0.0, 0.0, -90.0));

	// The same for the left hand's socket, whose weapon bone has its axes flipped from the right's. The roll alone would point the blade out of the fist with its edge backwards,
	// so the mesh is also turned 180 degrees about the blade's length. As a rotator, that's a yaw of 180 and a roll of the opposite sign.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FTransform LeftHeldOffset = FTransform(FRotator(0.0, 180.0, -90.0));

	// On the back, for what the right hand holds
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName RightSheathSocket = TEXT("BackPack");

	// On the back, for what the left hand holds
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName LeftSheathSocket = TEXT("BackPack");

	// Where a mesh's grip sits relative to the right sheath socket, whose axes match the mesh's: +X is the character's left, +Y its front and +Z up. Meshes point along +Z from their grip.
	// The default rests the grip behind the right shoulder with the blade running down across the back. An item's Hand Stored Transform is applied on top of this.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FTransform RightSheathOffset = FTransform(FRotator(-130.0, 0.0, 0.0), FVector(-16.0, -14.0, 18.0));

	// The same for the left sheath socket. The default mirrors the right, a little further out so that two weapons don't intersect.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FTransform LeftSheathOffset = FTransform(FRotator(130.0, 0.0, 0.0), FVector(16.0, -18.0, 18.0));

	// Where the worn backpack's mesh goes, with no offset because backpack meshes are made with their pivot at this bone.
	// The character's mesh has a backpack of its own skinned to this bone, which is hidden so that only the worn one shows.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName BackpackBone = TEXT("BackpackBone");

	// What head and face armor's meshes attach to. All of the character's own head and face are skinned to this bone, so hiding it is what removes them.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName HeadBone = TEXT("head");

	// What body armor's mesh attaches to, unless it's a skeletal mesh
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName BodyArmorBone = TEXT("spine_03");

	// The top of the character's body in the skeleton. Hiding it is what removes the body, with everything but the head going with it.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Sockets")
	FName BodyBone = TEXT("pelvis");

	// How far a long item hanging off the worn backpack can reach below it. The default stops just short of the floor when standing.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Swinging", meta = (ClampMin = "0.0", Units = cm))
	float HungItemMaxDrop = 20.0f;

	// How many degrees sheathed weapons and hung items swing back for each cm per second they're moving at
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Swinging", meta = (ClampMin = "0.0"))
	float SwingPerSpeed = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Swinging", meta = (ClampMin = "0.0", ClampMax = "60.0", Units = deg))
	float MaxSwing = 25.0f;

	// How hard they're pulled towards the angle their speed calls for. Higher swings faster.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Swinging", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float SwingStiffness = 60.0f;

	// How quickly their swinging dies down. Lower swings back and forth more times when the character stops.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Swinging", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float SwingDamping = 4.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Follows this inventory's hands, quick slots and backpack. Null shows nothing.
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

	// Hides the weapons, backpack, what hangs off it and what's worn on the head and face without changing where they are, such as while the camera is inside the character's head.
	// Body armor stays, because it's all there is of the character's body when it replaces it.
	void SetMeshesHidden(bool bHidden);

	// Null until something has been shown in that hand
	FORCEINLINE UStaticMeshComponent* GetHandMesh(EFSHand Hand) const { return HandMeshes[static_cast<int32>(Hand)]; }

	// Null until a backpack has been worn
	FORCEINLINE UStaticMeshComponent* GetBackpackMesh() const { return BackpackMesh; }

	// Null until something has hung on that side of the backpack
	FORCEINLINE UStaticMeshComponent* GetHungItemMesh(EFSQuickSlot QuickSlot) const { return HungItemMeshes.Get(QuickSlot); }

	// Null until armor with a static mesh has been worn in that slot
	FORCEINLINE UStaticMeshComponent* GetArmorMesh(EFSArmorType ArmorType) const { return ArmorMeshes[static_cast<int32>(ArmorType)]; }

	// Null until armor with a skeletal mesh has been worn in that slot
	FORCEINLINE UPoseableMeshComponent* GetSkeletalArmorMesh(EFSArmorType ArmorType) const { return SkeletalArmorMeshes[static_cast<int32>(ArmorType)]; }

private:
	// A mesh that swings about a point as what it's attached to moves
	struct FSwing
	{
		// Where the mesh sits at rest, and the point it swings about, relative to what it's attached to
		FTransform Rest;
		FVector Pivot = FVector::ZeroVector;

		// In degrees, along X and Y of what it's attached to
		FVector2D Angle = FVector2D::ZeroVector;
		FVector2D AngleSpeed = FVector2D::ZeroVector;

		FVector LastAttachLocation = FVector::ZeroVector;
		bool bHasLastAttachLocation = false;

		void Settle();
	};

	void UpdateSwing(FSwing& Swing, UStaticMeshComponent* Mesh, float DeltaTime, double StiffnessScale) const;

	void HandleHandItemsChanged();

	// Matches the meshes to what's held and worn, and where they should be
	void RefreshMeshes();
	void RefreshMesh(EFSHand Hand);
	void RefreshBackpackMesh();
	void RefreshArmorMeshes();
	void RefreshArmorMesh(EFSArmorType ArmorType, USkeletalMeshComponent& CharacterMesh);

	// Hides the parts of the character that what's worn takes the place of, and shows them again once it's gone. Has to be what changes the character's hidden bones last, because any other change undoes keeping the head when the body is hidden.
	void RefreshHiddenBones(USkeletalMeshComponent& CharacterMesh);

	// A mesh to show an item on the character
	UStaticMeshComponent* CreateAttachedMesh(USkeletalMeshComponent* CharacterMesh) const;

	// Whether what's worn in a slot takes the place of its part of the character
	bool IsArmorReplacingPart(EFSArmorType ArmorType) const;

	bool IsHoldingAnything() const;
	EFSWeaponType GetHeldWeaponType(EFSHand Hand) const;

	UPROPERTY(Transient)
	TObjectPtr<UFSInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HandMeshes[2];

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BackpackMesh;

	UPROPERTY(Transient)
	FFSHungItemMeshes HungItemMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ArmorMeshes[3];

	// These copy the character's pose rather than following it as a leader, because a follower also takes on its leader's hidden bones, and the body armor has to show where the character's body is hidden
	UPROPERTY(Transient)
	TObjectPtr<UPoseableMeshComponent> SkeletalArmorMeshes[3];

	FSwing HandSwings[2];
	FSwing HungItemSwings[2];

	bool bDrawn = false;
	bool bMeshesHidden = false;

	// Whether the head or body bones are hidden on armor's behalf, to know when to show them again
	bool bHeadReplaced = false;
	bool bBodyReplaced = false;
};
