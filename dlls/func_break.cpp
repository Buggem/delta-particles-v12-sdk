/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== bmodels.cpp ========================================================

  spawn, think, and use functions for entities that use brush models

*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "func_break.h"
#include "decals.h"
#include "explode.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;

// =================== FUNC_Breakable ==============================================

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname in the FGD so we can control which entities can
// be spawned, and still remain fairly flexible
const char *CBreakable::pSpawnObjects[] =
{
	NULL,				// 0
	"item_battery",		// 1
	"item_healthkit",	// 2
	"weapon_9mmhandgun",// 3
	"ammo_9mmclip",		// 4
	"weapon_m4a1",		// 5
	"ammo_556mmclip",	// 6
	"ammo_ARgrenades",	// 7
	"weapon_shotgun",	// 8
	"ammo_buckshot",	// 9
	"weapon_crossbow",	// 10
	"ammo_crossbow",	// 11
	"weapon_357",		// 12
	"ammo_357",			// 13
	"weapon_rpg",		// 14
	"ammo_rpgclip",		// 15
	"ammo_gaussclip",	// 16
	"weapon_handgrenade",// 17
	"weapon_tripmine",	// 18
	"weapon_satchel",	// 19
	"weapon_snark",		// 20
	"weapon_hornetgun",	// 21
	"weapon_44desert_eagle",	// 22
	"ammo_44clip",		// 23
	"weapon_smg",		// 24
	"ammo_45ACPclip",	// 25
	"ammo_weapon_barrett_m82a1",	// 25	
	"ammo_14mmclip",	// 27
};

