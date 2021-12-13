/**
 * @file
 * @brief Cloud creating spells.
**/

#include "AppHdr.h"

#include "spl-clouds.h"

#include <algorithm>

#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "corpse.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "fight.h"
#include "items.h"
#include "level-state-type.h"
#include "message.h"
#include "mon-behv.h" // ME_WHACK
#include "ouch.h"
#include "random-pick.h"
#include "shout.h"
#include "spl-util.h"
#include "target.h"
#include "terrain.h"

spret conjure_flame(int pow, bool fail)
{
    cloud_struct* cloud = cloud_at(you.pos());

    if (cloud && !(cloud->type == CLOUD_FIRE || cloud->type == CLOUD_EMBERS))
    {
        mpr("There's already a cloud here!");
        return spret::abort;
    }

    fail_check();

    if (cloud && cloud->type == CLOUD_FIRE)
    {
        // Reinforce the cloud - but not too much.
        // It must be a fire cloud from a previous test.
        mpr("The fire blazes with new energy!");
        const int extra_dur = 2 + min(random2(pow) / 2, 20);
        cloud->decay += extra_dur * 5;
        cloud->source = you.mid ;
        cloud->set_whose(KC_YOU);
    }
    else if (cloud && cloud->type == CLOUD_EMBERS)
    {
        mpr("The fire ignites!");
        place_cloud(CLOUD_FIRE, you.pos(), min(5 + (random2(pow)/2)
                                                 + (random2(pow)/2), 23), &you);
    }
    else
    {
        you.props[CFLAME_DUR_KEY] = min(5 + (random2(pow)/2)
                                               + (random2(pow)/2), 23);
        place_cloud(CLOUD_EMBERS, you.pos(), 1, &you);
        // Create a cloud for the time it takes to cast plus 1 aut, so that no
        // matter what happens the flame tries to ignite after the next player
        // action.
        cloud = cloud_at(you.pos());
        cloud->decay = player_speed() + 1;
        mpr("The fire begins to smoulder!");
    }
    noisy(spell_effect_noise(SPELL_CONJURE_FLAME), you.pos());

    return spret::success;
}

spret cast_big_c(int pow, spell_type spl, const actor *caster, bolt &beam,
                      bool fail)
{
    if (grid_distance(beam.target, you.pos()) > beam.range
        || !in_bounds(beam.target))
    {
        mpr("That is beyond the maximum range.");
        return spret::abort;
    }

    if (cell_is_solid(beam.target))
    {
        const char *feat = feat_type_name(env.grid(beam.target));
        mprf("You can't place clouds on %s.", article_a(feat).c_str());
        return spret::abort;
    }

    cloud_type cty = CLOUD_NONE;
    //XXX: there should be a better way to specify beam cloud types
    switch (spl)
    {
        case SPELL_POISONOUS_CLOUD:
            beam.flavour = BEAM_POISON;
            beam.name = "blast of poison";
            cty = CLOUD_POISON;
            break;
        case SPELL_HOLY_BREATH:
            beam.flavour = BEAM_HOLY;
            beam.origin_spell = SPELL_HOLY_BREATH;
            cty = CLOUD_HOLY;
            break;
        case SPELL_FREEZING_CLOUD:
            beam.flavour = BEAM_COLD;
            beam.name = "freezing blast";
            cty = CLOUD_COLD;
            break;
        default:
            mpr("That kind of cloud doesn't exist!");
            return spret::abort;
    }

    beam.thrower           = KILL_YOU;
    beam.hit               = AUTOMATIC_HIT;
    beam.damage            = CONVENIENT_NONZERO_DAMAGE;
    beam.is_tracer         = true;
    beam.use_target_as_pos = true;
    beam.origin_spell      = spl;
    beam.affect_endpoint();
    if (beam.beam_cancelled)
        return spret::abort;

    fail_check();

    big_cloud(cty, caster, beam.target, pow, 8 + random2(3), -1);
    noisy(spell_effect_noise(spl), beam.target);
    return spret::success;
}

