/**
 * @file
 * @brief Miscellaneous debugging functions.
**/

#include "AppHdr.h"

#include "dbg-util.h"

#include "artefact.h"
#include "directn.h"
#include "dungeon.h"
#include "item-name.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "options.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"

monster_type debug_prompt_for_monster()
{
    char specs[1024];

    mprf(MSGCH_PROMPT, "Which monster by name? ");
    if (!cancellable_get_line_autohist(specs, sizeof specs))
    {
        if (specs[0] == '\0')
            return MONS_NO_MONSTER;

        return get_monster_by_name(specs, true);
    }
    return MONS_NO_MONSTER;
}

void debug_dump_levgen()
{
    if (crawl_state.game_is_arena())
        return;

    CrawlHashTable &props = env.properties;

    string method;
    string type;

    if (crawl_state.generating_level)
    {
        mpr("Currently generating level.");
        method = env.level_build_method;
        type   = comma_separated_line(env.level_layout_types.begin(),
                                      env.level_layout_types.end(), ", ");

        if (!env.placing_vault.empty())
            mprf("Vault being placed: %s", env.placing_vault.c_str());
        if (!env.new_subvault_names.empty())
        {
            mprf("Subvaults: %s", comma_separated_line(
                 env.new_subvault_names.begin(), env.new_subvault_names.end(),
                 ", ").c_str());
        }
    }
    else
    {
        if (!props.exists(BUILD_METHOD_KEY))
            method = "ABSENT";
        else
            method = props[BUILD_METHOD_KEY].get_string();

        if (!props.exists(LAYOUT_TYPE_KEY))
            type = "ABSENT";
        else
            type = props[LAYOUT_TYPE_KEY].get_string();
    }

    mprf("Level build method = %s, level layout type  = %s, absdepth0 = %d",
         method.c_str(), type.c_str(), env.absdepth0);

    if (!env.level_vaults.empty())
    {
        mpr("Level vaults:");
        for (auto &vault : env.level_vaults)
        {
            string vault_name = vault->map.name;
            if (vault->map.subvault_places.size())
            {
                vault_name += " [";
                for (unsigned int j = 0; j < vault->map.subvault_places.size(); ++j)
                {
                    vault_name += vault->map.subvault_places[j].subvault->name;
                    if (j < vault->map.subvault_places.size() - 1)
                        vault_name += ", ";
                }
                vault_name += "]";
            }
            mprf("    %s", vault_name.c_str());
        }
    }
    mpr("");
}

string debug_coord_str(const coord_def &pos)
{
    return make_stringf("(%d, %d)%s", pos.x, pos.y,
                        !in_bounds(pos) ? " <OoB>" : "");
}

string debug_mon_str(const monster* mon)
{
    const int midx = mon->mindex();
    if (invalid_monster_index(midx))
        return make_stringf("Invalid monster index %d", midx);

    string out = "Monster '" + mon->full_name(DESC_PLAIN) + "' ";
    out += make_stringf("%s [midx = %d]", debug_coord_str(mon->pos()).c_str(),
                        midx);

    return out;
}

static void _debug_mid_name(mid_t mid)
{
    if (mid == MID_PLAYER)
    {
        fprintf(stderr, "player %s",
                debug_coord_str(you.pos()).c_str());
    }
    else
    {
        monster * const mons = monster_by_mid(mid);
        if (mons)
            fprintf(stderr, "%s", debug_mon_str(mons).c_str());
        else
            fprintf(stderr, "bad monster[%" PRImidt"]", mid);
    }
}

void debug_dump_constriction(const actor *act)
{
    if (act->constricting)
    {
        for (const auto &entry : *act->constricting)
        {
            fprintf(stderr, "Constricting ");
            _debug_mid_name(entry.first);
            fprintf(stderr, " for %d ticks\n", entry.second);
        }
    }

    if (act->constricted_by)
    {
        fprintf(stderr, "Constricted by ");
        _debug_mid_name(act->constricted_by);
        fprintf(stderr, "\n");
    }
}