void CBreakable::KeyValue( KeyValueData* pkvd )
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(pkvd->szKeyName, "explosion"))
	{
		if (!stricmp(pkvd->szValue, "directed"))
			m_Explosion = expDirected;
		else if (!stricmp(pkvd->szValue, "random"))
			m_Explosion = expRandom;
		else
			m_Explosion = expRandom;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi( pkvd->szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood

		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deadmodel"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shards"))
	{
//			m_iShards = atof(pkvd->szValue);
			pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel") )
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spawnobject") )
	{
		int object = atoi( pkvd->szValue );
		if ( object > 0 && object < ARRAYSIZE(pSpawnObjects) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "explodemagnitude") )
	{
		ExplosionSetMagnitude( atoi( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "lip") )
		pkvd->fHandled = TRUE;
	else if (FStrEq(pkvd->szKeyName, "respawn") ) //LRC
	{
		m_iRespawnTime = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "whenhit") ) //LRC
	{
		m_iszWhenHit = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iClass") )
	{
		m_iClass = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}


//
// func_breakable - bmodel that breaks into pieces after taking damage
//
LINK_ENTITY_TO_CLASS( func_breakable, CBreakable );
TYPEDESCRIPTION CBreakable::m_SaveData[] =
{
	DEFINE_FIELD( CBreakable, m_Material, FIELD_INTEGER ),
	DEFINE_FIELD( CBreakable, m_Explosion, FIELD_INTEGER ),

// Don't need to save/restore these because we precache after restore
//	DEFINE_FIELD( CBreakable, m_idShard, FIELD_INTEGER ),

	DEFINE_FIELD( CBreakable, m_angle, FIELD_FLOAT ),
	DEFINE_FIELD( CBreakable, m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( CBreakable, m_iszSpawnObject, FIELD_STRING ),

	// Explosion magnitude is stored in pev->impulse

	//LRC- time until respawn
	DEFINE_FIELD( CBreakable, m_iRespawnTime, FIELD_INTEGER ),
	//LRC- health to set on respawn
	DEFINE_FIELD( CBreakable, m_iInitialHealth, FIELD_INTEGER ),
	DEFINE_FIELD( CBreakable, m_iInitialRenderAmt, FIELD_INTEGER ),
	DEFINE_FIELD( CBreakable, m_iInitialRenderMode, FIELD_INTEGER ),
	DEFINE_FIELD( CBreakable, m_iszWhenHit, FIELD_STRING ),
	DEFINE_FIELD( CBreakable, m_pHitProxy, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CBreakable, CBaseEntity );

void CBreakable::Spawn( void )
{
    Precache( );    

	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )
		pev->takedamage	= DAMAGE_NO;
	else
		pev->takedamage	= DAMAGE_YES;
  
	if (m_iClass) //LRC - might these additions cause problems?
	{
		pev->flags |= FL_MONSTER; 
		pev->view_ofs = (pev->maxs + pev->mins) / 2;
	}

	if (m_iszWhenHit) //LRC - locus trigger
	{
		m_pHitProxy = GetClassPtr( (CPointEntity*)NULL );
	}

	pev->solid		= SOLID_BSP;
    pev->movetype	= MOVETYPE_PUSH;
    m_angle			= pev->angles.y;
	pev->angles.y	= 0;
	m_iInitialHealth = pev->health;
	m_iInitialRenderAmt = pev->renderamt;
	m_iInitialRenderMode = pev->rendermode;

	// HACK:  matGlass can receive decals, we need the client to know about this
	//  so use class to store the material flag
	if ( m_Material == matGlass )
	{
		pev->playerclass = 1;
	}

	SET_MODEL(ENT(pev), STRING(pev->model) );//set size and link into world.

	SetTouch(&CBreakable:: BreakTouch );
	SetUse(&CBreakable:: BreakUse );
	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )		// Only break on trigger
		SetTouch( NULL );

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if ( !IsBreakable() && pev->rendermode != kRenderNormal )
		pev->flags |= FL_WORLDBRUSH;

	if (m_iStyle >= 32)
		LIGHT_STYLE(m_iStyle, "z");
	else if (m_iStyle <= -32)
		LIGHT_STYLE(-m_iStyle, "a");
}

STATE CBreakable::GetState( void )
{
	if (m_iRespawnTime)
	{
		if (pev->effects & EF_NODRAW)
			return STATE_OFF;
		else
			return STATE_ON;
	}
	else return STATE_OFF;
}

const char *CBreakable::pSoundsWood[] = 
{
	"debris/wood1.wav",
	"debris/wood2.wav",
	"debris/wood3.wav",
};

const char *CBreakable::pSoundsFlesh[] = 
{
	"debris/flesh1.wav",
	"debris/flesh2.wav",
	"debris/flesh3.wav",
	"debris/flesh5.wav",
	"debris/flesh6.wav",
	"debris/flesh7.wav",
};

const char *CBreakable::pSoundsMetal[] = 
{
	"debris/metal1.wav",
	"debris/metal2.wav",
	"debris/metal3.wav",
};

const char *CBreakable::pSoundsConcrete[] = 
{
	"debris/concrete1.wav",
	"debris/concrete2.wav",
	"debris/concrete3.wav",
};


const char *CBreakable::pSoundsGlass[] = 
{
	"debris/glass1.wav",
	"debris/glass2.wav",
	"debris/glass3.wav",
};

const char *CBreakable::pSoundsShells[] = 
{
	"player/pl_shell1.wav",
	"player/pl_shell2.wav",
	"player/pl_shell3.wav",
};

const char **CBreakable::MaterialSoundList( Materials precacheMaterial, int &soundCount )
{
	const char	**pSoundList = NULL;

    switch ( precacheMaterial ) 
	{
	case matWood:
		pSoundList = pSoundsWood;
		soundCount = ARRAYSIZE(pSoundsWood);
		break;
	case matFlesh:
		pSoundList = pSoundsFlesh;
		soundCount = ARRAYSIZE(pSoundsFlesh);
		break;
	case matComputer:
	case matUnbreakableGlass:
	case matGlass:
		pSoundList = pSoundsGlass;
		soundCount = ARRAYSIZE(pSoundsGlass);
		break;

	case matMetal:
		pSoundList = pSoundsMetal;
		soundCount = ARRAYSIZE(pSoundsMetal);
		break;

	case matShells:
		pSoundList = pSoundsShells;
		soundCount = ARRAYSIZE(pSoundsShells);
		break;

	case matCinderBlock:
	case matRocks:
		pSoundList = pSoundsConcrete;
		soundCount = ARRAYSIZE(pSoundsConcrete);
		break;
	
	
	case matCeilingTile:
	case matNone:
	default:
		soundCount = 0;
		break;
	}

	return pSoundList;
}

void CBreakable::MaterialSoundPrecache( Materials precacheMaterial )
{
	const char	**pSoundList;
	int			i, soundCount = 0;

	pSoundList = MaterialSoundList( precacheMaterial, soundCount );

	for ( i = 0; i < soundCount; i++ )
	{
		PRECACHE_SOUND( (char *)pSoundList[i] );
	}
}

void CBreakable::MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume )
{
	const char	**pSoundList;
	int			soundCount = 0;

	pSoundList = MaterialSoundList( soundMaterial, soundCount );

	if ( soundCount )
		EMIT_SOUND( pEdict, CHAN_BODY, pSoundList[ RANDOM_LONG(0,soundCount-1) ], volume, 1.0 );
}


void CBreakable::Precache( void )
{
	const char *pGibName;

    switch (m_Material) 
	{
	case matWood:
		pGibName = "models/woodgibs.mdl";
		
		PRECACHE_SOUND("debris/bustcrate1.wav");
		PRECACHE_SOUND("debris/bustcrate2.wav");
		break;
	case matFlesh:
		pGibName = "models/fleshgibs.mdl";
		
		PRECACHE_SOUND("debris/bustflesh1.wav");
		PRECACHE_SOUND("debris/bustflesh2.wav");
		break;
	case matComputer:
		PRECACHE_SOUND("buttons/spark5.wav");
		PRECACHE_SOUND("buttons/spark6.wav");
		pGibName = "models/computergibs.mdl";
		
		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "models/glassgibs.mdl";
		
		PRECACHE_SOUND("debris/bustglass1.wav");
		PRECACHE_SOUND("debris/bustglass2.wav");
		break;
	case matMetal:
		pGibName = "models/metalplategibs.mdl";
		
		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;
	case matCinderBlock:
		pGibName = "models/cindergibs.mdl";
		
		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matRocks:
		pGibName = "models/rockgibs.mdl";
		
		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matCeilingTile:
		pGibName = "models/ceilinggibs.mdl";
		
		PRECACHE_SOUND ("debris/bustceiling.wav");  
		break;
	case matShells:
		
		PRECACHE_SOUND("player/pl_shell1.wav");
		PRECACHE_SOUND("player/pl_shell2.wav");
		PRECACHE_SOUND("player/pl_shell3.wav");
		break;
	}
	MaterialSoundPrecache( m_Material );
	if ( m_iszGibModel )
		pGibName = STRING(m_iszGibModel);

	m_idShard = PRECACHE_MODEL( (char *)pGibName );
	//ALERT(at_debug,"Breakable: Shard is %d\n",m_idShard);

	// Precache the spawn item's data
	if ( m_iszSpawnObject )
		UTIL_PrecacheOther( (char *)STRING( m_iszSpawnObject ) );
}

// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.


void CBreakable::DamageSound( void )
{
	int pitch;
	float fvol;
	char *rgpsz[6];
	int i;
	int material = m_Material;

//	if (RANDOM_LONG(0,1))
//		return;

	if (RANDOM_LONG(0,2))
		pitch = PITCH_NORM;
	else
		pitch = 95 + RANDOM_LONG(0,34);

	fvol = RANDOM_FLOAT(0.75, 1.0);

	if (material == matComputer && RANDOM_LONG(0,1))
		material = matMetal;

	switch (material)
	{
	case matComputer:
	case matGlass:
	case matUnbreakableGlass:
		rgpsz[0] = "debris/glass1.wav";
		rgpsz[1] = "debris/glass2.wav";
		rgpsz[2] = "debris/glass3.wav";
		i = 3;
		break;

	case matWood:
		rgpsz[0] = "debris/wood1.wav";
		rgpsz[1] = "debris/wood2.wav";
		rgpsz[2] = "debris/wood3.wav";
		i = 3;
		break;

	case matMetal:
		rgpsz[0] = "debris/metal1.wav";
		rgpsz[1] = "debris/metal3.wav";
		rgpsz[2] = "debris/metal2.wav";
		i = 2;
		break;

	case matFlesh:
		rgpsz[0] = "debris/flesh1.wav";
		rgpsz[1] = "debris/flesh2.wav";
		rgpsz[2] = "debris/flesh3.wav";
		rgpsz[3] = "debris/flesh5.wav";
		rgpsz[4] = "debris/flesh6.wav";
		rgpsz[5] = "debris/flesh7.wav";
		i = 6;
		break;

	case matRocks:
	case matCinderBlock:
		rgpsz[0] = "debris/concrete1.wav";
		rgpsz[1] = "debris/concrete2.wav";
		rgpsz[2] = "debris/concrete3.wav";
		i = 3;
		break;
		
	case matCeilingTile:
		// UNDONE: no ceiling tile shard sound yet
		i = 0;
		break;
	}

	if (i)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, rgpsz[RANDOM_LONG(0,i-1)], fvol, ATTN_NORM, 0, pitch);
}

