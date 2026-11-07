// Copyright HoowanDev 2026 all rights reserved.


#include "FSPlayerController.h"
#include "Character/FSCharacter.h"
#include "Character/FSItemUseComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerState.h"
#include "TimerManager.h"

namespace
{
	// How far through the camera blend the worn pack is swapped for the held one. Late on opening and early
	// on closing, so the swap happens while the camera is close to the head and the player can't see it.
	constexpr float OpeningSwapFraction = 0.6f;
	constexpr float ClosingSwapFraction = 0.4f;
}

void AFSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only the owning client has a local player to receive input
	if (!IsLocalController() || !DefaultMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AFSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InventoryMidpointTimer);
	GetWorldTimerManager().ClearTimer(InventoryFinishedTimer);
	InventoryViewState = EFSInventoryViewState::Closed;

	if (InventoryRig)
	{
		InventoryRig->Destroy();
		InventoryRig = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AFSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput && ToggleInventoryAction)
	{
		EnhancedInput->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AFSPlayerController::ToggleInventory);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory can't be toggled: EnhancedInput=%d ToggleInventoryAction=%s"), EnhancedInput != nullptr, *GetNameSafe(ToggleInventoryAction));
	}

	if (EnhancedInput && InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AFSPlayerController::Interact);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Items can't be picked up: EnhancedInput=%d InteractAction=%s"), EnhancedInput != nullptr, *GetNameSafe(InteractAction));
	}

	if (EnhancedInput && InventorySelectAction)
	{
		EnhancedInput->BindAction(InventorySelectAction, ETriggerEvent::Started, this, &AFSPlayerController::SelectAtInventoryCursor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory items can't be selected: EnhancedInput=%d InventorySelectAction=%s"), EnhancedInput != nullptr, *GetNameSafe(InventorySelectAction));
	}

	if (EnhancedInput && InventoryRotateAction)
	{
		EnhancedInput->BindAction(InventoryRotateAction, ETriggerEvent::Started, this, &AFSPlayerController::RotateCarriedInventoryItem);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Carried items can't be rotated: EnhancedInput=%d InventoryRotateAction=%s"), EnhancedInput != nullptr, *GetNameSafe(InventoryRotateAction));
	}
}

UFSInventoryComponent* AFSPlayerController::GetInventory() const
{
	const AFSPlayerState* FSPlayerState = GetPlayerState<AFSPlayerState>();
	return FSPlayerState ? FSPlayerState->GetInventory() : nullptr;
}

const FFSInventoryItem* AFSPlayerController::GetHoveredInventoryItem() const
{
	const UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !InventoryRig || InventoryViewState != EFSInventoryViewState::Open)
	{
		return nullptr;
	}

	if (InventoryRig->GetHoveredHandSlot() != INDEX_NONE)
	{
		const FFSInventoryItem& HandItem = Inventory->GetHandItem(AFSInventoryRig::GetHandOfSlot(InventoryRig->GetHoveredHandSlot()));
		return HandItem.Definition ? &HandItem : nullptr;
	}

	if (InventoryRig->GetHoveredQuickSlot() != INDEX_NONE)
	{
		const FFSInventoryItem& QuickSlotItem = Inventory->GetQuickSlotItem(AFSInventoryRig::GetQuickSlotOfIndex(InventoryRig->GetHoveredQuickSlot()));
		return QuickSlotItem.Definition ? &QuickSlotItem : nullptr;
	}

	if (InventoryRig->GetHoveredArmorSlot() != INDEX_NONE)
	{
		const FFSInventoryItem& ArmorItem = Inventory->GetArmorItem(AFSInventoryRig::GetArmorTypeOfSlot(InventoryRig->GetHoveredArmorSlot()));
		return ArmorItem.Definition ? &ArmorItem : nullptr;
	}

	if (InventoryRig->GetHoveredOrganizeCell().X != INDEX_NONE)
	{
		return Inventory->FindItemAt(InventoryRig->GetHoveredOrganizeCell(), EFSItemGrid::Organize);
	}

	return Inventory->FindItemAt(InventoryRig->GetHoveredCell());
}

void AFSPlayerController::Interact()
{
	if (InventoryViewState != EFSInventoryViewState::Closed || !FSCharacter)
	{
		return;
	}

	AFSItemPickup* Pickup = FSCharacter->GetNearestPickup();
	UFSInventoryComponent* Inventory = GetInventory();

	if (Pickup && Pickup->IsBackpack() && Inventory && Inventory->HasBackpack())
	{
		DropBackpack();
	}

	if (!Pickup || Pickup->TryPickUp(Inventory))
	{
		return;
	}

	// Checked first so the item stays in the world if the inventory can't open
	if (InventoryRigClass && CanOpenInventory() && Pickup->TryCarry(Inventory))
	{
		OpenInventory();
	}
}

