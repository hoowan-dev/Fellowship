// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/FSInventoryComponent.h"
#include "FSInventoryRig.generated.h"

class ACharacter;
class USkeletalMeshComponent;
class UCameraComponent;
class UFSInventoryComponent;
class UFSItemDefinition;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPoseableMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FFSInventoryItem;
enum class EFSHand : uint8;
enum class EFSQuickSlot : uint8;
enum class EFSArmorType : uint8;
enum class EFSItemGrid : uint8;

/**
 * What the player sees with the inventory open: a first person camera looking at the held backpack, with the grid floating between them,
 * a column of two cells either side of it, with that side's quick slot on top and its hand below, the player's forearms raised behind the hands' cells,
 * and a row of buttons below it. The backpack sways gently, with what's in the quick slots hanging off it. The player can walk slowly with it open,
 * which bobs the camera and jiggles the items, the arms and the pack. The grids, cells and buttons stay still.
 * While organizing, all of that slides to the left to make room for a second grid on the right.
 * The display moves away from the camera by however much it takes to keep every box on a 16:9 screen.
 * Without a backpack only the forearms, the hands' cells and the armor slots are there.
 * Exists only on the owning client. The grid's cells run from (0, 0) at the top left.
 */
UCLASS()
class FELLOWSHIP_API AFSInventoryRig : public AActor
{
	GENERATED_BODY()

public:
	// The instance custom data each grid cell has, which M_InventoryGridCell reads. Each is 1 or 0.
	static constexpr int32 CellHoveredData = 0;
	static constexpr int32 CellTakenData = 1;
	static constexpr int32 CellCanPlaceData = 2;
	static constexpr int32 CellCannotPlaceData = 3;
	static constexpr int32 CellCanSwapData = 4;

	// The first of the three values for the red, green and blue of the rarity of the item in a cell, which only matter while it's taken
	static constexpr int32 CellItemColorData = 5;
	static constexpr int32 CellDataCount = 8;

	// Each border bar's custom data is its colour's red, green and blue, which M_InventoryItemBorder reads
	static constexpr int32 BorderDataCount = 3;

	// The instance of the hand slots each hand's cell is
	static constexpr int32 LeftHandSlot = 0;
	static constexpr int32 RightHandSlot = 1;
	static constexpr int32 HandSlotCount = 2;

	// Which hand a hand slot is for
	static EFSHand GetHandOfSlot(int32 HandSlot);

	// The instance of the quick slots each quick slot's cell is
	static constexpr int32 LeftQuickSlot = 0;
	static constexpr int32 RightQuickSlot = 1;
	static constexpr int32 QuickSlotCount = 2;

	// The buttons below the grid, from left to right
	static constexpr int32 DropBackpackButton = 0;
	static constexpr int32 UseItemButton = 1;
	static constexpr int32 OrganizeInventoryButton = 2;
	static constexpr int32 ButtonCount = 3;

	// Which of the inventory's quick slots an instance of the quick slots is for
	static EFSQuickSlot GetQuickSlotOfIndex(int32 QuickSlotIndex);

	// There's a cell for each type of armor, and an instance of the armor slots is that type's value
	static constexpr int32 ArmorSlotCount = 3;

	// Which of the inventory's armor slots an instance of the armor slots is for
	static EFSArmorType GetArmorTypeOfSlot(int32 ArmorSlot);

	AFSInventoryRig();

	virtual void Tick(float DeltaTime) override;

	// Moves the rig to the character's head, facing the way the character faces, and keeps it there. The arms take on the character's mesh.
	void AttachToCharacter(ACharacter* Character);

	void SetPackVisible(bool bVisible);

	// The grid scales in and out rather than appearing instantly
	void SetGridVisible(bool bVisible);

	// Finds the cell a world space ray passes through. Returns false if it misses the grid.
	bool GetCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const;

	// Highlights a cell, or every cell an item covers if there's one in it. While an item is carried, previews the cells it would
	// cover with its top left in this cell instead, showing whether it can go there. Any cell outside the grid clears the highlight.
	void SetHoveredCell(const FIntPoint& Cell);