void debug_dump_mon(const monster* mon, bool recurse)
{
    const int midx = mon->mindex();
    if (invalid_monster_index(midx) || invalid_monster_type(mon->type))
        return;

    fprintf(stderr, "<<<<<<<<<\n");

    fprintf(stderr, "Name: %s\n", mon->name(DESC_PLAIN, true).c_str());
    fprintf(stderr, "Base name: %s\n",
            mon->base_name(DESC_PLAIN, true).c_str());
    fprintf(stderr, "Full name: %s\n\n",
            mon->full_name(DESC_PLAIN).c_str());

    if (in_bounds(mon->pos()))
    {
        string feat = raw_feature_description(mon->pos());
        fprintf(stderr, "On/in/over feature: %s\n\n", feat.c_str());
    }

    fprintf(stderr, "Foe: ");
    if (mon->foe == MHITNOT)
        fprintf(stderr, "none");
    else if (mon->foe == MHITYOU)
        fprintf(stderr, "player");
    else if (invalid_monster_index(mon->foe))
        fprintf(stderr, "invalid monster index %d", mon->foe);
    else if (mon->foe == midx)
        fprintf(stderr, "self");
    else
        fprintf(stderr, "%s", debug_mon_str(&menv[mon->foe]).c_str());

    fprintf(stderr, "\n");

    fprintf(stderr, "Target: ");
    if (mon->target.origin())
        fprintf(stderr, "none\n");
    else
        fprintf(stderr, "%s\n", debug_coord_str(mon->target).c_str());

    int target = MHITNOT;
    fprintf(stderr, "At target: ");
    if (mon->target.origin())
        fprintf(stderr, "N/A");
    else if (mon->target == you.pos())
    {
        fprintf(stderr, "player");
        target = MHITYOU;
    }
    else if (mon->target == mon->pos())
    {
        fprintf(stderr, "self");
        target = midx;
    }
    else if (in_bounds(mon->target))
    {
        target = mgrd(mon->target);

        if (target == NON_MONSTER)
            fprintf(stderr, "nothing");
        else if (target == midx)
            fprintf(stderr, "improperly linked self");
        else if (target == mon->foe)
            fprintf(stderr, "same as foe");
        else if (invalid_monster_index(target))
            fprintf(stderr, "invalid monster index %d", target);
        else
            fprintf(stderr, "%s", debug_mon_str(&menv[target]).c_str());
    }
    else
        fprintf(stderr, "<OoB>");

    fprintf(stderr, "\n");
    debug_dump_constriction(mon);

    if (mon->is_patrolling())
    {
        fprintf(stderr, "Patrolling: %s\n\n",
                debug_coord_str(mon->patrol_point).c_str());
    }

    if (mon->travel_target != MTRAV_NONE)
    {
        fprintf(stderr, "\nTravelling:\n");
        fprintf(stderr, "    travel_target      = %d\n", mon->travel_target);
        fprintf(stderr, "    travel_path.size() = %u\n",
                (unsigned int)mon->travel_path.size());

        if (!mon->travel_path.empty())
        {
            fprintf(stderr, "    next travel step: %s\n",
                    debug_coord_str(mon->travel_path.back()).c_str());
            fprintf(stderr, "    last travel step: %s\n",
                    debug_coord_str(mon->travel_path.front()).c_str());
        }
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "Inventory:\n");
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        const int idx = mon->inv[i];

        if (idx == NON_ITEM)
            continue;

        fprintf(stderr, "    slot #%d: ", i);

        if (idx < 0 || idx > MAX_ITEMS)
        {
            fprintf(stderr, "invalid item index %d\n", idx);
            continue;
        }
        const item_def &item(mitm[idx]);

        if (!item.defined())
        {
            fprintf(stderr, "invalid item\n");
            continue;
        }

        fprintf(stderr, "%s", item.name(DESC_PLAIN, false, true).c_str());

        if (!item.held_by_monster())
        {
            fprintf(stderr, " [not held by monster, pos = %s]",
                    debug_coord_str(item.pos).c_str());
        }
        else if (item.holding_monster() != mon)
        {
            fprintf(stderr, " [held by other monster: %s]",
                    debug_mon_str(item.holding_monster()).c_str());
        }

        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");


    bool found_spells = false;
    for (unsigned i = 0; i < mon->spells.size(); ++i)
    {
        spell_type spell = mon->spells[i].spell;

        if (!found_spells)
        {
            fprintf(stderr, "Spells:\n");
            found_spells = true;
        }

        fprintf(stderr, "    slot #%u: ", i);
        if (!is_valid_spell(spell))
            fprintf(stderr, "Invalid spell #%d\n", (int) spell);
        else
            fprintf(stderr, "%s\n", spell_title(spell));
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "attitude: %d, behaviour: %d, number: %d, flags: 0x%" PRIx64"\n",
            mon->attitude, mon->behaviour, mon->number, mon->flags.flags);

    fprintf(stderr, "colour: %d, foe_memory: %d, shield_blocks:%d, "
                  "experience: %u\n",
            mon->colour, mon->foe_memory, mon->shield_blocks, mon->experience);

    fprintf(stderr, "god: %s, seen_context: %d\n",
            god_name(mon->god).c_str(), mon->seen_context);

    fprintf(stderr, ">>>>>>>>>\n\n");

    if (!recurse)
        return;

    if (!invalid_monster_index(mon->foe) && mon->foe != midx
        && !invalid_monster_type(menv[mon->foe].type))
    {
        fprintf(stderr, "Foe:\n");
        debug_dump_mon(&menv[mon->foe], false);
    }

    if (!invalid_monster_index(target) && target != midx
        && target != mon->foe
        && !invalid_monster_type(menv[target].type))
    {
        fprintf(stderr, "Target:\n");
        debug_dump_mon(&menv[target], false);
    }
}

