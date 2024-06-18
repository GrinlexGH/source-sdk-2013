//=========================//
//
// Purpose: Weapon pipe code
//
// $NoKeywords: $
//
//=========================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "soundent.h"
#include "basebludgeonweapon.h"
#include "vstdlib/random.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "weapon_pipe.h"
#include "te_effect_dispatch.h"
#include "rumble_shared.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BLUDGEON_HULL_DIM       16

static const Vector g_bludgeonMins(-BLUDGEON_HULL_DIM, -BLUDGEON_HULL_DIM, -BLUDGEON_HULL_DIM);
static const Vector g_bludgeonMaxs(BLUDGEON_HULL_DIM, BLUDGEON_HULL_DIM, BLUDGEON_HULL_DIM);

ConVar    sk_plr_dmg_pipe("sk_plr_dmg_pipe", "10");
ConVar    sk_npc_dmg_pipe("sk_npc_dmg_pipe", "5");

//-----------------------------------------------------------------------------
// CWeaponPipe
//-----------------------------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST(CWeaponPipe, DT_WeaponPipe)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS(weapon_pipe, CWeaponPipe);
PRECACHE_WEAPON_REGISTER(weapon_pipe);

acttable_t CWeaponPipe::m_acttable[] =
{
    { ACT_MELEE_ATTACK1, ACT_MELEE_ATTACK_SWING, true },

    { ACT_IDLE, ACT_IDLE_ANGRY_MELEE, false },
    { ACT_IDLE_ANGRY, ACT_IDLE_ANGRY_MELEE, false },
#if EXPANDED_HL2_WEAPON_ACTIVITIES
    { ACT_RUN, ACT_RUN_MELEE, false },
    { ACT_WALK, ACT_WALK_MELEE, false },

    { ACT_ARM, ACT_ARM_MELEE, false },
    { ACT_DISARM, ACT_DISARM_MELEE, false },
#endif

#ifdef MAPBASE
    // HL2:DM activities (for third-person animations in SP)
    { ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
    { ACT_HL2MP_IDLE, ACT_HL2MP_IDLE_MELEE, false },
    { ACT_HL2MP_RUN, ACT_HL2MP_RUN_MELEE, false },
    { ACT_HL2MP_IDLE_CROUCH, ACT_HL2MP_IDLE_CROUCH_MELEE, false },
    { ACT_HL2MP_WALK_CROUCH, ACT_HL2MP_WALK_CROUCH_MELEE, false },
    { ACT_HL2MP_GESTURE_RANGE_ATTACK, ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE, false },
    { ACT_HL2MP_GESTURE_RELOAD, ACT_HL2MP_GESTURE_RELOAD_MELEE, false },
    { ACT_HL2MP_JUMP, ACT_HL2MP_JUMP_MELEE, false },
#if EXPANDED_HL2DM_ACTIVITIES
    { ACT_HL2MP_GESTURE_RANGE_ATTACK2, ACT_HL2MP_GESTURE_RANGE_ATTACK2_MELEE, false },
    { ACT_HL2MP_WALK, ACT_HL2MP_WALK_MELEE, false },
#endif
#endif
};

IMPLEMENT_ACTTABLE(CWeaponPipe);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponPipe::CWeaponPipe(void)
    :
    SwgStartSeqSuccEnd(false),
    SwgStartSeqShouldPlay(false),
    SwgStartSeqEnd(true),
    SwgWantToSwing(false),
    SwgPressStartTime(0),
    SwgStartSeqID(0)
{ }

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float CWeaponPipe::GetDamageForActivity(Activity hitActivity)
{
    if ((GetOwner() != NULL) && (GetOwner()->IsPlayer()))
        return sk_plr_dmg_pipe.GetFloat();

    return sk_npc_dmg_pipe.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Add in a view kick for this weapon
//-----------------------------------------------------------------------------
void CWeaponPipe::AddViewKick(void)
{
    CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

    if (pPlayer == NULL)
        return;

    QAngle punchAng;

    punchAng.x = random->RandomFloat(1.0f, 2.0f);
    punchAng.y = random->RandomFloat(-2.0f, -1.0f);
    punchAng.z = 0.0f;

    pPlayer->ViewPunch(punchAng);
}


//-----------------------------------------------------------------------------
// Attempt to lead the target (needed because citizens can't hit manhacks with the pipe!)
//-----------------------------------------------------------------------------
ConVar sk_pipe_lead_time("sk_pipe_lead_time", "0.9");

int CWeaponPipe::WeaponMeleeAttack1Condition(float flDot, float flDist)
{
    // Attempt to lead the target (needed because citizens can't hit manhacks with the pipe!)
    CAI_BaseNPC *pNPC = GetOwner()->MyNPCPointer();
    CBaseEntity *pEnemy = pNPC->GetEnemy();
    if (!pEnemy)
        return COND_NONE;

    Vector vecVelocity;
    vecVelocity = pEnemy->GetSmoothedVelocity();

    // Project where the enemy will be in a little while
    float dt = sk_pipe_lead_time.GetFloat();
    dt += random->RandomFloat(-0.3f, 0.2f);
    if (dt < 0.0f)
        dt = 0.0f;

    Vector vecExtrapolatedPos;
    VectorMA(pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos);

    Vector vecDelta;
    VectorSubtract(vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta);

    if (fabs(vecDelta.z) > 70)
    {
        return COND_TOO_FAR_TO_ATTACK;
    }

    Vector vecForward = pNPC->BodyDirection2D();
    vecDelta.z = 0.0f;
    float flExtrapolatedDist = Vector2DNormalize(vecDelta.AsVector2D());
    if ((flDist > 64) && (flExtrapolatedDist > 64))
    {
        return COND_TOO_FAR_TO_ATTACK;
    }

    float flExtrapolatedDot = DotProduct2D(vecDelta.AsVector2D(), vecForward.AsVector2D());
    if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
    {
        return COND_NOT_FACING_ATTACK;
    }

    return COND_CAN_MELEE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Animation event handlers
//-----------------------------------------------------------------------------
void CWeaponPipe::HandleAnimEventMeleeHit(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
    // Trace up or down based on where the enemy is...
    // But only if we're basically facing that direction
    Vector vecDirection;
    AngleVectors(GetAbsAngles(), &vecDirection);

    CBaseEntity *pEnemy = pOperator->MyNPCPointer() ? pOperator->MyNPCPointer()->GetEnemy() : NULL;
    if (pEnemy)
    {
        Vector vecDelta;
        VectorSubtract(pEnemy->WorldSpaceCenter(), pOperator->Weapon_ShootPosition(), vecDelta);
        VectorNormalize(vecDelta);

        Vector2D vecDelta2D = vecDelta.AsVector2D();
        Vector2DNormalize(vecDelta2D);
        if (DotProduct2D(vecDelta2D, vecDirection.AsVector2D()) > 0.8f)
        {
            vecDirection = vecDelta;
        }
    }

    Vector vecEnd;
    VectorMA(pOperator->Weapon_ShootPosition(), 50, vecDirection, vecEnd);
    CBaseEntity *pHurt = pOperator->CheckTraceHullAttack(pOperator->Weapon_ShootPosition(), vecEnd,
        Vector(-16, -16, -16), Vector(36, 36, 36), sk_npc_dmg_pipe.GetFloat(), DMG_CLUB, 0.75);

    // did I hit someone?
    if (pHurt)
    {
        // play sound
        WeaponSound(MELEE_HIT);

        // Fake a trace impact, so the effects work out like a player's crowbaw
        trace_t traceHit;
        UTIL_TraceLine(pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit);
        ImpactEffect(traceHit);
    }
    else
    {
        WeaponSound(MELEE_MISS);
    }
}


//-----------------------------------------------------------------------------
// Animation event
//-----------------------------------------------------------------------------
void CWeaponPipe::Operator_HandleAnimEvent(animevent_t *pEvent, CBaseCombatCharacter *pOperator)
{
    switch (pEvent->event)
    {
    case EVENT_WEAPON_MELEE_HIT:
        HandleAnimEventMeleeHit(pEvent, pOperator);
        break;

    default:
        BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
        break;
    }
}

void CWeaponPipe::ItemPreFrame() {
    CBasePlayer* pOwner = ToBasePlayer(GetOwner());

    if (!pOwner)
        return;

    if ((pOwner->m_afButtonPressed & IN_ATTACK2) && SwgStartSeqEnd) {
        SendWeaponAnim(ACT_VM_SWINGING);
        SwgStartSeqEnd = false;
        SwgStartSeqShouldPlay = true;
        SwgStartSeqID = GetSequence();
        SwgPressStartTime = gpGlobals->curtime;
    }

    if ((SwgStartSeqShouldPlay && (GetActivity() != ACT_VM_SWINGING))
        || (SwgStartSeqSuccEnd && (GetActivity() != ACT_VM_SWINGINGIDLE))) {
        SwgStartSeqShouldPlay = false;
        SwgStartSeqEnd = true;
        SwgStartSeqSuccEnd = false;
    }

    // >= -- animation is not finished
    // <= -- animation is finished
    if (SwgStartSeqShouldPlay && ((SwgPressStartTime + SequenceDuration(SwgStartSeqID)) <= gpGlobals->curtime)) {
        SwgStartSeqShouldPlay = false;
        SwgStartSeqEnd = true;
        SwgStartSeqSuccEnd = true;
        SendWeaponAnim(ACT_VM_SWINGINGIDLE);
    }

    if (pOwner->m_afButtonReleased & IN_ATTACK2 && SwgStartSeqSuccEnd) {
        int defValue = sk_plr_dmg_pipe.GetInt();
        sk_plr_dmg_pipe.SetValue(defValue * 3);
        BaseClass::PrimaryAttack();
        sk_plr_dmg_pipe.SetValue(defValue);
        SwgStartSeqSuccEnd = false;
    }

    if (SwgStartSeqShouldPlay) {
        if (pOwner->m_afButtonReleased & IN_ATTACK2)
            SwgWantToSwing = true;
        else if(pOwner->m_afButtonPressed & IN_ATTACK2)
            SwgWantToSwing = false;
    }

    if (SwgStartSeqSuccEnd && SwgWantToSwing) {
        SwgWantToSwing = false;
        int defValue = sk_plr_dmg_pipe.GetInt();
        sk_plr_dmg_pipe.SetValue(defValue * 3);
        BaseClass::PrimaryAttack();
        sk_plr_dmg_pipe.SetValue(defValue);
        SwgStartSeqSuccEnd = false;
    }

    BaseClass::ItemPreFrame();
}

void CWeaponPipe::SecondaryAttack() {
    return;
}

void CWeaponPipe::PrimaryAttack() {
    BaseClass::PrimaryAttack();
}