	// Finds the hand slot a world space ray passes through. Returns false if it misses both.
	bool GetHandSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutHandSlot) const;

	// Highlights a hand slot, showing whether the carried item would go in it or swap with what's there. INDEX_NONE clears the highlight.
	void SetHoveredHandSlot(int32 HandSlot);

	// Finds the quick slot a world space ray passes through. Returns false if it misses both.
	bool GetQuickSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutQuickSlot) const;

	// Highlights a quick slot, showing whether the carried item would go in it or swap with what's there. INDEX_NONE clears the highlight.
	void SetHoveredQuickSlot(int32 QuickSlot);

	// Finds the armor slot a world space ray passes through. Returns false if it misses them all. They're there without a backpack.
	bool GetArmorSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutArmorSlot) const;

	// Highlights an armor slot, showing whether the carried item would go in it, swap with what's there or isn't allowed there. INDEX_NONE clears the highlight.
	void SetHoveredArmorSlot(int32 ArmorSlot);

	// Finds the button a world space ray passes through. Returns false if it misses them all. There are no buttons without a backpack.
	bool GetButtonFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutButton) const;

	// Finds the cell of the organizing grid a world space ray passes through. Returns false if it misses, or if the player isn't organizing.
	bool GetOrganizeCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const;

	// Highlights a cell of the organizing grid, and any item it covers, or previews where the carried item would go. Cells outside that grid clear the highlight.
	void SetHoveredOrganizeCell(const FIntPoint& Cell);

	// Highlights a button. INDEX_NONE clears the highlight.
	void SetHoveredButton(int32 Button);

	FORCEINLINE int32 GetHoveredButton() const { return HoveredButton; }

	// Moves the light and the carried item to where the mouse points, given as a world space ray. The light shines wherever that is, on everything in the inventory.
	// The carried item leans the way the point has been moving on tick, and trails behind it.
	void SetCursorRay(const FVector& RayOrigin, const FVector& RayDirection);

	// Matches the pack, the grid, the item meshes, their borders and the carried item to what the inventory holds
	void ShowInventory(const UFSInventoryComponent& Inventory);

	// Where a held item's mesh sits relative to its hand's socket on the arms, gripped as the character grips it drawn, at its full size
	FTransform GetHandItemTransform(const FFSInventoryItem& Item, int32 HandSlot) const;

	// The socket on the arms that a hand's item is held at, the same as the character holds it at
	FName GetHandSocket(int32 HandSlot) const;

	// Where a quick slot's item's mesh sits relative to the grid, upright and fitted to the slot's cell
	FTransform GetQuickSlotItemTransform(const FFSInventoryItem& Item, int32 QuickSlot) const;

	// The same for what a hand holds, shown in the hand's cell
	FTransform GetHandSlotItemTransform(const FFSInventoryItem& Item, int32 HandSlot) const;
	FTransform GetArmorSlotItemTransform(const FFSInventoryItem& Item, int32 ArmorSlot) const;

	// Where an item's mesh sits relative to the grid, fitted to the cells it covers
	FTransform GetItemTransform(const FFSInventoryItem& Item) const;

	// The same for an item in the organizing grid
	FTransform GetOrganizeItemTransform(const FFSInventoryItem& Item) const;

	// Where a carried item's mesh sits relative to the grid, with the top left of its cells at a point on the grid. It's closer to the camera
	// than every item in the grid, and shrunk by just enough to look the same size and in the same place as if it were on the grid.
	FTransform GetCarriedItemTransform(const FFSInventoryItem& Item, const FVector2D& GridPoint) const;

	FORCEINLINE const TArray<TObjectPtr<UStaticMeshComponent>>& GetItemMeshes() const { return ItemMeshes; }
	FORCEINLINE UStaticMeshComponent* GetCarriedItemMesh() const { return CarriedItemMesh; }
	FORCEINLINE UStaticMeshComponent* GetHandItemMesh(int32 HandSlot) const { return HandSlot == LeftHandSlot ? LeftHandItemMesh : RightHandItemMesh; }

	// The same item shown in the hand's cell, fitted to it like the other slots' items
	FORCEINLINE UStaticMeshComponent* GetHandSlotItemMesh(int32 HandSlot) const { return HandSlot == LeftHandSlot ? LeftHandSlotItemMesh : RightHandSlotItemMesh; }
	FORCEINLINE int32 GetHoveredHandSlot() const { return HoveredHandSlot; }
	FORCEINLINE UPointLightComponent* GetCursorLight() const { return CursorLight; }
	FORCEINLINE const FIntPoint& GetHoveredCell() const { return HoveredCell; }
	FORCEINLINE const FIntPoint& GetHoveredOrganizeCell() const { return HoveredOrganizeCell; }
	FORCEINLINE UInstancedStaticMeshComponent* GetOrganizeGrid() const { return OrganizeGrid; }

	// Whether the last inventory shown was being organized
	FORCEINLINE bool IsOrganizing() const { return bOrganizing; }

	// How far the display is across the screen right now, in cm of the display. On screen, that's this much smaller.
	FORCEINLINE double GetDisplaySlide() const { return DisplaySlide; }

	// How big the plane of the grid looks right now, as a fraction of how big it looks at its usual distance
	FORCEINLINE double GetDisplayScale() const { return DisplayScale; }
	FORCEINLINE UInstancedStaticMeshComponent* GetGrid() const { return Grid; }
	FORCEINLINE UInstancedStaticMeshComponent* GetItemBorders() const { return ItemBorders; }
	FORCEINLINE int32 GetCellIndex(const FIntPoint& Cell) const { return Cell.Y * GridWidth + Cell.X; }
	FORCEINLINE UInstancedStaticMeshComponent* GetHandSlots() const { return HandSlots; }
	FORCEINLINE UInstancedStaticMeshComponent* GetQuickSlots() const { return QuickSlots; }
	FORCEINLINE UStaticMeshComponent* GetQuickSlotItemMesh(int32 QuickSlot) const { return QuickSlot == LeftQuickSlot ? LeftQuickSlotItemMesh : RightQuickSlotItemMesh; }
	FORCEINLINE int32 GetHoveredQuickSlot() const { return HoveredQuickSlot; }
	FORCEINLINE UInstancedStaticMeshComponent* GetArmorSlots() const { return ArmorSlots; }
	FORCEINLINE UStaticMeshComponent* GetArmorSlotItemMesh(int32 ArmorSlot) const { return ArmorSlotItemMeshes[ArmorSlot]; }
	FORCEINLINE int32 GetHoveredArmorSlot() const { return HoveredArmorSlot; }
	FORCEINLINE UPoseableMeshComponent* GetArms() const { return Arms; }
	FORCEINLINE UStaticMeshComponent* GetPack() const { return Pack; }

	// Where the quick slots' items are tied onto the pack's sides, on the display, in cm from its middle as seen from the camera on the grid's plane:
	// X to the right and Y up. Taken at the middle of the pack's width.
	FVector2D GetPackHangPoint() const;

	// The height on the grid's plane that the point where items hang off the pack is raised to, level with the middle of the quick slots' cells,
	// so that what hangs down from there sits about where the cells are
	double GetPackHangTarget() const;
	FORCEINLINE UStaticMeshComponent* GetBackgroundDarkener() const { return BackgroundDarkener; }
	FORCEINLINE UCameraComponent* GetViewCamera() const { return ViewCamera; }

	// How far the carried item is leaning, in degrees: X to the right and Y up
	FORCEINLINE const FVector2D& GetCarriedItemSway() const { return CarriedItemSway; }

	// How far the carried item trails behind the mouse on the plane of the grid, in cm: X to the right and Y up
	FORCEINLINE const FVector2D& GetCarriedItemTrail() const { return CarriedItemTrail; }

	// The size of a hand's or quick slot's cell: X across and Y up
	FORCEINLINE FVector2D GetSideSlotSize() const { return FVector2D(SideSlotWidth, SideSlotHeight); }
	FORCEINLINE float GetSpacing() const { return Spacing; }

	// Each armor slot is square, sized so the column of them, with the spacing between, is exactly as tall as the grid
	FORCEINLINE double GetArmorSlotSize() const { return FMath::Max(GridHeight * CellSize - Spacing * (ArmorSlotCount - 1), 0.1) / ArmorSlotCount; }

	// From the top of a slot's cell to the middle of its name, halfway across the gap above it
	FORCEINLINE double GetSlotLabelHeight() const { return Spacing * 0.5; }

	// The meshes of what's in the quick slots, hanging off the displayed backpack
	FORCEINLINE const FFSHungItemMeshes& GetPackHungItems() const { return PackHungItems; }

	// How far the items and the arms have jiggled away from where they sit, in cm: X to the right and Y up. Walking makes them jiggle.
	FORCEINLINE const FVector2D& GetItemJiggle() const { return ItemJiggle; }

	// The same jiggle as an angle seen from the camera, in radians, as the items show it on the screen, scaled in and out with the grid.
	// Something else in front of the camera that moves by this much on the screen jiggles along with them.
	FORCEINLINE FVector2D GetItemJiggleAngle() const { return ItemJiggle * GridScale / (GridDistance + DisplayDepth); }

	// What every item mesh hangs from, so that they can jiggle while the cells they sit in stay still
	FORCEINLINE USceneComponent* GetItemRoot() const { return ItemRoot; }
	FORCEINLINE USceneComponent* GetDisplay() const { return Display; }

	// How much the camera is bobbing, from 0 standing still to 1 walking at Bob Full Speed
	FORCEINLINE float GetBobWeight() const { return BobWeight; }
	FORCEINLINE UInstancedStaticMeshComponent* GetButtons() const { return Buttons; }

	// Whether the last inventory shown had a backpack, which is what the grid, quick slots and buttons belong to. True until one is shown.
	FORCEINLINE bool HasBackpack() const { return bHasBackpack; }

	// The centre of a hand's cell on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetHandSlotCentre(int32 HandSlot) const;

	// The centre of a quick slot's cell on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetQuickSlotCentre(int32 QuickSlot) const;

	// The centre of an armor slot's cell on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetArmorSlotCentre(int32 ArmorSlot) const;

	// The point in the world to centre a hand slot's name on, just above its cell
	FVector GetHandSlotLabelLocation(int32 HandSlot) const;

	// The point in the world to centre a quick slot's name on, just above its cell
	FVector GetQuickSlotLabelLocation(int32 QuickSlot) const;

	// The point in the world to centre an armor slot's name on, just above its cell
	FVector GetArmorSlotLabelLocation(int32 ArmorSlot) const;

	// The centre of a button on the plane of the grid, in cm from the grid's centre: X to the right and Y up.
	// Drop Backpack is above the grid and Use Item below it, both as wide as it. Organize Inventory is right of the right column, as tall as it.
	FVector2D GetButtonSize(int32 Button) const;
	FVector2D GetButtonCentre(int32 Button) const;

	// Whether a button is there to see and press. Drop Backpack needs a backpack, and the others are always there.
	FORCEINLINE bool IsButtonShown(int32 Button) const { return bHasBackpack || Button != DropBackpackButton; }

	// Whether a button stands on end, with its label running down it. Organize Inventory does, beside the right column.
	FORCEINLINE bool IsButtonUpright(int32 Button) const { return Button == OrganizeInventoryButton; }

	// The point in the world to centre a button's label on
	FVector GetButtonLabelLocation(int32 Button) const;

	// How far the middle of the organizing grid is to the right of the middle of the grid, just clear of the Organize Inventory button. It slides along with everything else, starting off screen.
	double GetOrganizeGridOffset() const;