/*
 * A cloud_func that places an individual cloud as part of a cloud area. This
 * function is called by apply_area_cloud();
 *
 * @param where       The location of the cloud.
 * @param pow         The spellpower of the spell placing the clouds, which
 *                    determines how long the cloud will last.
 * @param spread_rate How quickly the cloud spreads.
 * @param ctype       The type of cloud to place.
 * @param agent       Any agent that may have caused the cloud. If this is the
 *                    player, god conducts are applied.
 * @param excl_rad    How large of an exclusion radius to make around the
 *                    cloud.
 * @returns           The number of clouds made, which is always 1.
 *
*/
static int _make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                                cloud_type ctype, const actor *agent,
                                int excl_rad)
{
    place_cloud(ctype, where,
                (3 + random2(pow / 4) + random2(pow / 4) + random2(pow / 4)),
                agent, spread_rate, excl_rad);

    return 1;
}

/*
 * Make a large area of clouds centered on a given place. This never creates
 * player exclusions.
 *
 * @param ctype       The type of cloud to place.
 * @param agent       Any agent that may have caused the cloud. If this is the
 *                    player, god conducts are applied.
 * @param where       The location of the cloud.
 * @param pow         The spellpower of the spell placing the clouds, which
 *                    determines how long the cloud will last.
 * @param size        How large a radius of clouds to place from the `where'
 *                    argument.
 * @param spread_rate How quickly the cloud spreads.
*/
void big_cloud(cloud_type cl_type, const actor *agent,
               const coord_def& where, int pow, int size, int spread_rate)
{
    // The starting point _may_ be a place no cloud can be placed on.
    apply_area_cloud(_make_a_normal_cloud, where, pow, size,
                     cl_type, agent, spread_rate, -1);
}

spret cast_corpse_rot(int pow, bool fail)
{
    fail_check();
    return corpse_rot(&you, pow);
}

spret corpse_rot(actor* caster, int pow, bool actual)
{
    // If there is no caster (god wrath), centre the effect on the player.
    const coord_def center = caster ? caster->pos() : you.pos();
    bool saw_rot = false;
    int did_rot = 0;

    for (radius_iterator ri(center, LOS_NO_TRANS); ri; ++ri)
    {
        for (stack_iterator si(*ri); si; ++si)
            if (si->is_type(OBJ_CORPSES, CORPSE_BODY))
            {
                if (!actual)
                    return spret::success;
                // Found a corpse. Skeletonise it if possible.
                if (!mons_skeleton(si->mon_type))
                {
                    item_was_destroyed(*si);
                    destroy_item(si->index());
                }
                else
                    turn_corpse_into_skeleton(*si);

                if (!saw_rot && you.see_cell(*ri))
                    saw_rot = true;

                ++did_rot;

                // Chance to get an extra cloud per corpse (50% at max power).
                if (x_chance_in_y(pow, 100))
                    ++did_rot;
            }
    }
    if (!actual)
        return spret::abort;

    for (fair_adjacent_iterator ai(center); ai; ++ai)
    {
        if (did_rot == 0)
            break;

        if (cell_is_solid(*ai) || cloud_at(*ai))
            continue;

        place_cloud(CLOUD_MIASMA, *ai, 2 + random2avg(8, 2), caster);
        --did_rot;
    }

    // Continue out to radius 2 if there are still corpses available.
    if (did_rot)
    {
        for (distance_iterator di(center, true, true, 2); di; ++di)
        {
            if (did_rot == 0)
                break;

            if (cell_is_solid(*di) || cloud_at(*di))
                continue;

            place_cloud(CLOUD_MIASMA, *di, 2 + random2avg(8, 2), caster);
            --did_rot;
        }
    }

    // Abort the spell for players; monsters and wrath fail silently
    if (saw_rot)
        mprf("You %s decay.", you.can_smell() ? "smell" : "sense");
    else if (!caster || caster->is_player())
        return spret::abort;

    return spret::success;
}

void holy_flames(monster* caster, actor* defender)
{
    const coord_def pos = defender->pos();
    int cloud_count = 0;
    const int dur = 8 + random2avg(caster->get_hit_dice() * 3, 2);

    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || cloud_at(*ai)
            || cell_is_solid(*ai)
            || is_sanctuary(*ai)
            || monster_at(*ai))
        {
            continue;
        }

        place_cloud(CLOUD_HOLY, *ai, dur, caster);

        cloud_count++;
    }

    if (cloud_count)
    {
        if (defender->is_player())
            mpr("Blessed fire suddenly surrounds you!");
        else
            simple_monster_message(*defender->as_monster(),
                                   " is surrounded by blessed fire!");
    }
}