void AFSPlayerController::SelectAtInventoryCursor()
{
	if (InventoryViewState == EFSInventoryViewState::Open)
	{
		UpdateHoveredInventoryCell();

		// Outside the grid means tossing the carried item, so the buttons and the slots have to be checked first
		if (InventoryRig->GetHoveredButton() != INDEX_NONE)
		{
			SelectInventoryButton(InventoryRig->GetHoveredButton());
		}
		else if (InventoryRig->GetHoveredHandSlot() != INDEX_NONE)
		{
			SelectInventoryHand(AFSInventoryRig::GetHandOfSlot(InventoryRig->GetHoveredHandSlot()));
		}
		else if (InventoryRig->GetHoveredQuickSlot() != INDEX_NONE)
		{
			SelectInventoryQuickSlot(AFSInventoryRig::GetQuickSlotOfIndex(InventoryRig->GetHoveredQuickSlot()));
		}
		else if (InventoryRig->GetHoveredArmorSlot() != INDEX_NONE)
		{
			SelectInventoryArmorSlot(AFSInventoryRig::GetArmorTypeOfSlot(InventoryRig->GetHoveredArmorSlot()));
		}
		else if (InventoryRig->GetHoveredOrganizeCell().X != INDEX_NONE)
		{
			SelectInventoryOrganizeCell(InventoryRig->GetHoveredOrganizeCell());
		}
		else
		{
			SelectInventoryCell(InventoryRig->GetHoveredCell());
		}
	}
}

void AFSPlayerController::SelectInventoryCell(const FIntPoint& Cell)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	if (!Inventory->IsCarryingItem())
	{
		Inventory->PickUpItemAt(Cell);
	}
	else if (Cell == FIntPoint(INDEX_NONE, INDEX_NONE))
	{
		TossCarriedItem();
	}
	else
	{
		Inventory->PlaceCarriedItem(Cell);
	}

	RefreshInventoryView();
}

void AFSPlayerController::SelectInventoryOrganizeCell(const FIntPoint& Cell)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	if (Inventory->IsCarryingItem())
	{
		Inventory->PlaceCarriedItem(Cell, EFSItemGrid::Organize);
	}
	else
	{
		Inventory->PickUpItemAt(Cell, EFSItemGrid::Organize);
	}

	RefreshInventoryView();
}

void AFSPlayerController::SelectInventoryHand(EFSHand Hand)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryViewState == EFSInventoryViewState::Open && Inventory->SwapCarriedItemWithHand(Hand))
	{
		RefreshInventoryView();
	}
}

void AFSPlayerController::SelectInventoryQuickSlot(EFSQuickSlot QuickSlot)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryViewState == EFSInventoryViewState::Open && Inventory->SwapCarriedItemWithQuickSlot(QuickSlot))
	{
		RefreshInventoryView();
	}
}

void AFSPlayerController::SelectInventoryArmorSlot(EFSArmorType ArmorType)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryViewState == EFSInventoryViewState::Open && Inventory->SwapCarriedItemWithArmorSlot(ArmorType))
	{
		RefreshInventoryView();
	}
}

void AFSPlayerController::SelectInventoryButton(int32 Button)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	if (Button == AFSInventoryRig::DropBackpackButton)
	{
		DropBackpack();
	}
	else if (Button == AFSInventoryRig::OrganizeBackpackButton)
	{
		if (Inventory->IsOrganizing())
		{
			StopOrganizing();
		}
		else
		{
			Inventory->StartOrganizing();
		}

		RefreshInventoryView();
	}
}

void AFSPlayerController::StopOrganizing()
{
	UFSInventoryComponent* Inventory = GetInventory();
	TArray<FFSInventoryItem> Items;

	if (!Inventory || !FSCharacter || !Inventory->StopOrganizing(Items))
	{
		return;
	}

	// There can be a lot of them, so they're spread out around the character's feet rather than all landing in one spot. The golden angle keeps a spiral like this evenly filled, however many there are.
	constexpr double SpreadPerItem = 12.0;
	constexpr double GoldenAngle = 2.39996;

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const double Radius = SpreadPerItem * FMath::Sqrt(static_cast<double>(Index));
		const double Angle = GoldenAngle * Index;
		AFSItemPickup::DropAtFeet(GetDroppedItemClass(), Items[Index].Definition, *FSCharacter, FVector2D(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle)));
	}
}