protected:
	virtual void BeginPlay() override;

	void BuildGrid();
	void BuildHandSlots();
	void BuildQuickSlots();
	void BuildArmorSlots();
	void BuildButtons();
	void BuildOrganizeGrid();
	// Matches the arms to the character, or to what's worn on its body
	void RefreshArmsMesh();

	void PoseArms();
	void PoseArm(bool bLeft);

	// Shows the backpack's mesh in the background, placed by its Inventory Mesh Transform. Null shows nothing.
	void FitPackMesh(const UFSItemDefinition* Backpack);

	// Places the pack where it rests, plus where the slide and its jiggle take it, turned by its sway
	void UpdatePackTransform();

	// Places the display where the slide takes it
	void UpdateDisplayPlacement();

	// Moves the camera's bob and the jiggles on by a frame, from how fast the character the rig is attached to is walking
	void UpdateWalkingMotion(float DeltaTime);

	// Shows the grid and everything on it if the display is showing and it's scaled in, except for what needs a backpack when there isn't one
	void UpdateGridVisibility();

	// Whether the grids and what's on them are showing: the display is up and they've started scaling in. The backpack's own cells also need a backpack.
	FORCEINLINE bool IsGridShown() const { return Display->GetVisibleFlag() && GridScale > 0.0f; }

	void AddItemBorder(const FFSInventoryItem& Item, const FVector2D& TopLeft);

	// Borders a slot in the colour of what's in it. An empty slot gets none. Size is X across and Y up.
	void AddSlotBorder(const FFSInventoryItem& Item, const FVector2D& SlotCentre, const FVector2D& SlotSize);

	void AddBorder(double Left, double Top, double Width, double Height, double Thickness, const FLinearColor& Color);
	bool IsCellInGrid(const FIntPoint& Cell) const;

	// Finds where a world space ray meets the plane of the grid, in cm from the grid's centre: X to the right and Y up. Returns false if it never does.
	bool GetGridPointFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FVector2D& OutGridPoint) const;
	FIntPoint GetCellFromGridPoint(const FVector2D& GridPoint) const;
	FTransform GetItemTransformAt(const FFSInventoryItem& Item, const FVector2D& TopLeft) const;
	// Matches the highlight on one of the grids to a newly hovered cell
	void UpdateCellHighlight(UInstancedStaticMeshComponent& Cells, EFSItemGrid Which, const FIntPoint& NewCell, FIntPoint& Hovered, TArray<FIntPoint>& Highlighted, int32& HighlightedData);

	// The top left of an item's cells on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetItemTopLeft(const FFSInventoryItem& Item) const;
	FVector2D GetOrganizeItemTopLeft(const FFSInventoryItem& Item) const;

	bool IsCellInOrganizeGrid(const FIntPoint& Cell) const;
	FIntPoint GetOrganizeCellFromGridPoint(const FVector2D& GridPoint) const;

	// Where the display goes to keep every box on the screen, either at rest or making room for the organizing grid: how far across it is, and how big it looks
	void GetDisplayLayout(bool bForOrganizing, double& OutSlide, double& OutScale) const;

	// How much further from the camera something at a depth in the display is than the grid is
	double GetDepthRatio(double Depth) const;

	// How far something at a depth has to move across the display to stay lined up with the grid on screen. Sliding the display moves everything the same distance, which looks like less the further away it is.
	double GetSlideCorrection(double Depth) const;

	// Puts the display where the slide has got to, and everything that has to stay lined up with its cell as it goes
	void ApplySlide();

	// Where the slot items rest depends on the slide
	void UpdateSlotItemRests();
	// Size is X across and Y up
	FTransform GetSlotItemTransform(const FFSInventoryItem& Item, const FVector2D& SlotCentre, const FVector2D& SlotSize) const;
	FVector GetSlotLabelLocation(const FVector2D& SlotCentre, double SlotHeight) const;
	FVector GetLocationOnGrid(const FVector2D& GridPoint) const;

	// Starts the carried item's mesh turning from how it looked at its old rotation to how it should look at its current one
	void StartCarriedItemTurn(const FFSInventoryItem& Item, uint8 OldRotation);

	// Moves the sway on by a frame, or settles it if nothing's carried
	void UpdateCarriedItemSway(float DeltaTime);

	// Places the carried item's mesh under the cursor, leaning and part way through any turn
	void UpdateCarriedItemMesh();

	// Places the meshes of the items in the hands and quick slots, swaying about their cells
	void UpdateSlotItemSway();

	// The turn of a slot's item at this moment. Phase is how far ahead of the others it is, as a fraction of a sway.
	FQuat GetSlotItemSwayRotation(double Phase) const;

	// Moves a highlight between the cells of one kind of slot. Hovered and HoveredData are that kind's record of what it has set.
	void SetHoveredSlot(UInstancedStaticMeshComponent& Slots, int32& Hovered, int32& HoveredData, int32 NewHovered, bool bNewHoveredTaken, bool bNewHoveredBlocked = false);

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UCameraComponent> ViewCamera;

	// Frame the pack and grid are placed in: X runs away from the camera, Y to the right, Z up
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> Display;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> Pack;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> Grid;

	// Four bars around each item in the inventory. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> ItemBorders;

	// Holds every item mesh, in the grid, the slots and on the cursor, so that they jiggle as the player walks while the cells stay still.
	// Attached to the grid at its middle, so the items scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> ItemRoot;

	// Has no mesh while nothing is carried
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> CarriedItemMesh;

	// A cell over each hand, for what that hand holds, at the bottom of the column beside the grid on its side. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> HandSlots;

	// Held in the hand, at its full size. Has no mesh while the hand is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> LeftHandItemMesh;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> RightHandItemMesh;

	// What the hand holds, also shown in its cell. Attached to the grid so it scales in and out along with it. Has no mesh while the hand is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> LeftHandSlotItemMesh;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> RightHandSlotItemMesh;

	// A cell for each quick slot, at the top of the column beside the grid on its side. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> QuickSlots;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the slot is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> LeftQuickSlotItemMesh;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the slot is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> RightQuickSlotItemMesh;

	// A cell for each armor slot, in a column left of the left hand. Attached to the grid so they scale in and out along with it, but they stay without a backpack.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> ArmorSlots;

	// Attached to the grid so they scale in and out along with it. They have no mesh while their slot is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> ArmorSlotItemMeshes[ArmorSlotCount];

	// A wide cell below the grid that drops the backpack when selected. Attached to the grid so it scales in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> Buttons;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> OrganizeGrid;

	// Fills the screen just behind the grid and everything on it, shading the arms, the pack and the world beyond them
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> BackgroundDarkener;

	// A copy of the character's mesh with its arms posed in the display's frame and the rest of its body shrunk away, since it's all one mesh
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPoseableMeshComponent> Arms;

	// Follows the mouse, shining on everything in the inventory: the items, the arms and the pack. The cells are unlit, so it doesn't brighten them.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPointLightComponent> CursorLight;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", Units = cm))
	float CellSize = 3.0f;

	// From the camera to the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "1.0", Units = cm))
	float GridDistance = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> GridCellMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> ItemBorderMaterial;

	// How wide the border around an item on the grid is. It's drawn inside the cells the item covers.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = cm))
	float ItemBorderThickness = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = s))
	float GridScaleTime = 0.08f;

	// How much of the cells it covers an item fills, leaving a gap so it doesn't touch the lines
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ItemFillFraction = 0.9f;

	// The gap between a carried item and the tallest item in the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = cm))
	float CarriedItemClearance = 1.0f;

	// How many degrees a carried item leans for each cm per second it's dragged at on the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0"))
	float CarriedItemSwayPerSpeed = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0", ClampMax = "60.0", Units = deg))
	float CarriedItemMaxSway = 35.0f;

	// How far a carried item trails behind the mouse for each cm per second it's dragged at on the grid, like something dangling from it
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0", Units = s))
	float CarriedItemTrailPerSpeed = 0.02f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0", Units = cm))
	float CarriedItemMaxTrail = 3.0f;

	// How hard a carried item is pulled towards the lean its speed calls for. Higher swings faster.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float CarriedItemSwayStiffness = 150.0f;

	// How quickly a carried item's swinging dies down. Lower swings back and forth more times when it stops.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float CarriedItemSwayDamping = 6.0f;

	// How long a carried item takes to turn when it's rotated
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carried Item", meta = (ClampMin = "0.01", Units = s))
	float CarriedItemTurnTime = 0.15f;

	// How far the items in the hands and quick slots turn each way as they sway
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Slot Items", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = deg))
	float SlotItemSwayAngle = 3.0f;

	// How long the items in the hands and quick slots take to sway there and back
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Slot Items", meta = (ClampMin = "0.1", Units = s))
	float SlotItemSwayPeriod = 3.5f;

	// Unlit and translucent, with its opacity set through a scalar parameter named Darkness
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Background")
	TObjectPtr<UMaterialInterface> BackgroundDarkenMaterial;

	// How much of what's behind the grid is blocked out, once the grid has scaled in
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Background", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackgroundDarkness = 0.7f;

	// How far in front of the grid the light under the mouse sits
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.0", Units = cm))
	float CursorLightHeight = 8.0f;

	// How far the light reaches, as it's rendered: the meshes it shines on are drawn at FirstPersonScale of their size.
	// Far enough to reach the arms and the pack behind the grid from anywhere on it.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.1", Units = cm))
	float CursorLightRadius = 28.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.0"))
	float CursorLightIntensity = 1.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light")
	FLinearColor CursorLightColor = FLinearColor(1.0f, 0.85f, 0.65f);

	// The size of each cell in the columns either side of the grid, with a quick slot on top and a hand below
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Side Slots", meta = (ClampMin = "0.1", Units = cm))
	float SideSlotWidth = 6.0f;

	// With the defaults the column, two of these and the spacing between them, is as tall as an 8 cell grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Side Slots", meta = (ClampMin = "0.1", Units = cm))
	float SideSlotHeight = 11.5f;

	// From the camera to the palms, which sit behind the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "1.0", Units = cm))
	float HandDistance = 65.0f;

	// How far each joint of a hand's fingers bends to grip what it holds, and the thumb's. An empty hand stays open.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (Units = deg))
	float FingerGripAngle = 70.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (Units = deg))
	float ThumbGripAngle = 30.0f;

	// From the wrist bone to the part of the hand centred in its cell
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.0", Units = cm))
	float WristToPalm = 10.0f;

	// The directions below pose the right hand, in the display's frame: X away from the camera, Y to the right, Z up. The left hand mirrors it.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector FingerDirection = FVector(0.15, -0.2, 1.0);

	// The way the palm faces, which only needs to be roughly across the fingers
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector PalmDirection = FVector(-1.0, -0.25, 0.0);

	// From the elbow to the wrist
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector ForearmDirection = FVector(0.4, -0.2, 0.9);

	// From the shoulder to the elbow. The upper arm is kept so that a narrow screen, which shows more below the elbow, doesn't show the arm's cut off end.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector UpperArmDirection = FVector(0.55, -0.3, 0.75);


	// How thick every button is: the height of Use Item below the grid and Drop Backpack above it, and the width of Organize Inventory standing on end beside the right column
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Buttons", meta = (ClampMin = "0.1", Units = cm))
	float ButtonHeight = 5.0f;

	// Between every pair of neighbours, across and up and down: the grid and the buttons above and below it, the grid and the columns either side,
	// a quick slot and the hand below it, the left column and the armor slots, one armor slot and the next, the right column and Organize Inventory,
	// and Organize Inventory and the organizing grid. The names over the cells sit in the middle of the gap above them.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "0.0", Units = cm))
	float Spacing = 1.0f;

	// How far across from the middle of the screen the boxes can go, at the grid's usual distance. 90 degrees across makes the edge of the screen 40cm away.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "1.0", Units = cm))
	float ScreenHalfWidth = 39.0f;

	// How far up and down from the middle of the screen they can go. A 16:9 screen's edge is 22.5cm away.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "1.0", Units = cm))
	float ScreenHalfHeight = 22.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Organizing", meta = (ClampMin = "0.01", Units = s))
	float OrganizeSlideTime = 0.35f;

	// From the camera to the centre of the pack
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "1.0", Units = cm))
	float PackDistance = 70.0f;

	// The pack is scaled so its longest side is this long, whatever size the mesh is
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "1.0", Units = cm))
	float PackDisplaySize = 55.0f;

	// How far the pack turns each way as it sways
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "0.0", ClampMax = "30.0", Units = deg))
	float PackSwayAngle = 2.0f;

	// How long the pack takes to sway there and back
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "0.1", Units = s))
	float PackSwayPeriod = 5.0f;

	// How much more the pack jiggles than the rest of the display as the player walks, being heavier and further away
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "0.0"))
	float PackJiggleScale = 2.5f;

	// How stiffly the pack follows the jiggle. Lower lags further behind.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "1.0"))
	float PackJiggleStiffness = 60.0f;

	// How far the character walks in one stride, two steps, which is one sway of the bob from side to side and two dips
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "1.0", Units = cm))
	float BobStrideLength = 110.0f;

	// The walking speed at which the bob and the jiggle are full. Slower walks less.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "1.0", Units = "cm/s"))
	float BobFullSpeed = 180.0f;

	// How far the camera dips with each step and rolls with each stride
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "0.0", Units = deg))
	float BobPitch = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "0.0", Units = deg))
	float BobRoll = 0.5f;

	// How far the items and the arms jiggle up and down with each step, and half as far side to side with each stride
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "0.0", Units = cm))
	float JiggleAmount = 0.175f;

	// How stiffly the items and the arms follow their jiggle, and how quickly that settles. A spring, so it overshoots a little.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "1.0"))
	float JiggleStiffness = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Walking", meta = (ClampMin = "0.0"))
	float JiggleDamping = 10.0f;

	// How far the camera looks down
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|View", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = deg))
	float ViewPitchDown = 20.0f;

	// Added to the head bone's height, which sits low in the head
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|View", meta = (Units = cm))
	float ViewHeightOffset = 10.0f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ItemMeshes;

	// Replaced by the size of the backpack's grid when it's shown
	int32 GridWidth = 8;
	int32 GridHeight = 8;

	// What ShowInventory last showed, used to find the cells a hovered item covers
	TWeakObjectPtr<const UFSInventoryComponent> ShownInventory;

	// Where the arms get their mesh from, unless what's worn takes its place
	TWeakObjectPtr<const USkeletalMeshComponent> AttachedCharacterMesh;

	FIntPoint HoveredCell = FIntPoint(INDEX_NONE, INDEX_NONE);
	int32 HoveredHandSlot = INDEX_NONE;

	// Which of the cell custom data values the hovered hand slot has set
	int32 HoveredHandSlotData = CellHoveredData;

	int32 HoveredQuickSlot = INDEX_NONE;

	// Which of the cell custom data values the hovered quick slot has set
	int32 HoveredQuickSlotData = CellHoveredData;

	int32 HoveredArmorSlot = INDEX_NONE;

	// Which of the cell custom data values the hovered armor slot has set
	int32 HoveredArmorSlotData = CellHoveredData;

	int32 HoveredButton = INDEX_NONE;

	FIntPoint HoveredOrganizeCell = FIntPoint(INDEX_NONE, INDEX_NONE);
	TArray<FIntPoint> HighlightedOrganizeCells;
	int32 HighlightedOrganizeCellData = CellHoveredData;

	bool bOrganizing = false;

	// How far through the slide the display is, from 0 at rest to 1 making room for the organizing grid
	float SlideAmount = 0.0f;

	double DisplaySlide = 0.0;
	double DisplayScale = 1.0;

	// How far the display is further from the camera than usual, which is what makes it look smaller
	double DisplayDepth = 0.0;

	// Where the pack sits with the display at rest
	FVector PackRestLocation = FVector::ZeroVector;

	// How far the pack is raised, as a height on the grid's plane, so that it's raised by the same amount on the screen however far away it is
	double PackRaise = 0.0;

	// Where items are tied onto the pack's sides, in its mesh's own space
	FVector PackHangPoint = FVector::ZeroVector;

	bool bHasBackpack = true;

	TArray<FIntPoint> HighlightedCells;

	// Which of the cell custom data values HighlightedCells have set
	int32 HighlightedCellData = CellHoveredData;

	// How far in front of the grid a carried item sits, which ShowInventory finds from the items in the grid
	double CarriedItemLift = 1.0;

	float GridScale = 0.0f;
	float GridScaleTarget = 0.0f;

	// Where the mouse last pointed on the plane of the grid, and where it was when the sway last looked
	FVector2D CursorGridPoint = FVector2D::ZeroVector;
	FVector2D SwayCursorGridPoint = FVector2D::ZeroVector;

	FVector2D CarriedItemSway = FVector2D::ZeroVector;
	FVector2D CarriedItemSwayVelocity = FVector2D::ZeroVector;
	FVector2D CarriedItemTrail = FVector2D::ZeroVector;
	FVector2D CarriedItemTrailVelocity = FVector2D::ZeroVector;

	// The pack's place and look at rest, which the slide, its jiggle and its sway are added to
	FQuat PackRestRotation = FQuat::Identity;
	FVector PackRestScale = FVector::OneVector;
	double PackSwayTime = 0.0;

	UPROPERTY(Transient)
	FFSHungItemMeshes PackHungItems;

	// How far through a stride the bob is, as a fraction of a stride, and how strong it is
	double BobPhase = 0.0;
	float BobWeight = 0.0f;

	FVector2D ItemJiggle = FVector2D::ZeroVector;
	FVector2D ItemJiggleVelocity = FVector2D::ZeroVector;
	FVector2D PackJiggle = FVector2D::ZeroVector;
	FVector2D PackJiggleVelocity = FVector2D::ZeroVector;

	// The carried item ShowInventory last showed, used to tell when it's been rotated
	TWeakObjectPtr<const UFSItemDefinition> ShownCarriedDefinition;
	int32 ShownCarriedId = INDEX_NONE;
	uint8 ShownCarriedRotation = 0;

	// How far the carried item's mesh is from its proper place as a turn starts: an angle about the view and a move on the grid. It closes as the time runs out.
	float CarriedItemTurnTimeLeft = 0.0f;
	double CarriedItemTurnAngle = 0.0;
	FVector CarriedItemTurnShift = FVector::ZeroVector;

	// Where the items in the hands and quick slots sit, which they sway about
	FTransform QuickSlotItemRests[QuickSlotCount];
	FTransform HandSlotItemRests[HandSlotCount];
	FTransform ArmorSlotItemRests[ArmorSlotCount];
	double SlotItemSwayTime = 0.0;

	// Made from BackgroundDarkenMaterial, so the rig can fade it
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackgroundDarkenInstance;
};