void CBreakable::BreakTouch( CBaseEntity *pOther )
{
	float flDamage;
	entvars_t*	pevToucher = pOther->pev;
	
	// only players can break these right now
	if ( !pOther->IsPlayer() || !IsBreakable() )
	{
        return;
	}

	if ( FBitSet ( pev->spawnflags, SF_BREAK_TOUCH ) )
	{// can be broken when run into 
		flDamage = pevToucher->velocity.Length() * 0.01;

		if (flDamage >= pev->health)
		{
			SetTouch( NULL );
			TakeDamage(pevToucher, pevToucher, flDamage, DMG_CRUSH);

			// do a little damage to player if we broke glass or computer
			pOther->TakeDamage( pev, pev, flDamage/4, DMG_SLASH );
		}
	}

	if ( FBitSet ( pev->spawnflags, SF_BREAK_PRESSURE ) && pevToucher->absmin.z >= pev->maxs.z - 2 )
	{// can be broken when stood upon
		
		// play creaking sound here.
		DamageSound();

		SetThink(&CBreakable:: Die );
		SetTouch( NULL );
		
		if ( m_flDelay == 0 )
		{// !!!BUGBUG - why doesn't zero delay work?
			m_flDelay = 0.1;
		}

		SetNextThink( m_flDelay );

	}

}


