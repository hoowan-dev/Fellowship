// Copyright HoowanDev 2026 all rights reserved.


#include "Character/Enemies/FSPrickBehavior.h"
#include "Character/Enemies/FSEnemy.h"

DEFINE_LOG_CATEGORY_STATIC(LogFSPrick, Log, All);

void UFSPrickBehavior::Begin(AFSEnemy& InEnemy)
{
	Super::Begin(InEnemy);

	UE_LOG(LogFSPrick, Verbose, TEXT("%s woke up"), *InEnemy.GetName());
}

void UFSPrickBehavior::Tick(float DeltaTime)
{
	// TODO: the Prick's movement and AI
}

void UFSPrickBehavior::OnDamaged(float Damage, AActor* DamageCauser)
{
	// TODO: how the Prick reacts to being hit
	UE_LOG(LogFSPrick, Verbose, TEXT("%s took %.1f damage from %s"), *GetNameSafe(GetEnemy()), Damage, *GetNameSafe(DamageCauser));
}

void UFSPrickBehavior::OnDied()
{
	// TODO: anything the Prick does as it dies
	UE_LOG(LogFSPrick, Verbose, TEXT("%s died"), *GetNameSafe(GetEnemy()));
}