void AFSPlayerController::RotateCarriedInventoryItem()
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryViewState == EFSInventoryViewState::Open && Inventory->RotateCarriedItem())
	{
		RefreshInventoryView();
	}
}

void AFSPlayerController::TossCarriedItem()
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Inventory->IsCarryingItem() || !FSCharacter)
	{
		return;
	}

	// Spawning fails while the world is being torn down, and the item is then still on the cursor rather than lost
	if (AFSItemPickup::DropAtFeet(GetDroppedItemClass(), Inventory->GetCarriedItem().Definition, *FSCharacter))
	{
		Inventory->ClearCarriedItem();
	}
}

void AFSPlayerController::DropBackpack()
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Inventory->HasBackpack() || !FSCharacter)
	{
		return;
	}

	// Spawning fails while the world is being torn down, and the backpack is then still being worn rather than lost
	AFSItemPickup* Pickup = AFSItemPickup::DropAtFeet(GetDroppedItemClass(), Inventory->GetBackpack(), *FSCharacter);
	if (!Pickup)
	{
		return;
	}

	// There's nothing left to organize
	StopOrganizing();

	FFSBackpackContents Contents;
	Inventory->RemoveBackpack(Contents);
	Pickup->SetBackpackContents(Contents);

	if (InventoryViewState != EFSInventoryViewState::Closed)
	{
		RefreshInventoryView();
	}
}

TSubclassOf<AFSItemPickup> AFSPlayerController::GetDroppedItemClass() const
{
	return TossedItemClass ? TossedItemClass : TSubclassOf<AFSItemPickup>(AFSItemPickup::StaticClass());
}

void AFSPlayerController::RefreshInventoryView()
{
	const UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryRig)
	{
		InventoryRig->ShowInventory(*Inventory);
		UpdateHoveredInventoryCell();
	}
}

void AFSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (InventoryViewState != EFSInventoryViewState::Opening && InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	// The world keeps running, so the character can be knocked off a ledge while looking at the pack
	if (!CanOpenInventory())
	{
		CloseInventory();
		return;
	}

	if (InventoryViewState == EFSInventoryViewState::Open)
	{
		UpdateHoveredInventoryCell();
	}
}

void AFSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	FSCharacter = Cast<AFSCharacter>(InPawn);
}

void AFSPlayerController::OnUnPossess()
{
	CloseInventoryImmediately();

	Super::OnUnPossess();

	FSCharacter = nullptr;
}

void AFSPlayerController::ToggleInventory()
{
	// Toggling during a blend is ignored, because reversing a camera blend part way through snaps it to the end
	if (InventoryViewState == EFSInventoryViewState::Closed)
	{
		OpenInventory();
	}
	else if (InventoryViewState == EFSInventoryViewState::Open)
	{
		CloseInventory();
	}
}

bool AFSPlayerController::CanOpenInventory() const
{
	const UCharacterMovementComponent* Movement = FSCharacter ? FSCharacter->GetCharacterMovement() : nullptr;
	return Movement && Movement->IsMovingOnGround();
}

void AFSPlayerController::OpenInventory()
{
	if (InventoryViewState != EFSInventoryViewState::Closed || !CanOpenInventory())
	{
		return;
	}

	if (!InventoryRigClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory can't open: InventoryRigClass isn't set"));
		return;
	}

	if (!InventoryRig)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InventoryRig = GetWorld()->SpawnActor<AFSInventoryRig>(InventoryRigClass, SpawnParameters);
	}

	InventoryRig->AttachToCharacter(FSCharacter);

	if (const UFSInventoryComponent* Inventory = GetInventory())
	{
		InventoryRig->ShowInventory(*Inventory);
	}

	InventoryViewState = EFSInventoryViewState::Opening;
	SetInventoryInputActive(true);
	StartInventoryBlend(InventoryRig, OpeningSwapFraction);
}