//
// Smash the our breakable object
//

// Break when triggered
void CBreakable::BreakUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// for a respawnable entity, ON means someone wants it to respawn- but this one's solid already.
	if (m_iRespawnTime && useType == USE_ON)
		return;
	if ( IsBreakable() )
	{
		pev->angles.y = m_angle;
		UTIL_MakeVectors(pev->angles);
		g_vecAttackDir = gpGlobals->v_forward;

		Die();
	}
}

//LRC
void CBreakable::RespawnUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// OFF means someone wants it to break, but this one's broken already.
	if (useType == USE_OFF)
	{
		if (m_iRespawnTime > 0)
		{
			// reset my respawn time
			SetThink(&CBreakable:: RespawnThink );
			SetNextThink( m_iRespawnTime );
		}
		return;
	}
//	ALERT(at_debug,"Respawn trigger received\n");
	SetThink(&CBreakable:: RespawnThink );
	SetNextThink( 0.1 );
}

//LRC
void CBreakable::RespawnThink( void )
{
//	ALERT(at_debug,"RespawnThink: ");
	CBaseEntity *pList[2];
	int count = UTIL_EntitiesInBox( pList, 2, pev->mins, pev->maxs, FL_MONSTER | FL_CLIENT );
	if ( count )
	{
		// Can't respawn right now, a monster or player is in the way. Wait a bit.
//		ALERT(at_debug,"Respawn failed, count is %d\n",count);
		SetThink(&CBreakable:: RespawnThink );
		SetNextThink( 2 ); // CONSIDER: change this number?
	}
	else
	{
		// fade in, don't just appear(?)
		if (pev->spawnflags & SF_BREAK_FADE_RESPAWN)
		{
			SetThink(&CBreakable:: RespawnFadeThink );
			SetNextThink( 0.1 );
			pev->renderamt = 0;
			if (m_iInitialRenderMode == kRenderNormal)
			{
				pev->rendermode = kRenderTransTexture;
				m_iInitialRenderAmt = 255;
			}
		}
//		ALERT(at_debug,"Respawn OK\n");
		pev->solid = SOLID_BSP;
		pev->effects &= ~EF_NODRAW;
		pev->health = m_iInitialHealth;
		SetUse(&CBreakable:: BreakUse );
		if ( !FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )
			pev->takedamage	= DAMAGE_YES;

		// trigger the "fire on respawn" target
		FireTargets( STRING(pev->netname), this, this, USE_TOGGLE, 0);
	}
}

void CBreakable::RespawnFadeThink ( void )
{
	int newamt = min( pev->renderamt + 50, m_iInitialRenderAmt);
//	ALERT(at_debug, "FadeThink: %d changed to %d\n",pev->renderamt,newamt);
	pev->renderamt = newamt;
	if (pev->renderamt < m_iInitialRenderAmt)
	{
		SetNextThink( 0.1 );
	}
	else
	{
		pev->rendermode = m_iInitialRenderMode;
	}
}

