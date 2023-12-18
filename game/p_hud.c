/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "g_local.h"



/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission (edict_t *ent)
{
	if (deathmatch->value || coop->value)
		ent->client->showscores = true;
	VectorCopy (level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = level.intermission_origin[0]*8;
	ent->client->ps.pmove.origin[1] = level.intermission_origin[1]*8;
	ent->client->ps.pmove.origin[2] = level.intermission_origin[2]*8;
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.gunindex = 0;
	ent->client->ps.blend[3] = 0;
	ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	// clean up powerup info
	ent->client->quad_framenum = 0;
	ent->client->invincible_framenum = 0;
	ent->client->breather_framenum = 0;
	ent->client->enviro_framenum = 0;
	ent->client->grenade_blew_up = false;
	ent->client->grenade_time = 0;

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;

	// add the layout

	if (deathmatch->value || coop->value)
	{
		DeathmatchScoreboardMessage (ent, NULL);
		gi.unicast (ent, true);
	}

}

void BeginIntermission (edict_t *targ)
{
	int		i, n;
	edict_t	*ent, *client;

	if (level.intermissiontime)
		return;		// already activated

	game.autosaved = false;

	// respawn any dead clients
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		if (client->health <= 0)
			respawn(client);
	}

	level.intermissiontime = level.time;
	level.changemap = targ->map;

	if (strstr(level.changemap, "*"))
	{
		if (coop->value)
		{
			for (i=0 ; i<maxclients->value ; i++)
			{
				client = g_edicts + 1 + i;
				if (!client->inuse)
					continue;
				// strip players of all keys between units
				for (n = 0; n < MAX_ITEMS; n++)
				{
					if (itemlist[n].flags & IT_KEY)
						client->client->pers.inventory[n] = 0;
				}
			}
		}
	}
	else
	{
		if (!deathmatch->value)
		{
			level.exitintermission = 1;		// go immediately to the next level
			return;
		}
	}

	level.exitintermission = 0;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission");
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission");
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		MoveClientToIntermission (client);
	}
}


/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage (edict_t *ent, edict_t *killer)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, j, k;
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		score, total;
	int		picnum;
	int		x, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	char	*tag;

	// sort the clients by score
	total = 0;
	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = g_edicts + 1 + i;
		if (!cl_ent->inuse || game.clients[i].resp.spectator)
			continue;
		score = game.clients[i].resp.score;
		for (j=0 ; j<total ; j++)
		{
			if (score > sortedscores[j])
				break;
		}
		for (k=total ; k>j ; k--)
		{
			sorted[k] = sorted[k-1];
			sortedscores[k] = sortedscores[k-1];
		}
		sorted[j] = i;
		sortedscores[j] = score;
		total++;
	}

	// print level name and exit rules
	string[0] = 0;

	stringlength = strlen(string);

	// add the clients in sorted order
	if (total > 12)
		total = 12;

	for (i=0 ; i<total ; i++)
	{
		cl = &game.clients[sorted[i]];
		cl_ent = g_edicts + 1 + sorted[i];

		picnum = gi.imageindex ("i_fixme");
		x = (i>=6) ? 160 : 0;
		y = 32 + 32 * (i%6);

		// add a dogtag
		if (cl_ent == ent)
			tag = "tag1";
		else if (cl_ent == killer)
			tag = "tag2";
		else
			tag = NULL;
		if (tag)
		{
			Com_sprintf (entry, sizeof(entry),
				"xv %i yv %i picn %s ",x+32, y, tag);
			j = strlen(entry);
			if (stringlength + j > 1024)
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
		}

		// send the layout
		Com_sprintf (entry, sizeof(entry),
			"client %i %i %i %i %i %i ",
			x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe)/600);
		j = strlen(entry);
		if (stringlength + j > 1024)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
