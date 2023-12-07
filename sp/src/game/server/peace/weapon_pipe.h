//===============================//
//
// Purpose: Weapon pipe definition
//
//===============================//

#ifndef WEAPON_PIPE_H
#define WEAPON_PIPE_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#define PIPE_RANGE  75.0f
#define PIPE_REFIRE 0.4f

//-----------------------------------------------------------------------------
// CWeaponPipe
//-----------------------------------------------------------------------------

class CWeaponPipe : public CBaseHLBludgeonWeapon
{
public:
    DECLARE_CLASS(CWeaponPipe, CBaseHLBludgeonWeapon);

    DECLARE_SERVERCLASS();
    DECLARE_ACTTABLE();

    CWeaponPipe();

    float       GetRange(void)      { return PIPE_RANGE;  }
    float       GetFireRate(void)   { return PIPE_REFIRE; }

    void        AddViewKick(void);
    float       GetDamageForActivity(Activity hitActivity);

    virtual int WeaponMeleeAttack1Condition(float flDot, float flDist);
    void        SecondaryAttack(void) { return; }

    void ItemPostFrame(void);

    // Animation event
    virtual void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);

#ifdef MAPBASE
    // Don't use backup activities
    acttable_t* GetBackupActivityList()         { return NULL; }
    int         GetBackupActivityListCount()    { return 0; }
#endif

private:
    // Animation event handlers
    void HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator);

    bool  IsSwgStartSeqEnd = true;
    int   SwgStartSeqID;
    float SwgPressStartTime;
    bool  SwgWantToSwing;
};

#endif // WEAPON_PIPE_H