NODE_LINKENT CBreakable::HandleLinkEnt(int afCapMask, bool nodeQueryStatic)
{
	if (nodeQueryStatic) {
		return NLE_ALLOW;
	}
	return NLE_PROHIBIT;
}

void CBreakable::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	// random spark if this is a 'computer' object
	if (RANDOM_LONG(0,1) )
	{
		switch( m_Material )
		{
			case matComputer:
			{
				UTIL_Sparks( ptr->vecEndPos );

				float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
				switch ( RANDOM_LONG(0,1) )
				{
					case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);	break;
					case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);	break;
				}
			}
			break;
			
			case matUnbreakableGlass:
				UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
			break;
		}
	}

	//LRC
	if (m_iszWhenHit)
	{
		m_pHitProxy->pev->origin = ptr->vecEndPos;
		m_pHitProxy->pev->velocity = vecDir;
		FireTargets( STRING(m_iszWhenHit), m_pHitProxy, this, USE_TOGGLE, 0 );
	}

	CBaseDelay::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// Special takedamage for func_breakable. Allows us to make
// exceptions that are breakable-specific
// bitsDamageType indicates the type of damage sustained ie: DMG_CRUSH
//=========================================================
int CBreakable :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
		
		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if ( FBitSet ( pevAttacker->flags, FL_CLIENT ) &&
				 FBitSet ( pev->spawnflags, SF_BREAK_CROWBAR ) && (bitsDamageType & DMG_CLUB))
			flDamage = pev->health;
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
	}
	
	if (!IsBreakable())
		return 0;

	// Breakables take double damage from the crowbar
	if ( bitsDamageType & DMG_CLUB )
		flDamage *= 2;

	// Boxes / glass / etc. don't take much poison damage, just the impact of the dart - consider that 10%
	if ( bitsDamageType & DMG_POISON )
		flDamage *= 0.1;

// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
// LRC - Die() does everything necessary
//		if (!m_iRespawnTime)
//		{
//			Killed( pevAttacker, GIB_NORMAL );
//		}
		Die();
		return 0;
	}

	// Make a shard noise each time func breakable is hit.
	// Don't play shard noise if cbreakable actually died.

	DamageSound();

	return 1;
}