void DeathmatchScoreboard (edict_t *ent)
{
	DeathmatchScoreboardMessage (ent, ent->enemy);
	gi.unicast (ent, true);
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f (edict_t *ent)
{
	ent->client->showinventory = false;
	ent->client->showhelp = false;

	if (!deathmatch->value && !coop->value)
		return;

	if (ent->client->showscores)
	{
		ent->client->showscores = false;
		return;
	}

	ent->client->showscores = true;
	DeathmatchScoreboard (ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk;
	gclient_t* cl = ent->client;

	if (skill->value == 0)
		sk = "easy";
	else if (skill->value == 1)
		sk = "medium";
	else if (skill->value == 2)
		sk = "hard";
	else
		sk = "hard+";

	char allyWinmon[32];
	StoreWinmonName(cl->winmonList[cl->winmonNum], allyWinmon);

	// send the layout
	Com_sprintf (string, sizeof(string),
		"xv 32 yv 8 picn help "			// background
		"xv 202 yv 12 string2 \"%s\" "		// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 62 cstring2 \"%s\" "		// help 1
		"xv 0 yv 70 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"Current Winmon: %s\" "		// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ", 
		sk,
		level.level_name,
		"welcome to winmon!",
		"shoot monsters with your",
		"blaster to enter fights.",
		allyWinmon,
		level.killed_monsters, level.total_monsters, 
		level.found_goals, level.total_goals,
		level.found_secrets, level.total_secrets);

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}

/*
==================
WinmonHud

Draw winmon hud.
==================
*/
void WinmonHud(edict_t* ent)
{
	char	string[2048];
	char* sk;
	gclient_t* cl = ent->client;

	char allyWinmon[32];
	StoreWinmonName(cl->winmonList[cl->winmonNum], allyWinmon);

	char enemyWinmon[32];
	StoreWinmonName(cl->winmonEnemy, enemyWinmon);

	char move1[32];
	char move2[32];
	char move3[32];
	char move4[32];

	StoreWinmonMoves(cl->winmonList[cl->winmonNum], move1, move2, move3, move4);

	// send the layout
	Com_sprintf(string, sizeof(string),
		"xv 32 yv 8 picn winmontest "					// background
		"xv 46 yv 12 string2 \"%s \" "		// skill
		"xv 46 yv 24 string2 \"level: %i\" "				// level name
		"xv 46 yv 32 string2 \"hp: %i\" "
		"xv 120 yv 50 string2 \"Q: Next turn\" "
		"xv 120 yv 50 string2 \"%s \" "
		"xv 200 yv 12 string2 \"%s \" "			// help 1
		"xv 200 yv 24 string2 \"level: 1\" "				// level name
		"xv 200 yv 32 string2 \"hp: %i\" "				// help 2
		"xv 36 yv 144 string2 \"J: %s \" "
		"xv 36 yv 156 string2 \"K: %s \" "
		"xv 36 yv 168 string2 \"L: %s \" "
		"xv 36 yv 180 string2 \"P: %s \" ",
		allyWinmon,
		cl->winmonLevels[cl->winmonNum],
		cl->currentWinmonHealth,
		cl->winmonMessage,
		enemyWinmon,
		cl->enemyWinmonHealth,
		move1,
		move2,
		move3,
		move4
	);

	gi.WriteByte(svc_layout);
	gi.WriteString(string);
	gi.unicast(ent, true);
}

void StoreWinmonMoves(int winmonNum, char* move1, char* move2, char* move3, char* move4) {
	if (winmonNum == 0) {
		memset(move1, 0, sizeof("Wackle: 10 dmg 100 acc"));
		strncpy(move1, "Wackle: 10 dmg 100 acc", sizeof("Wackle: 10 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wember: 20 dmg 100 acc"));
		strncpy(move2, "Wember: 20 dmg 100 acc", sizeof("Wember: 20 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wake Wown: 30 dmg 80 acc"));
		strncpy(move3, "Wake Wown: 30 dmg 80 acc", sizeof("Wake Wown: 30 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wail Whip"));
		strncpy(move4, "Wail Whip", sizeof("Wail Whip") - 1);
	}
	else if (winmonNum == 1) {
		memset(move1, 0, sizeof("Weint: 30 dmg 100 acc"));
		strncpy(move1, "Weint: 30 dmg 100 acc", sizeof("Weint: 30 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wame Wurst: 40 dmg 100 acc"));
		strncpy(move2, "Wame Wurst: 40 dmg 100 acc", sizeof("Wame Wurst: 40 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wire Wang: 60 dmg 80 acc"));
		strncpy(move3, "Wire Wang: 60 dmg 80 acc", sizeof("Wire Wang: 60 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wrowl"));
		strncpy(move4, "Wrowl", sizeof("Wrowl") - 1);
	}
	else if (winmonNum == 2) {
		memset(move1, 0, sizeof("Wolar Beam: 80 dmg 100 acc"));
		strncpy(move1, "Wolar Beam: 80 dmg 100 acc", sizeof("Wolar Beam: 80 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wamethrower: 60 dmg 100 acc"));
		strncpy(move2, "Wamethrower: 60 dmg 100 acc", sizeof("Wamethrower: 60 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wire Wlast: 100 dmg 80 acc"));
		strncpy(move3, "Wire Wlast: 100 dmg 80 acc", sizeof("Wire Wlast: 100 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wrotect"));
		strncpy(move4, "Wrotect", sizeof("Wrotect") - 1);
	}
	else if (winmonNum == 3) {
		memset(move1, 0, sizeof("Wackle: 10 dmg 100 acc"));
		strncpy(move1, "Wackle: 10 dmg 100 acc", sizeof("Wackle: 10 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Water Wun: 20 dmg 100 acc"));
		strncpy(move2, "Water Wun: 20 dmg 100 acc", sizeof("Water Wun: 20 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wake Wown: 30 dmg 80 acc"));
		strncpy(move3, "Wake Wown: 30 dmg 80 acc", sizeof("Wake Wown: 30 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wail Whip"));
		strncpy(move4, "Wail Whip", sizeof("Wail Whip") - 1);
	}
	else if (winmonNum == 4) {
		memset(move1, 0, sizeof("Weint: 30 dmg 100 acc"));
		strncpy(move1, "Weint: 30 dmg 100 acc", sizeof("Weint: 30 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wame Wurst: 40 dmg 100 acc"));
		strncpy(move2, "Water Wulse: 40 dmg 100 acc", sizeof("Water Wulse: 40 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wazor Whell: 60 dmg 80 acc"));
		strncpy(move3, "Wazor Whell: 60 dmg 80 acc", sizeof("Wazor Whell: 60 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wrowl"));
		strncpy(move4, "Wrowl", sizeof("Wrowl") - 1);
	}
	else if (winmonNum == 5) {
		memset(move1, 0, sizeof("Wetaliate: 80 dmg 100 acc"));
		strncpy(move1, "Wetaliate: 80 dmg 100 acc", sizeof("Wetaliate: 80 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wcald: 60 dmg 100 acc"));
		strncpy(move2, "Wcald: 60 dmg 100 acc", sizeof("Wcald: 60 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wydro Wump: 100 dmg 80 acc"));
		strncpy(move3, "Wydro Wump: 100 dmg 80 acc", sizeof("Wydro Wump: 100 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wrotect"));
		strncpy(move4, "Wrotect", sizeof("Wrotect") - 1);
	}
	else if (winmonNum == 6) {
		memset(move1, 0, sizeof("Wackle: 10 dmg 100 acc"));
		strncpy(move1, "Wackle: 10 dmg 100 acc", sizeof("Wackle: 10 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Weafage: 20 dmg 100 acc"));
		strncpy(move2, "Weafage: 20 dmg 100 acc", sizeof("Weafage: 20 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wake Wown: 30 dmg 80 acc"));
		strncpy(move3, "Wake Wown: 30 dmg 80 acc", sizeof("Wake Wown: 30 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wail Whip"));
		strncpy(move4, "Wail Whip", sizeof("Wail Whip") - 1);
	}
	else if (winmonNum == 7) {
		memset(move1, 0, sizeof("Wtomp: 30 dmg 100 acc"));
		strncpy(move1, "Wtomp: 30 dmg 100 acc", sizeof("Wtomp: 30 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wnergy Wall: 40 dmg 100 acc"));
		strncpy(move2, "Wnergy Wall: 40 dmg 100 acc", sizeof("Wnergy Wall: 40 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Wazor Weaf: 60 dmg 80 acc"));
		strncpy(move3, "Wazor Weaf: 60 dmg 80 acc", sizeof("Wazor Weaf: 60 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wrowl"));
		strncpy(move4, "Wrowl", sizeof("Wrowl") - 1);
	}
	else if (winmonNum == 8) {
		memset(move1, 0, sizeof("Wouble Wdge: 80 dmg 100 acc"));
		strncpy(move1, "Wouble Wdge: 80 dmg 100 acc", sizeof("Wouble Wdge: 80 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wetal Wlizzard: 60 dmg 100 acc"));
		strncpy(move2, "Wetal Wlizzard: 60 dmg 100 acc", sizeof("Wetal Wlizzard: 60 dmg 100 acc") - 1);
		memset(move3, 0, sizeof("Weaf Wtorm: 100 dmg 80 acc"));
		strncpy(move3, "Weaf Wtorm: 100 dmg 80 acc", sizeof("Weaf Wtorm: 100 dmg 80 acc") - 1);
		memset(move4, 0, sizeof("Wrotect"));
		strncpy(move4, "Wrotect", sizeof("Wrotect") - 1);
	}
	else if (winmonNum == 9) {
		memset(move1, 0, sizeof("Wrength: 40 dmg 100 acc"));
		strncpy(move1, "Wrength: 40 dmg 100 acc", sizeof("Wrength: 40 dmg 100 acc") - 1);
		memset(move2, 0, sizeof("Wrotect"));
		strncpy(move2, "Wrotect", sizeof("Wrotect") - 1);
		memset(move3, 0, sizeof("Wrowl"));
		strncpy(move3, "Wrowl", sizeof("Wrowl") - 1);
		memset(move4, 0, sizeof("Wail Whip"));
		strncpy(move4, "Wail Whip", sizeof("Wail Whip") - 1);
	}
}

void StoreWinmonName(int winmonNum, char* str) {
	if (winmonNum == 0) {
		memset(str, 0, sizeof("Wyndaquil"));
		strncpy(str, "Wyndaquil", sizeof("Wyndaquil") - 1);
	} 
	else if (winmonNum == 1) {
		memset(str, 0, sizeof("Wuilava"));
		strncpy(str, "Wuilava", sizeof("Wuilava") - 1);
	}
	else if (winmonNum == 2) {
		memset(str, 0, sizeof("Wyphlosion"));
		strncpy(str, "Wyphlosion", sizeof("Wyphlosion") - 1);
	}
	else if (winmonNum == 3) {
		memset(str, 0, sizeof("Wotodile"));
		strncpy(str, "Wotodile", sizeof("Wotodile") - 1);
	}
	else if (winmonNum == 4) {
		memset(str, 0, sizeof("Wroconaw"));
		strncpy(str, "Wroconaw", sizeof("Wroconaw") - 1);
	}
	else if (winmonNum == 5) {
		memset(str, 0, sizeof("Weraligatr"));
		strncpy(str, "Weraligatr", sizeof("Weraligatr") - 1);
	}
	else if (winmonNum == 6) {
		memset(str, 0, sizeof("Whikorita"));
		strncpy(str, "Whikorita", sizeof("Whikorita") - 1);
	}
	else if (winmonNum == 7) {
		memset(str, 0, sizeof("Wayleef"));
		strncpy(str, "Wayleef", sizeof("Wayleef") - 1);
	}
	else if (winmonNum == 8) {
		memset(str, 0, sizeof("Weganium"));
		strncpy(str, "Weganium", sizeof("Weganium") - 1);
	}
	else if (winmonNum == 9) {
		memset(str, 0, sizeof("Wevee"));
		strncpy(str, "Wevee", sizeof("Wevee") - 1);
	}
}

/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	// this is for backwards compatability
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged))
	{
		ent->client->showhelp = false;
		return;
	}

	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;


	if (ent->client->inWinmonFight) {
		WinmonHud(ent);
	}
	else {
		HelpComputer(ent);
	}
}