void AFSPlayerController::CloseInventory()
{
	if (InventoryViewState != EFSInventoryViewState::Opening && InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	StopOrganizing();
	TossCarriedItem();
	RefreshInventoryView();

	InventoryViewState = EFSInventoryViewState::Closing;
	InventoryRig->SetGridVisible(false);
	SetInventoryInputActive(false);
	StartInventoryBlend(FSCharacter, ClosingSwapFraction);
}

void AFSPlayerController::CloseInventoryImmediately()
{
	if (InventoryViewState == EFSInventoryViewState::Closed)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(InventoryMidpointTimer);
	GetWorldTimerManager().ClearTimer(InventoryFinishedTimer);

	StopOrganizing();
	TossCarriedItem();
	RefreshInventoryView();

	InventoryRig->SetGridVisible(false);
	SetInventoryInputActive(false);
	SwapWornPackForHeldPack(false);
	InventoryRig->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetViewTarget(FSCharacter);

	InventoryViewState = EFSInventoryViewState::Closed;
}

void AFSPlayerController::SetInventoryInputActive(bool bActive)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		UInputMappingContext* ContextToAdd = bActive ? InventoryMappingContext : DefaultMappingContext;
		UInputMappingContext* ContextToRemove = bActive ? DefaultMappingContext : InventoryMappingContext;

		if (ContextToRemove)
		{
			Subsystem->RemoveMappingContext(ContextToRemove);
		}

		if (ContextToAdd)
		{
			Subsystem->AddMappingContext(ContextToAdd, 0);
		}
	}

	bShowMouseCursor = bActive;

	// The use buttons aren't mapped while the inventory is open, so they'd never be released
	if (bActive && FSCharacter)
	{
		FSCharacter->GetItemUse()->StopUsingAll();
	}

	if (bActive)
	{
		// Without this the cursor disappears whenever a mouse button is held
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);

		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		GetViewportSize(ViewportWidth, ViewportHeight);
		SetMouseLocation(ViewportWidth / 2, ViewportHeight / 2);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AFSPlayerController::StartInventoryBlend(AActor* NewViewTarget, float MidpointFraction)
{
	SetViewTargetWithBlend(NewViewTarget, InventoryBlendTime, VTBlend_EaseInOut, 2.0f);

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(InventoryMidpointTimer, this, &AFSPlayerController::HandleInventoryBlendMidpoint, InventoryBlendTime * MidpointFraction, false);
	TimerManager.SetTimer(InventoryFinishedTimer, this, &AFSPlayerController::HandleInventoryBlendFinished, InventoryBlendTime, false);
}

void AFSPlayerController::SwapWornPackForHeldPack(bool bHeld)
{
	if (FSCharacter)
	{
		FSCharacter->SetInventoryViewBonesHidden(bHeld);
	}

	InventoryRig->SetPackVisible(bHeld);
}

void AFSPlayerController::HandleInventoryBlendMidpoint()
{
	SwapWornPackForHeldPack(InventoryViewState == EFSInventoryViewState::Opening);
}

void AFSPlayerController::HandleInventoryBlendFinished()
{
	if (InventoryViewState == EFSInventoryViewState::Opening)
	{
		InventoryViewState = EFSInventoryViewState::Open;
		InventoryRig->SetGridVisible(true);
	}
	else if (InventoryViewState == EFSInventoryViewState::Closing)
	{
		InventoryViewState = EFSInventoryViewState::Closed;
		InventoryRig->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AFSPlayerController::UpdateHoveredInventoryCell()
{
	FVector RayOrigin;
	FVector RayDirection;
	FIntPoint Cell(INDEX_NONE, INDEX_NONE);
	int32 HandSlot = INDEX_NONE;
	int32 QuickSlot = INDEX_NONE;
	int32 ArmorSlot = INDEX_NONE;
	int32 Button = INDEX_NONE;
	FIntPoint OrganizeCell(INDEX_NONE, INDEX_NONE);

	if (DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		InventoryRig->GetOrganizeCellFromWorldRay(RayOrigin, RayDirection, OrganizeCell);
		InventoryRig->GetButtonFromWorldRay(RayOrigin, RayDirection, Button);
		InventoryRig->GetCellFromWorldRay(RayOrigin, RayDirection, Cell);
		InventoryRig->GetHandSlotFromWorldRay(RayOrigin, RayDirection, HandSlot);
		InventoryRig->GetQuickSlotFromWorldRay(RayOrigin, RayDirection, QuickSlot);
		InventoryRig->GetArmorSlotFromWorldRay(RayOrigin, RayDirection, ArmorSlot);
		InventoryRig->SetCursorRay(RayOrigin, RayDirection);
	}

	InventoryRig->SetHoveredCell(Cell);
	InventoryRig->SetHoveredHandSlot(HandSlot);
	InventoryRig->SetHoveredQuickSlot(QuickSlot);
	InventoryRig->SetHoveredArmorSlot(ArmorSlot);
	InventoryRig->SetHoveredButton(Button);
	InventoryRig->SetHoveredOrganizeCell(OrganizeCell);
}