void CBreakable::Die( void )
{
	Vector vecSpot;// shard origin
	Vector vecVelocity;// shard velocity
	CBaseEntity *pEntity = NULL;
	char cFlag = 0;
	int pitch;
	float fvol;
	
	pitch = 95 + RANDOM_LONG(0,29);

	if (pitch > 97 && pitch < 103)
		pitch = 100;

	// The more negative pev->health, the louder
	// the sound should be.

	fvol = RANDOM_FLOAT(0.85, 1.0) + (fabs(pev->health) / 100.0);

	if (fvol > 1.0)
		fvol = 1.0;


	switch (m_Material)
	{
	case matGlass:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustglass1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustglass2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_GLASS;
		break;

	case matWood:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustcrate1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustcrate2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_WOOD;
		break;

	case matComputer:
	case matMetal:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustmetal1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustmetal2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_METAL;
		break;

	case matFlesh:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_FLESH;
		break;

	case matRocks:
	case matCinderBlock:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustconcrete1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustconcrete2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_CONCRETE;
		break;

	case matCeilingTile:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustceiling.wav", fvol, ATTN_NORM, 0, pitch);
		break;
	}
    
		
	if (m_Explosion == expDirected)
		vecVelocity = g_vecAttackDir * 200;
	else
	{
		vecVelocity.x = 0;
		vecVelocity.y = 0;
		vecVelocity.z = 0;
	}

	vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );

		// size
		WRITE_COORD( pev->size.x);
		WRITE_COORD( pev->size.y);
		WRITE_COORD( pev->size.z);

		// velocity
		WRITE_COORD( vecVelocity.x ); 
		WRITE_COORD( vecVelocity.y );
		WRITE_COORD( vecVelocity.z );

		// randomization
		WRITE_BYTE( 10 ); 

		// Model
		WRITE_SHORT( m_idShard );	//model id#

		// # of shards
		WRITE_BYTE( 0 );	// let client decide

		// duration
		WRITE_BYTE( 25 );// 2.5 seconds

		// flags
		WRITE_BYTE( cFlag );
	MESSAGE_END();

	float size = pev->size.x;
	if ( size < pev->size.y )
		size = pev->size.y;
	if ( size < pev->size.z )
		size = pev->size.z;

	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 8 pixel high sheet
	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	mins.z = pev->absmax.z;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, 256, mins, maxs, FL_ONGROUND );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			ClearBits( pList[i]->pev->flags, FL_ONGROUND );
			pList[i]->pev->groundentity = NULL;
		}
	}

	// If I'm getting removed, don't fire something that could fire myself
	if (!m_iRespawnTime)
		pev->targetname = 0;

	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->takedamage = DAMAGE_NO;

	if (m_iStyle >= 32)
		LIGHT_STYLE(m_iStyle, "a");
	else if (m_iStyle <= -32)
		LIGHT_STYLE(-m_iStyle, "z");

	// Fire targets on break
	SUB_UseTargets( NULL, USE_TOGGLE, 0 );

	if (m_iRespawnTime == -1)
	{
//		ALERT(at_debug,"Waiting for respawn trigger\n");
		SetUse(&CBreakable:: RespawnUse );
	}
	else if (m_iRespawnTime)
	{
//		ALERT(at_debug,"Respawning in %d secs\n",m_iRespawnTime);
		SetThink(&CBreakable:: RespawnThink );
		SetNextThink( m_iRespawnTime );
	}
	else
	{
//		ALERT(at_debug,"No respawn\n");

		//tidy up
		if (m_pHitProxy)
		{
			m_pHitProxy->SetThink(&CBreakable::SUB_Remove );
			m_pHitProxy->SetNextThink( 0.1 );
			m_pHitProxy = NULL;
		}

	SetThink(&CBreakable::SUB_Remove );
		SetNextThink( 0.1 );
//		ALERT(at_console, "Set SUB_Remove\n");
	}

	if ( m_iszSpawnObject )
		CBaseEntity::Create( (char *)STRING(m_iszSpawnObject), VecBModelOrigin(pev), pev->angles, edict() );


	if ( Explodable() )
	{
		ExplosionCreate( Center(), pev->angles, edict(), ExplosionMagnitude(), TRUE );
	}
}



BOOL CBreakable :: IsBreakable( void ) 
{ 
	return m_Material != matUnbreakableGlass;
}


int	CBreakable :: DamageDecal( int bitsDamageType )
{
	if ( m_Material == matGlass  )
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0,2);

	if ( m_Material == matUnbreakableGlass )
		return DECAL_BPROOF1;

	return CBaseEntity::DamageDecal( bitsDamageType );
}


class CPushable : public CBreakable
{
public:
	void	Spawn ( void );
	void	Precache( void );
	void	Touch ( CBaseEntity *pOther );
	void	Move( CBaseEntity *pMover, int push );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	NODE_LINKENT HandleLinkEnt(int afCapMask, bool nodeQueryStatic);
	void	EXPORT StopSound( void );
//	virtual void	SetActivator( CBaseEntity *pActivator ) { m_pPusher = pActivator; }

	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_CONTINUOUS_USE; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	inline float MaxSpeed( void ) { return m_maxSpeed; }
	
	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	
	static	TYPEDESCRIPTION m_SaveData[];

	static char *m_soundNames[3];
	int		m_lastSound;	// no need to save/restore, just keeps the same sound from playing twice in a row
	float	m_maxSpeed;
	float	m_soundTime;
};

TYPEDESCRIPTION	CPushable::m_SaveData[] = 
{
	DEFINE_FIELD( CPushable, m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CPushable, m_soundTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CPushable, CBreakable );

LINK_ENTITY_TO_CLASS( func_pushable, CPushable );

char *CPushable :: m_soundNames[3] = { "debris/pushbox1.wav", "debris/pushbox2.wav", "debris/pushbox3.wav" };


void CPushable :: Spawn( void )
{
	Vector vecMins = pev->mins;
	Vector vecMaxs = pev->maxs;

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		CBreakable::Spawn();
	else
		Precache( );

	pev->movetype	= MOVETYPE_PUSHSTEP;
	pev->solid		= SOLID_BBOX;
	SET_MODEL( ENT(pev), STRING(pev->model) );

//	UTIL_SetSize( pev, vecMins, vecMaxs );

	if ( pev->friction > 399 )
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits( pev->flags, FL_FLOAT );
	pev->friction = 0;
	
	pev->origin.z += 1;	// Pick up off of the floor
	UTIL_SetOrigin( this, pev->origin );

	// Multiply by area of the box's cross-section (assume 1000 units^3 standard volume)
	pev->skin = ( pev->skin * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y) ) * 0.0005;
	m_soundTime = 0;
}