/*
==================
Cmd_Winmon_f

Display the current winmon hud
==================
*/
void Cmd_Winmon_f(edict_t* ent)
{

	ent->client->showinventory = false;
	ent->client->showscores = false;
	ent->client->showhelp = false;

	ent->client->showwinmon = true;
	WinmonHud(ent);
}

//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats (edict_t *ent)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;

	//
	// health
	//
	ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
	ent->client->ps.stats[STAT_HEALTH] = ent->health;

	//
	// ammo
	//
	if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */)
	{
		ent->client->ps.stats[STAT_AMMO_ICON] = 0;
		ent->client->ps.stats[STAT_AMMO] = 0;
	}
	else
	{
		item = &itemlist[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
	}
	
	//
	// armor
	//
	power_armor_type = PowerArmorType (ent);
	if (power_armor_type)
	{
		cells = ent->client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells == 0)
		{	// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
			power_armor_type = 0;;
		}
	}

	index = ArmorIndex (ent);
	if (power_armor_type && (!index || (level.framenum & 8) ) )
	{	// flash between power armor and other armor icon
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex ("i_powershield");
		ent->client->ps.stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex (index);
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
	}
	else
	{
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
		ent->client->ps.stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > ent->client->pickup_msg_time)
	{
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	//
	// timers
	//
	if (ent->client->quad_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_quad");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum)/10;
	}
	else if (ent->client->invincible_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_invulnerability");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum)/10;
	}
	else if (ent->client->enviro_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_envirosuit");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum)/10;
	}
	else if (ent->client->breather_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_rebreather");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum)/10;
	}
	else
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = 0;
		ent->client->ps.stats[STAT_TIMER] = 0;
	}

	//
	// selected item
	//
	if (ent->client->pers.selected_item == -1)
		ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
	else
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex (itemlist[ent->client->pers.selected_item].icon);

	ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

	//
	// layouts
	//
	ent->client->ps.stats[STAT_LAYOUTS] = 0;

	if (deathmatch->value)
	{
		if (ent->client->pers.health <= 0 || level.intermissiontime
			|| ent->client->showscores)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}
	else
	{
		if (ent->client->showscores || ent->client->showhelp)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}

	//
	// frags
	//
	ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

	//
	// help icon / current weapon if not shown
	//
	if (ent->client->pers.helpchanged && (level.framenum&8) )
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex ("i_help");
	else if ( (ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
		&& ent->client->pers.weapon)
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex (ent->client->pers.weapon->icon);
	else
		ent->client->ps.stats[STAT_HELPICON] = 0;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats (edict_t *ent)
{
	int i;
	gclient_t *cl;

	for (i = 1; i <= maxclients->value; i++) {
		cl = g_edicts[i].client;
		if (!g_edicts[i].inuse || cl->chase_target != ent)
			continue;
		memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
		G_SetSpectatorStats(g_edicts + i);
	}
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats (edict_t *ent)
{
	gclient_t *cl = ent->client;

	if (!cl->chase_target)
		G_SetStats (ent);

	cl->ps.stats[STAT_SPECTATOR] = 1;

	// layouts are independant in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;
	if (cl->pers.health <= 0 || level.intermissiontime || cl->showscores)
		cl->ps.stats[STAT_LAYOUTS] |= 1;
	if (cl->showinventory && cl->pers.health > 0)
		cl->ps.stats[STAT_LAYOUTS] |= 2;

	if (cl->chase_target && cl->chase_target->inuse)
		cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS + 
			(cl->chase_target - g_edicts) - 1;
	else
		cl->ps.stats[STAT_CHASE] = 0;
}