void debug_dump_item(const char *name, int num, const item_def &item,
                       const char *format, ...)
{
#ifdef DEBUG_FATAL
    const msg_channel_type chan = MSGCH_WARN;
#else
    const msg_channel_type chan = MSGCH_ERROR;
#endif

    va_list args;
    va_start(args, format);
    string msg = vmake_stringf(format, args);
    va_end(args);

    mprf(chan, "%s", msg.c_str());
    mprf(chan, "%s", name);

    mprf("    item #%d:  base: %d; sub: %d; plus: %d; plus2: %d; special: %d",
         num, item.base_type, item.sub_type,
         item.plus, item.plus2, item.special);

    mprf("    quant: %d; ident: 0x%08" PRIx32"; ident_type: %d",
         item.quantity, item.flags, get_ident_type(item));

    mprf("    x: %d; y: %d; link: %d", item.pos.x, item.pos.y, item.link);

#ifdef DEBUG_FATAL
    if (!crawl_state.game_crashed)
        die("%s %s", msg.c_str(), name);
#endif
    crawl_state.cancel_cmd_repeat();
}

skill_type debug_prompt_for_skill(const char *prompt)
{
    char specs[80];
    msgwin_get_line_autohist(prompt, specs, sizeof(specs));
    if (specs[0] == '\0')
        return SK_NONE;

    return skill_from_name(lowercase_string(specs).c_str());
}

/**
 * Get a skill type from a skill name, accepting abbreviations.
 *
 * @see str_to_skill for an exact version.
 */
skill_type skill_from_name(const char *name)
{
    skill_type skill = SK_NONE;

    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        string sk_name = lowercase_string(skill_name(sk));

        size_t pos = sk_name.find(name);
        if (pos != string::npos)
        {
            skill = sk;
            // We prefer prefixes over partial matches.
            if (!pos)
                break;
        }
    }

    return skill;
}

string debug_art_val_str(const item_def& item)
{
    ASSERT(is_artefact(item));

    return make_stringf("art val: %d, item val: %d",
                        artefact_value(item), item_value(item, true));
}

int debug_cap_stat(int stat)
{
    return stat < -128 ? -128 :
           stat >  127 ?  127
                       : stat;
}

#ifdef DEBUG
static FILE *debugf = 0;

void debuglog(const char *format, ...)
{
    va_list args;

    if (!debugf)
    {
        debugf = fopen("debuglog.txt", "w");
        ASSERT(debugf);
    }

    va_start(args, format);
    vfprintf(debugf, format, args);
    va_end(args);
    fflush(debugf);
}
#endif

#ifndef DEBUG_DIAGNOSTICS
void wizard_toggle_dprf()
{
    mpr("Diagnostic messages are available only in debug builds.");
}
#else
// Be sure to change enum diag_type in mpr.h to match.
static const char* diag_names[] =
{
    "normal",
    "level builder",
    "skill",
    "combat",
    "beam",
    "noise",
    "abyss",
    "monplace",
#ifdef DEBUG_MONSPEAK
    "speech",
#endif
#ifdef DEBUG_MONINDEX
    "monster index",
#endif
};

void wizard_toggle_dprf()
{
    COMPILE_CHECK(ARRAYSZ(diag_names) == NUM_DIAGNOSTICS);

    while (true)
    {
        string line;
        for (int i = 0; i < NUM_DIAGNOSTICS; i++)
        {
            line += make_stringf("%s[%c] %-10s%s ",
                                 Options.quiet_debug_messages[i] ? "<white>" : "",
                                 i + '0',
                                 diag_names[i],
                                 Options.quiet_debug_messages[i] ? "</white>" : "");
            if (i % 5 == 4 || i == NUM_DIAGNOSTICS - 1)
            {
                mprf(MSGCH_PROMPT, "%s", line.c_str());
                line.clear();
            }
        }
        mprf(MSGCH_PROMPT, "Toggle which debug class (ESC to exit)? ");

        int keyin = toalower(get_ch());

        if (key_is_escape(keyin) || keyin == ' '
            || keyin == '\r' || keyin == '\n')
        {
            canned_msg(MSG_OK);
            return;
        }

        if (keyin < '0' || keyin >= '0' + NUM_DIAGNOSTICS)
            continue;

        int diag = keyin - '0';
        Options.quiet_debug_messages.set(diag, !Options.quiet_debug_messages[diag]);
        mprf("%s messages will be %s.", diag_names[diag],
             Options.quiet_debug_messages[diag] ? "suppressed" : "printed");
        return;
    }
}
#endif