void CPushable :: Precache( void )
{
	for ( int i = 0; i < 3; i++ )
		PRECACHE_SOUND( m_soundNames[i] );

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		CBreakable::Precache( );
}


void CPushable :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "size") )
	{
		int bbox = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

		switch( bbox )
		{
		case 0:	// Point
			UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
			break;

		case 2: // Big Hull!?!?	!!!BUGBUG Figure out what this hull really is
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN*2, VEC_DUCK_HULL_MAX*2);
			break;

		case 3: // Player duck
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			break;

		default:
		case 1: // Player
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
			break;
		}

	}
	else if ( FStrEq(pkvd->szKeyName, "buoyancy") )
	{
		pev->skin = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBreakable::KeyValue( pkvd );
}


// Pull the func_pushable
void CPushable :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
	{
		if ( pev->spawnflags & SF_PUSH_BREAKABLE )
			this->CBreakable::Use( pActivator, pCaller, useType, value );
		return;
	}

	if ( pev->spawnflags & SF_PUSH_NOPULL ) return; //LRC: a non-pullable pushable.

	if ( pActivator->pev->velocity != g_vecZero )
		Move( pActivator, 0 );
}

NODE_LINKENT CPushable::HandleLinkEnt(int afCapMask, bool nodeQueryStatic)
{
	if (nodeQueryStatic) {
		return NLE_ALLOW;
	} else {
		return NLE_PROHIBIT;
	}
}

void CPushable :: Touch( CBaseEntity *pOther )
{
	if ( FClassnameIs( pOther->pev, "worldspawn" ) )
		return;

	Move( pOther, 1 );
}


void CPushable :: Move( CBaseEntity *pOther, int push )
{
	entvars_t*	pevToucher = pOther->pev;
	int playerTouch = 0;

	// Is entity standing on this pushable ?
	if ( FBitSet(pevToucher->flags,FL_ONGROUND) && pevToucher->groundentity && VARS(pevToucher->groundentity) == pev )
	{
		// Only push if floating
		if ( pev->waterlevel > 0 && pev->watertype > CONTENT_FLYFIELD)
			pev->velocity.z += pevToucher->velocity.z * 0.1;

		return;
	}


	if ( pOther->IsPlayer() )
	{
		if ( push && !(pevToucher->button & (IN_FORWARD|IN_USE)) )	// Don't push unless the player is pushing forward and NOT use (pull)
			return;
		playerTouch = 1;
	}

	float factor;

	if ( playerTouch )
	{
		if ( !(pevToucher->flags & FL_ONGROUND) )	// Don't push away from jumping/falling players unless in water
		{
			if ( pev->waterlevel < 1 || pev->watertype <= CONTENT_FLYFIELD)
				return;
			else 
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else 
		factor = 0.25;

	if (!push)
		factor = factor*0.5;

	pev->velocity.x += pevToucher->velocity.x * factor;
	pev->velocity.y += pevToucher->velocity.y * factor;

	float length = sqrt( pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y );
	if ( push && (length > MaxSpeed()) )
	{
		pev->velocity.x = (pev->velocity.x * MaxSpeed() / length );
		pev->velocity.y = (pev->velocity.y * MaxSpeed() / length );
	}
	if ( playerTouch )
	{
		pevToucher->velocity.x = pev->velocity.x;
		pevToucher->velocity.y = pev->velocity.y;
		if ( (gpGlobals->time - m_soundTime) > 0.7 )
		{
			m_soundTime = gpGlobals->time;
			if ( length > 0 && FBitSet(pev->flags,FL_ONGROUND) )
			{
				m_lastSound = RANDOM_LONG(0,2);
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound], 0.5, ATTN_NORM);
	//			SetThink( StopSound );
	//			SetNextThink( 0.1 );
			}
			else
				STOP_SOUND( ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound] );
		}
	}
}

#if 0
void CPushable::StopSound( void )
{
	Vector dist = pev->oldorigin - pev->origin;
	if ( dist.Length() <= 0 )
		STOP_SOUND( ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound] );
}
#endif

int CPushable::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		return CBreakable::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	return 1;
}

