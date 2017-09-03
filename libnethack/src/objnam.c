/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2016-02-17 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "artifact.h"

/* Summary of all NetHack's object naming functions:
   obj_typename(otyp): entry in discovery list, from player's point of view
   "scroll of mail (KIRJE)"
   simple_typename(otyp): object type's actual name or appearance, ignoring
   user-assigned names
   "scroll of mail" (identified); "scroll labeled KIRJE" (unidentified)
   distant_name(obj,func): name object as per func, except as if it isn't
   currently examinable by the user
   "potion", "scroll", "sword", "orange potion" (if previously viewed)
   fruitname(juice): name of material fruit is made from, or fruit juice
   e.g. with "slice of pizza": "pizza" (juice==0), "pizza juice" (juice == 1)
   xname(obj): general-use name as if player is viewing an object now
   "potions of sickness", "potion" (blind), "corpse", "orange potion" (unIDed)
   mshot_xname(obj): name of fired missile in a volley
   "the 2nd dagger", "the 4th arrow"
   doname(obj): fully detailed name of object or stack as seen in inventory
   "the blessed Amulet of Yendor (being worn)", "a poisoned +4 dagger"
   doname_price(obj): like doname, but with price info for shop items
   corpse_xname(obj, ignore_oquan): describe a corpse or pile of corpses
   "horse corpse", "horse corpses"
   cxname(obj): xname, but with specific corpse naming
   "potion of sickness", "horse corpses"
   killer_xname(obj): name from the game's view, minus info like BCU and
       greasedness, and any user-controlled portions in quotes
   "scroll of mail" (even if un-IDed)
   singular(obj,func): name one object of a stack as per func
   an(str): prefix "a" or "an" to str, if necessary
   An(str): prefix "A" or "An" to str, if necessary
   the(str): prefix "the" or "The" to str, if necessary
   The(str): prefix "the" or "The" to str, if necessary
   aobjnam(obj, verb): general-purpose name with precise stack sizes, and
   optional combined verb; otherwise like cxname
   "4 horse corpses", "3 orange potions shatter!", "speed boots burn"
   Tobjnam(obj, verb): general-purpose name with imprecise stack sizes,
   prepended "The", and optional combined verb; otherwise like xname
   "The corpses", "The orange potions shatter!", "The speed boots burn"
   otense(obj, verb): Conjugate verb as if obj was verbing
   "shatters" (stack size 1), "shatter" (stack size 2)
   vtense(subj, verb): Conjgate verb as if subj was verbing
   "they","shatter" -> "shatter"; "it","shatter" -> "shatters"
   Doname2(obj): doname() with leading capital
   "The blessed Amulet of Yendor (being worn)", "A poisoned +4 dagger"
   yname(obj): like xname(), but incorporates ownership details
   "your potions called Y", "Medusa's potion of oil", "the apple named X"
   Yname2(obj): yname() with leading capital
   "Your potions called Y", "Medusa's potion of oil", "The apple named X"
   ysimple_name(obj): like simple_typename(), with ownership details
   "your orange potions", "Medusa's potion of oil", "the apple"
   Ysimple_name2(obj): ysimple_name() with leading capital
   "Your orange potions", "Medusa's potion of oil", "The apple"
   makeplural(str): returns plural version of str
   "sheep" -> "sheep", "lump of sheep" -> "lumps of sheep", "mumak" -> "mumakil"
   makesingular(str): opposite of makeplural
   readobjname(str, default_obj, from_user): convert string to object
   if "nothing" is given, default_obj is returned
   cloak_simple_name(cloak): return vague description of given cloak
   "robe", "wrapping", "apron", "smock", "cloak"
   mimic_obj_name(monster): return object that mimic is mimicking
   "gold", "orange", "whatcha-may-callit" (mimic is mimicking a ])
 */

#define SCHAR_LIM 127

static boolean wishymatch(const char *, const char *, boolean);
static const char *add_erosion_words(const struct obj *obj, const char *);
static const char *xname2(const struct obj *obj,
                          boolean ignore_oquan, boolean mark_user);

struct Jitem {
    int item;
    const char *name;
};

/* true for gems/rocks that should have " stone" appended to their names */
#define GemStone(typ)   (typ == FLINT ||                                \
                         (objects[typ].oc_material == GEMSTONE &&       \
                          (typ != DILITHIUM_CRYSTAL && typ != RUBY &&   \
                           typ != DIAMOND && typ != SAPPHIRE &&         \
                           typ != BLACK_OPAL &&         \
                           typ != EMERALD && typ != OPAL)))


static const struct Jitem Japanese_items[] = {
    {SHORT_SWORD, "wakizashi"},
    {BROADSWORD, "ninja-to"},
    {FLAIL, "nunchaku"},
    {GLAIVE, "naginata"},
    {LOCK_PICK, "osaku"},
    {WOODEN_HARP, "koto"},
    {KNIFE, "shito"},
    {PLATE_MAIL, "tanko"},
    {HELMET, "kabuto"},
    {LEATHER_GLOVES, "yugake"},
    {FOOD_RATION, "gunyoki"},
    {POT_BOOZE, "sake"},
    {0, ""}
};


static const char *Japanese_item_name(int i);

struct opropdesc {
    uint64_t mask;
    enum youprop prop;
    const char *prefix;
    const char *shorthand; /* for 3+ properties */
    const char *suffix;
    const char *suffix_armor;
};

/* shorthand/suffix_armor uses prefix/suffix if empty */
static const struct opropdesc prop_desc[] = {
    {opm_reflects, REFLECTING, NULL, "reflect", "reflection",
     NULL},
    {opm_search, SEARCHING, NULL, "search", "searching", NULL},
    {opm_hunger, HUNGER, NULL, NULL, "hunger", NULL},
    {opm_stealth, STEALTH, NULL, NULL, "stealth", NULL},
    {opm_telepat, TELEPAT, NULL, NULL, "telepathy", NULL},
    {opm_fumble, FUMBLING, NULL, "fumble", "fumbling", NULL},
    {opm_warn, WARNING, NULL, "warn", "warning", NULL},
    {opm_aggravate, AGGRAVATE_MONSTER, NULL, "aggravates",
     "aggravate monster", NULL},
    {opm_fire, FIRE_RES, NULL, NULL, "fire", "fire resistance"},
    {opm_frost, COLD_RES, NULL, NULL, "frost", "cold resistance"},
    {opm_shock, SHOCK_RES, NULL, "shock", "lightning",
     "shock resistance"},
    {opm_drain, DRAIN_RES, "thirsty", "drain", NULL,
     "drain resistance"},
    {opm_speed, FAST, NULL, NULL, "speed", NULL},
    {opm_oilskin, 0, "oilskin", NULL, NULL, NULL},
    {opm_power, 0, NULL, NULL, "power", NULL},
    {opm_dexterity, 0, NULL, "dex", "dexterity", NULL},
    {opm_brilliance, 0, NULL, "brill", "brilliance", NULL},
    {opm_displacement, DISPLACED, NULL, "displace",
     "displacement", NULL},
    {opm_clairvoyant, CLAIRVOYANT, NULL, "clairvoyant",
     "clairvoyance", NULL},
    {opm_mercy, 0, NULL, NULL,
     "mercy", NULL},
    {opm_carrying, 0, NULL, "carry",
     "carrying", NULL},
    {opm_nasty, 0, "nasty", NULL, NULL, NULL},
    /* these below use "versus x" instead of "of x" for armor,
       checked via opm_vorpal */
    {opm_vorpal, 0, "vorpal", NULL, NULL, "beheading"},
    {opm_detonate, 0, NULL, "detonate", "detonation",
     "explosions"},
    {opm_none, 0, NULL, NULL, NULL, NULL},
};

/* Runs property updates for every property provided by object */
void
update_property_for_oprops(struct monst *mon, struct obj *obj,
                           enum objslot slot)
{
    uint64_t props = obj_properties(obj);
    if (!props)
        return;

    const struct opropdesc *desc;

    for (desc = prop_desc; desc->mask != opm_none; desc++)
        if ((props & desc->mask) && desc->prop) {
            if (update_property(mon, desc->prop, slot))
                learn_oprop(obj, desc->mask);
        }
}

uint64_t
filter_redundant_oprops(const struct obj *obj,
                        uint64_t props)
{
    const struct opropdesc *desc;
    int dummy; /* needed for item_provides_extrinsic */

    for (desc = prop_desc; desc->mask != opm_none; desc++)
        if ((props & desc->mask) && desc->prop &&
            item_provides_extrinsic_before_oprop(obj, desc->prop,
                                                 &dummy))
            props &= ~(desc->mask);

    return props;
}

boolean
obj_oprops_provides_property(const struct obj *obj,
                             enum youprop prop)
{
    uint64_t props = obj_properties(obj);
    if (!props)
        return FALSE;

    const struct opropdesc *desc;

    for (desc = prop_desc; desc->mask != opm_none; desc++)
        if (desc->prop == prop)
            return !!(props & desc->mask);

    return FALSE;
}

static const char *
display_oprops(const struct obj *obj, const char *input)
{
    if (!obj)
        return "";

    const char *buf = input;
    uint64_t props = obj_properties(obj);
    int prop_amount = 0;

    if (props != (props & obj->oprops_known)) {
        prop_amount++;
        buf = msgcat("magical ", buf);
        props &= obj->oprops_known;
    }

    if (!props)
        return buf;

    const struct opropdesc *desc;
    const struct opropdesc *olddesc = NULL;
    int suffixes = 0;

    struct objclass *ocl = &objects[obj->otyp];
    int known = ocl->oc_name_known;

    /* TODO: this can probably be made saner */
    if (strstr(buf, " of ") &&
        (!strstr(buf, "pair of") ||
         strstr(buf, "gauntlets of")) &&
        !strstr(buf, "set of") &&
        !strstr(buf, "tin of"))
        suffixes++;
    boolean use_armor = TRUE;
    boolean versus = FALSE;
    if (obj->oclass == WEAPON_CLASS || is_weptool(obj))
        use_armor = FALSE;

    for (desc = prop_desc; desc->mask != opm_none; desc++) {
        if (desc->mask == opm_vorpal && use_armor) {
            suffixes = 0;
            versus = TRUE;
            if (olddesc) {
                if (!use_armor || !olddesc->suffix_armor)
                    buf = msgcat_many(buf, " and ",
                                      olddesc->suffix, NULL);
                else
                    buf = msgcat_many(buf, " and ",
                                      olddesc->suffix_armor,
                                      NULL);
                olddesc = NULL;
            }
        }
        if (props & desc->mask) {
            prop_amount++;
            if (prop_amount > 4)
                break;

            if (desc->suffix ||
                (desc->suffix_armor && use_armor)) {
                if (olddesc) {
                    if (!use_armor || !olddesc->suffix_armor)
                        buf = msgcat_many(buf, ", ",
                                          olddesc->suffix, NULL);
                    else
                        buf = msgcat_many(buf, ", ",
                                          olddesc->suffix_armor,
                                          NULL);
                }

                if (!suffixes) {
                    if (!use_armor || !desc->suffix_armor)
                        buf = msgcat_many(buf,
                                          versus ? " versus " :
                                          " of ", desc->suffix,
                                          NULL);
                    else
                        buf = msgcat_many(buf,
                                          versus ? " versus " :
                                          " of ",
                                          desc->suffix_armor,
                                          NULL);
                } else
                    olddesc = desc;

                suffixes++;
            } else
                buf = msgcat_many(desc->prefix, " ", buf,
                                  NULL);
        }
    }

    if (olddesc) {
        buf = msgcat_many(buf, " and ",
                          !use_armor || !olddesc->suffix_armor ?
                          olddesc->suffix : olddesc->suffix_armor,
                          NULL);
        olddesc = NULL;
    }

    if (prop_amount <= 4)
        return buf;

    /* Unusually many properties, use shorthand */
    buf = msgcat(input, " (");
    prop_amount = 0;
    for (desc = prop_desc; desc->mask != opm_none; desc++) {
        if (props & desc->mask) {
            if (prop_amount)
                buf = msgcat(buf, ",");

            buf = msgcat(buf,
                         desc->shorthand ? desc->shorthand :
                         desc->prefix ? desc->prefix :
                         desc->suffix ? desc->suffix :
                         "???");
            prop_amount++;
        }
    }

    if (props != obj->oprops)
        buf = msgcat(buf, "+more");

    buf = msgcat(buf, ")");
    return buf;
}

const char *
obj_typename(int otyp)
{
    const char *buf;
    struct objclass *ocl = &objects[otyp];
    const char *actualn = OBJ_NAME(*ocl);
    const char *dn = OBJ_DESCR(*ocl);
    const char *un = ocl->oc_uname;
    int nn = ocl->oc_name_known;

    if (Role_if(PM_SAMURAI) && Japanese_item_name(otyp))
        actualn = Japanese_item_name(otyp);
    switch (ocl->oc_class) {
    case COIN_CLASS:
        buf = "coin";
        break;
    case POTION_CLASS:
        buf = "potion";
        break;
    case SCROLL_CLASS:
        buf = "scroll";
        break;
    case WAND_CLASS:
        buf = "wand";
        break;
    case SPBOOK_CLASS:
        buf = "spellbook";
        break;
    case RING_CLASS:
        buf = "ring";
        break;
    case AMULET_CLASS:
        if (nn)
            buf = actualn;
        else
            buf = "amulet";
        if (un)
            buf = msgcat_many(buf, " called ", un, NULL);
        if (dn)
            buf = msgprintf("%s (%s)", buf, dn);
        return buf;
    default:
        if (nn) {
            buf = actualn;
            if (GemStone(otyp))
                buf = msgcat(buf, " stone");
            if (un)
                buf = msgcat_many(buf, " called ", un, NULL);
            if (dn)
                buf = msgprintf("%s (%s)", buf, dn);
        } else {
            buf = dn ? dn : actualn;
            if (ocl->oc_class == GEM_CLASS)
                buf = msgcat(buf, (ocl->oc_material == MINERAL) ?
                             " stone" : " gem");
            if (un)
                buf = msgcat_many(buf, " called ", un, NULL);
        }
        return buf;
    }
    /* here for ring/scroll/potion/wand */
    if (nn) {
        if (ocl->oc_unique)
            buf = actualn;          /* avoid spellbook of Book of the Dead */
        else
            buf = msgcat_many(buf, " of ", actualn, NULL);
    }
    if (un)
        buf = msgcat_many(buf, " called ", un, NULL);
    if (dn)
        buf = msgprintf("%s (%s)", buf, dn);
    return buf;
}

/* less verbose result than obj_typename(); either the actual name
   or the description (but not both); user-assigned name is ignored */
const char *
simple_typename(int otyp)
{
    const char *bufp, *pp;
    char *save_uname = objects[otyp].oc_uname;

    objects[otyp].oc_uname = 0; /* suppress any name given by user */
    bufp = obj_typename(otyp);
    objects[otyp].oc_uname = save_uname;
    if ((pp = strstri(bufp, " (")) != 0)
        return msgchop(bufp, pp - bufp);
    return bufp;
}

boolean
obj_is_pname(const struct obj * obj)
{
    return ((boolean) (obj->dknown && obj->known && ox_name(obj) &&
                       /* Since there aren't any objects which are both
                          artifacts and unique, the last check is redundant. */
                       obj->oartifact && !objects[obj->otyp].oc_unique));
}

static boolean dont_reveal_dknown = FALSE;

/* Give the name of an object seen at a distance. Unlike xname/doname, we don't
   want to set dknown if it's not set already. */
const char *
distant_name(const struct obj *obj, const char *(*func) (const struct obj *))
{
    const char *str;

    /* We want less globals, not more. However, a global is less kludgy here than
       messing with blindness state (which also breaks if EotO is involved) */
    dont_reveal_dknown = TRUE;
    str = (*func) (obj);
    dont_reveal_dknown = FALSE;
    return str;
}

/* convert player specified fruit name into corresponding fruit juice name
   ("slice of pizza" -> "pizza juice" rather than "slice of pizza juice") */
/* juice: whether or not to append " juice" to the name */
const char *
fruitname(boolean juice)
{
    const char *fruit_nam = strstri(gamestate.fruits.curname, " of ");

    if (fruit_nam)
        fruit_nam += 4; /* skip past " of " */
    else
        fruit_nam = gamestate.fruits.curname;   /* use it as is */

    return msgprintf("%s%s", makesingular(fruit_nam), juice ? " juice" : "");
}


/* Basic first look at the object; this used to be part of xname. Examining an
   object this way happens automatically and does not involve touching (no
   stoning). */
void
examine_object(struct obj *obj)
{
    int typ = obj->otyp;
    struct objclass *ocl = &objects[typ];
    int nn = ocl->oc_name_known;

    /* clean up known when it's tied to oc_name_known, eg after AD_DRIN */
    if (!nn && ocl->oc_uses_known && ocl->oc_unique)
        obj->known = 0;
    if ((!Blind && !dont_reveal_dknown) ||
        (!corpsenm_is_relevant(typ) && !has_shuffled_appearance(typ)))
        obj->dknown = TRUE;
    if (Role_if(PM_PRIEST))
        obj->bknown = TRUE;
    if (weapon_type(obj) != P_NONE && P_SKILL(weapon_type(obj)) >= P_EXPERT)
        obj->known = TRUE;
}


const char *
xname(const struct obj *obj)
{
    return xname2(obj, FALSE, FALSE);
}


static const char *
xname2(const struct obj *obj, boolean ignore_oquan, boolean mark_user)
{
    const char *buf;
    int typ = obj->otyp;
    struct objclass *ocl = &objects[typ];
    int nn = ocl->oc_name_known;
    const char *actualn = OBJ_NAME(*ocl);
    const char *dn = OBJ_DESCR(*ocl);
    const char *un = ocl->oc_uname;
    boolean known = obj->known;
    boolean dknown = obj->dknown;
    boolean bknown = obj->bknown;

    if (Role_if(PM_SAMURAI) && Japanese_item_name(typ))
        actualn = Japanese_item_name(typ);

    buf = "";

    if (un && mark_user)
        un = msgcat_many("\"", un, "\"", NULL);

    /* Clean up known when it's tied to oc_name_known, eg after AD_DRIN. This
       is only required for unique objects since the article printed for the
       object is tied to the combination of the two and printing the wrong
       article gives away information.

       After the amnesia changes, it is likely that this is dead code, but it
       won't do any harm to leave it around in case amnesia is changed again. */
    if (!nn && ocl->oc_uses_known && ocl->oc_unique)
        known = 0;
    if (!Blind && dont_reveal_dknown)
        dknown = TRUE;
    if (Role_if(PM_PRIEST))
        bknown = TRUE;
    if (obj_is_pname(obj))
        goto nameit;

    /* called is after oprops so we need to know if we
       should display it before actually doing it to avoid
       duplicate if expressions */
    boolean do_un = FALSE;

    switch (obj->oclass) {
    case AMULET_CLASS:
        if (!dknown)
            buf = "amulet";
        else if (typ == AMULET_OF_YENDOR || typ == FAKE_AMULET_OF_YENDOR)
            /* each must be identified individually */
            buf = known ? actualn : dn;
        else if (nn)
            buf = actualn;
        else if (un) {
            buf = "amulet";
            do_un = TRUE;
        } else
            buf = msgcat(dn, " amulet");

        buf = display_oprops(obj, buf);
        if (do_un)
            buf = msgcat_many(buf, " called ", un, NULL);
        break;

    case WEAPON_CLASS:
        if (is_poisonable(obj) && obj->opoisoned)
            buf = "poisoned ";
        /* fall through */
    case VENOM_CLASS:
    case TOOL_CLASS:
        if (typ == LENSES)
            buf = "pair of ";

        if (!dknown)
            buf = msgcat(buf, dn ? dn : actualn);
        else if (nn)
            buf = msgcat(buf, actualn);
        else if (un) {
            buf = msgcat(buf, dn ? dn : actualn);
            do_un = TRUE;
        } else
            buf = msgcat(buf, dn ? dn : actualn);

        buf = display_oprops(obj, buf);
        if (do_un)
            buf = msgcat_many(buf, " called ", un, NULL);

        if (typ == FIGURINE)
            buf = msgcat_many(buf, " of ", an(mons[obj->corpsenm].mname), NULL);
        break;

    case ARMOR_CLASS:
        /* depends on order of the dragon scales objects */
        if (typ >= GRAY_DRAGON_SCALES && typ <= YELLOW_DRAGON_SCALES) {
            buf = msgcat("set of ", actualn);
            break;
        }
        if (is_boots(obj) || is_gloves(obj))
            buf = "pair of ";

        if (obj->otyp >= ELVEN_SHIELD && obj->otyp <= ORCISH_SHIELD &&
            !dknown) {
            buf = "shield";
            break;
        }
        if (obj->otyp == SHIELD_OF_REFLECTION && !dknown) {
            buf = "smooth shield";
            break;
        }

        if (nn)
            buf = msgcat(buf, actualn);
        else if (un) {
            if (is_boots(obj))
                buf = msgcat(buf, "boots");
            else if (is_gloves(obj))
                buf = msgcat(buf, "gloves");
            else if (is_cloak(obj))
                buf = "cloak";
            else if (is_helmet(obj))
                buf = helmet_name(obj);
            else if (is_shield(obj))
                buf = "shield";
            else
                buf = "armor";
            do_un = TRUE;
        } else
            buf = msgcat(buf, dn);

        buf = display_oprops(obj, buf);
        if (do_un)
            buf = msgcat_many(buf, " called ", un, NULL);
        break;

    case FOOD_CLASS:
        if (typ == SLIME_MOLD) {
            struct fruit *f;

            for (f = gamestate.fruits.chain; f; f = f->nextf) {
                if (f->fid == obj->spe) {
                    buf = msg_from_string(f->fname);
                    break;
                }
            }
            if (!f)
                impossible("Bad fruit #%d?", obj->spe);
            if (mark_user)
                buf = msgcat_many("\"", buf, "\"", NULL);
            break;
        }

        buf = actualn;
        if (typ == TIN && known) {
            if (obj->spe > 0)
                buf = "tin of spinach";
            else if (obj->corpsenm == NON_PM)
                buf = "empty tin";
            else if (vegetarian(&mons[obj->corpsenm]))
                buf = msgcat_many(buf, " of ", mons[obj->corpsenm].mname, NULL);
            else
                buf = msgcat_many(buf, " of ", mons[obj->corpsenm].mname,
                                  " meat", NULL);
        }

        /* meat rings */
        buf = display_oprops(obj, buf);
        break;

    case COIN_CLASS:
    case CHAIN_CLASS:
        buf = actualn;
        break;

    case ROCK_CLASS:
        if (typ == STATUE)
            buf = msgprintf("%s%s of %s%s",
                            (Role_if(PM_ARCHEOLOGIST) &&
                             (obj->spe & STATUE_HISTORIC)) ? "historic " : "",
                            actualn,
                            type_is_pname(&mons[obj->corpsenm]) ? "" :
                            (mons[obj->corpsenm].geno & G_UNIQ) ? "the " :
                            (strchr(vowels, *(mons[obj->corpsenm].mname)) ?
                             "an " : "a "),
                            mons[obj->corpsenm].mname);
        else
            buf = actualn;

        buf = display_oprops(obj, buf);
        break;

    case BALL_CLASS:
        /* Using separate strings for this gives cleaner code than trying to
           separate out the "very", in my opinion. */
        buf = (obj->owt > ocl->oc_weight) ?
            "very heavy iron ball" : "heavy iron ball";

        buf = display_oprops(obj, buf);
        break;

    case POTION_CLASS:
        if (dknown && obj->odiluted)
            buf = "diluted ";
        if (nn || un || !dknown) {
            buf = msgcat(buf, "potion");
            if (!dknown)
                break;
            if (nn) {
                buf = msgcat(buf, " of ");
                if (typ == POT_WATER && bknown &&
                    (obj->blessed || obj->cursed)) {
                    buf = msgcat(buf, obj->blessed ? "holy " : "unholy ");
                }
                buf = msgcat(buf, actualn);
            } else {
                buf = msgcat(buf, " called ");
                buf = msgcat(buf, un);
            }
        } else {
            buf = msgcat(buf, dn);
            buf = msgcat(buf, " potion");
        }
        break;

    case SCROLL_CLASS:
        buf = "scroll";
        if (!dknown)
            break;
        if (nn) {
            buf = msgcat(buf, " of ");
            buf = msgcat(buf, actualn);
        } else if (un) {
            buf = msgcat(buf, " called ");
            buf = msgcat(buf, un);
        } else if (ocl->oc_magic) {
            buf = msgcat(buf, " labeled ");
            buf = msgcat(buf, dn);
        } else {
            buf = msgcat(dn, " scroll");
        }
        break;

    case WAND_CLASS:
        if (!dknown)
            buf = "wand";
        else if (nn)
            buf = msgcat("wand of ", actualn);
        else if (un)
            buf = msgcat("wand called ", un);
        else
            buf = msgcat(dn, " wand");
        break;

    case SPBOOK_CLASS:
        if (!dknown) {
            buf = "spellbook";
        } else if (nn) {
            if (typ != SPE_BOOK_OF_THE_DEAD)
                buf = "spellbook of ";
            buf = msgcat(buf, actualn);
        } else if (un) {
            buf = msgcat("spellbook called ", un);
        } else
            buf = msgcat(dn, " spellbook");
        break;

    case RING_CLASS:
        if (!dknown)
            buf = "ring";
        else if (nn)
            buf = msgcat("ring of ", actualn);
        else if (un) {
            buf = "ring";
            do_un = TRUE;
        } else
            buf = msgcat(dn, " ring");

        buf = display_oprops(obj, buf);
        if (do_un)
            buf = msgcat_many(buf, " called ", un, NULL);
        break;

    case GEM_CLASS:
        {
            const char *rock = (ocl->oc_material == MINERAL) ? "stone" : "gem";

            if (!dknown) {
                buf = rock;
            } else if (!nn) {
                if (un) {
                    buf = rock;
                    do_un = TRUE;
                } else
                    buf = msgcat_many(dn, " ", rock, NULL);
            } else {
                buf = actualn;
                if (GemStone(typ))
                    buf = msgcat(buf, " stone");
            }
        }

        buf = display_oprops(obj, buf);
        if (do_un)
            buf = msgcat_many(buf, " called ", un, NULL);
        break;

    default:
        buf = msgprintf("glorkum %d %d %d", obj->oclass, typ, obj->spe);
    }
    if (!ignore_oquan && obj->quan != 1L)
        buf = makeplural(buf);

    if (ox_name(obj) && dknown) {
        buf = msgcat(buf, " named ");
    nameit:
        if (mark_user)
            buf = msgcat_many(buf, "\"", ox_name(obj), "\"", NULL);
        else
            buf = msgcat(buf, ox_name(obj));
    }

    if (!strncmpi(buf, "the ", 4))
        buf += 4;
    return buf;
}

/* xname() output augmented for multishot missile feedback */
const char *
mshot_xname(const struct obj *obj)
{
    const char *onm = xname(obj);

    if (m_shot.n > 1 && m_shot.o == obj->otyp) {
        /* "the Nth arrow"; value will eventually be passed to an() or The(),
           both of which correctly handle this "the " prefix */
        return msgprintf("the %d%s %s", m_shot.i, ordin(m_shot.i), onm);
    }

    return onm;
}


/* used for naming "the unique_item" instead of "a unique_item" */
boolean
the_unique_obj(const struct obj * obj)
{
    if (!obj->dknown)
        return FALSE;
    else if (obj->otyp == FAKE_AMULET_OF_YENDOR && !obj->known)
        return TRUE;    /* lie */
    else
        return (boolean) (objects[obj->otyp].oc_unique &&
                          (obj->known || obj->otyp == AMULET_OF_YENDOR));
}

static const char *
add_erosion_words(const struct obj *obj, const char *prefix)
{
    boolean iscrys = (obj->otyp == CRYSKNIFE);

    if (!is_damageable(obj) && !iscrys)
        return prefix;

    /* The only cases where any of these bits do double duty are for rotted
       food and diluted potions, which are all not is_damageable(). */
    if (obj->oeroded && !iscrys) {
        switch (obj->oeroded) {
        case 2:
            prefix = msgcat(prefix, "very ");
            break;
        case 3:
            prefix = msgcat(prefix, "thoroughly ");
            break;
        }
        prefix = msgcat(prefix, is_rustprone(obj) ? "rusty " : "burnt ");
    }
    if (obj->oeroded2 && !iscrys) {
        switch (obj->oeroded2) {
        case 2:
            prefix = msgcat(prefix, "very ");
            break;
        case 3:
            prefix = msgcat(prefix, "thoroughly ");
            break;
        }
        prefix = msgcat(prefix, is_corrodeable(obj) ? "corroded " : "rotted ");
    }
    if (obj->rknown && obj->oerodeproof)
        prefix = msgcat(prefix, iscrys ? "fixed " :
                        is_rustprone(obj) ? "rustproof " :
                        /* Should we use "stainless" instead? */
                        is_corrodeable(obj) ? "corrodeproof " :
                        is_flammable(obj) ? "fireproof " : "");
    return prefix;
}


static const char *
doname_base(const struct obj *obj, boolean with_price)
{
    boolean ispoisoned = FALSE;
    const char *buf;
    const char *prefix;

    if (with_price && (obj->unpaid || shop_item_cost(obj) > 0)) {
        /* If this item has a unique /base/ price for items within its class,
           and isn't a gem (shks lie about those) or a weapon or armor (and
           thus possibly enchanted), we automatically ID it. This takes weight
           into account too (as it's shown in inventory listings), which
           conveniently happens to distinguish between different description
           groups when we need it to.
           Tourists always ID shop objects. */
        if ((Role_if(PM_TOURIST) ||
             (obj->oclass != ARMOR_CLASS && obj->oclass != WEAPON_CLASS)) &&
            obj->oclass != GEM_CLASS) {
            int i;
            boolean id = TRUE;

            if (!Role_if(PM_TOURIST)) {
                for (i = 0; i < NUM_OBJECTS; i++) {
                    if (i == obj->otyp)
                        continue;
                    if (objects[i].oc_cost == objects[obj->otyp].oc_cost &&
                        objects[i].oc_class == objects[obj->otyp].oc_class &&
                        objects[i].oc_weight == objects[obj->otyp].oc_weight)
                        id = FALSE;
                }
            }

            /* tallow candles are unique by this check, wax ones aren't;
               special-case wax candles to make it consistent */
            if (id || obj->otyp == WAX_CANDLE)
                makeknown(obj->otyp);
        }
    }

    buf = xname(obj);


    /* When using xname, we want "poisoned arrow", and when using doname, we
       want "poisoned +0 arrow".  This kludge is about the only way to do it,
       at least until someone overhauls xname() and doname(), combining both
       into one function taking a parameter. */
    /* must check opoisoned--someone can have a weirdly-named fruit */
    if (!strncmp(buf, "poisoned ", 9) && obj->opoisoned) {
        buf += 9;
        ispoisoned = TRUE;
    }

    if (obj->quan != 1L)
        prefix = msgprintf("%d ", obj->quan);
    else if (obj_is_pname(obj) || the_unique_obj(obj)) {
        if (!strncmpi(buf, "the ", 4))
            buf += 4;
        prefix = "the ";
    } else if (!is_plural(obj)) /* Don't give boots/gloves/etc an article */
        prefix = "a ";
    else
        prefix = "";

#ifdef INVISIBLE_OBJECTS
    if (obj->oinvis)
        prefix = msgcat(prefix, "invisible ");
#endif

    if (obj->bknown && obj->oclass != COIN_CLASS &&
        (obj->otyp != POT_WATER || !objects[POT_WATER].oc_name_known ||
         (!obj->cursed && !obj->blessed))) {
        /* allow 'blessed clear potion' if we don't know it's holy water;
           always allow "uncursed potion of water" */
        if (obj->cursed)
            prefix = msgcat(prefix, "cursed ");
        else if (obj->blessed)
            prefix = msgcat(prefix, "blessed ");
        else if (flags.show_uncursed ||
                 ((!obj->known || !objects[obj->otyp].oc_charged ||
                   (obj->oclass == ARMOR_CLASS || obj->oclass == RING_CLASS))
                  /* For most items with charges or +/-, if you know how many
                     charges are left or what the +/- is, then you must have
                     totally identified the item, so "uncursed" is unneccesary,
                     because an identified object not described as "blessed" or
                     "cursed" must be uncursed. If the charges or +/- is not
                     known, "uncursed" must be printed to avoid ambiguity
                     between an item whose curse status is unknown, and an item
                     known to be uncursed. */
                  && obj->otyp != FAKE_AMULET_OF_YENDOR &&
                  obj->otyp != AMULET_OF_YENDOR && !Role_if(PM_PRIEST)))
            prefix = msgcat(prefix, "uncursed ");
    }

    if (obj->greased)
        prefix = msgcat(prefix, "greased ");

    switch (obj->oclass) {
    case AMULET_CLASS:
        if (obj->owornmask & W_WORN)
            buf = msgcat(buf, " (being worn)");
        break;
    case WEAPON_CLASS:
        if (ispoisoned)
            prefix = msgcat(prefix, "poisoned ");
    plus:
        prefix = add_erosion_words(obj, prefix);
        if (obj->known)
            prefix = msgprintf("%s%+d ", prefix, obj->spe);
        break;
    case ARMOR_CLASS:
        if (obj->owornmask & W_WORN)
            buf = msgcat(buf, (obj == uskin()) ?
                   " (embedded in your skin)" :
                   " (being worn)");
        goto plus;
    case TOOL_CLASS:
        /* weptools already get this done when we go to the +n code */
        if (!is_weptool(obj))
            prefix = add_erosion_words(obj, prefix);
        if (obj->owornmask & (W_WORN | W_MASK(os_saddle))) {
            buf = msgcat(buf, " (being worn)");
            break;
        }
        if (obj->otyp == LEASH && obj->leashmon != 0) {
            buf = msgcat(buf, " (in use)");
            break;
        }
        if (is_weptool(obj))
            goto plus;
        if (obj->otyp == CANDELABRUM_OF_INVOCATION) {
            const char *tmpbuf;

            if (!obj->spe)
                tmpbuf = "no";
            else
                tmpbuf = msgprintf("%d", obj->spe);
            buf = msgprintf("%s (%s candle%s%s)", buf, tmpbuf,
                            plur(obj->spe), !obj->lamplit ?
                            " attached" : ", lit");
        } else if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
                   obj->otyp == BRASS_LANTERN || Is_candle(obj)) {
            if (Is_candle(obj) &&
                obj->age < 20L * (long)objects[obj->otyp].oc_cost)
                prefix = msgcat(prefix, "partly used ");
            if (obj->lamplit)
                buf = msgcat(buf, " (lit)");
        }
        if (ignitable(obj) && obj->known && obj->otyp != MAGIC_LAMP &&
            !artifact_light(obj)) {
            long timeout =
                obj->lamplit ? report_timer(obj->olev, BURN_OBJECT,
                                            (const void *)obj) : moves;

            /* obj->age is the fuel remaining when the timer runs out. So we
               add it to the turns the timer has remaining to get remaining
               charge count. */
            buf = msgprintf("%s (%d:%ld)", buf, (int)obj->recharged,
                            obj->age + (timeout - moves));
            break;
        }

        if (objects[obj->otyp].oc_charged)
            goto charges;
        break;
    case WAND_CLASS:
        prefix = add_erosion_words(obj, prefix);
    charges:
        if (obj->known)
            buf = msgprintf("%s (%d:%d)", buf, (int)obj->recharged, obj->spe);
        break;
    case POTION_CLASS:
        if (obj->otyp == POT_OIL && obj->lamplit)
            buf = msgcat(buf, " (lit)");
        break;
    case RING_CLASS:
        prefix = add_erosion_words(obj, prefix);
    ring:
        if (obj->owornmask & W_MASK(os_ringr))
            buf = msgcat(buf, " (on right ");
        if (obj->owornmask & W_MASK(os_ringl))
            buf = msgcat(buf, " (on left ");
        if (obj->owornmask & W_RING) {
            buf = msgcat(buf, body_part(HAND));
            buf = msgcat(buf, ")");
        }
        if (obj->known && objects[obj->otyp].oc_charged)
            prefix = msgprintf("%s%+d ", prefix, obj->spe);
        break;
    case FOOD_CLASS:
        if (obj->oeaten)
            prefix = msgcat(prefix, "partly eaten ");
        if (obj->otyp == CORPSE) {
            if (mons[obj->corpsenm].geno & G_UNIQ) {
                prefix = msgcat_many(
                    (type_is_pname(&mons[obj->corpsenm]) ? "" : "the "),
                    s_suffix(mons[obj->corpsenm].mname), " ", NULL);
                if (obj->oeaten)
                    prefix = msgcat(prefix, "partly eaten ");
            } else {
                prefix = msgcat(prefix, mons[obj->corpsenm].mname);
                prefix = msgcat(prefix, " ");
            }
        } else if (obj->otyp == EGG) {
            if (obj->corpsenm >= LOW_PM &&
                (obj->known || mvitals[obj->corpsenm].mvflags & MV_KNOWS_EGG)) {
                prefix = msgcat(prefix, mons[obj->corpsenm].mname);
                prefix = msgcat(prefix, " ");
                if (obj->spe)
                    buf = msgcat(buf, " (laid by you)");
            }
        }
        if (obj->otyp == MEAT_RING)
            goto ring;
        break;
    case BALL_CLASS:
    case CHAIN_CLASS:
        prefix = add_erosion_words(obj, prefix);
        if (obj == uball)
            buf = msgcat(buf, " (chained to you)");
        break;
    }

    if (obj->owornmask & W_MASK(os_wep)) {
        if (obj->quan != 1L) {
            buf = msgcat(buf, " (wielded)");
        } else {
            const char *hand_s = body_part(HAND);

            if (bimanual(obj))
                hand_s = makeplural(hand_s);
            buf = msgcat_many(buf, " (weapon in ", hand_s, ")", NULL);
        }
    }
    if (obj->owornmask & W_MASK(os_swapwep)) {
        if (u.twoweap)
            buf = msgcat_many(buf, " (wielded in other ", body_part(HAND),
                              ")", NULL);
        else
            buf = msgcat(buf, " (alternate weapon; not wielded)");
    }
    if (obj->owornmask & W_MASK(os_quiver))
        buf = msgcat(buf, " (in quiver)");
    if (obj->unpaid) {
        xchar ox, oy;
        long quotedprice = unpaid_cost(obj);
        struct monst *shkp = NULL;

        if (Has_contents(obj) &&
            get_obj_location(obj, &ox, &oy, BURIED_TOO | CONTAINED_TOO) &&
            costly_spot(ox, oy) &&
            (shkp = shop_keeper(level, *in_rooms(level, ox, oy, SHOPBASE))))
            quotedprice += contained_cost(obj, shkp, 0L, FALSE, TRUE);
        buf = msgprintf("%s (unpaid, %ld %s)", buf, quotedprice,
                        currency(quotedprice));
    } else if (with_price) {
        int price = shop_item_cost(obj);

        if (price > 0)
            buf = msgprintf("%s (%d %s)", buf, price, currency(price));
    }
    if (!strncmp(prefix, "a ", 2) &&
        strchr(vowels, *(prefix + 2) ? *(prefix + 2) : *buf)
        && (*(prefix + 2) || (strncmp(buf, "uranium", 7)
                              && strncmp(buf, "unicorn", 7)
                              && strncmp(buf, "eucalyptus", 10)))) {
        prefix = msgcat("an ", prefix + 2);
    }
    return msgcat(prefix, buf);
}


const char *
doname(const struct obj *obj)
{
    return doname_base(obj, FALSE);
}


const char *
doname_price(const struct obj *obj)
{
    return doname_base(obj, TRUE);
}


/* used from invent.c */
boolean
not_fully_identified_core(const struct obj * otmp, boolean ignore_bknown,
                          int skill)
{
    /* gold doesn't have any interesting attributes [yet?] */
    if (otmp->oclass == COIN_CLASS)
        return FALSE;   /* always fully ID'd */

    /* check fundamental ID hallmarks first */
    if (!objects[otmp->otyp].oc_name_known || !otmp->dknown)
        return TRUE;
    if (otmp->oartifact && undiscovered_artifact(otmp->oartifact))
        return TRUE;
    if (!otmp->known && skill >= P_BASIC)
        return TRUE;
    if (!otmp->bknown && !ignore_bknown && skill >= P_SKILLED)
        return TRUE;

    /* object property ID */
    if (obj_properties(otmp) &&
        otmp->oprops_known != otmp->oprops && skill >= P_BASIC)
        return TRUE;

    /* otmj->rknown is the only item of interest if we reach here */
    /* 
     *  Note:  if a revision ever allows scrolls to become fireproof or
     *  rings to become shockproof, this checking will need to be revised.
     *  `rknown' ID only matters if xname() will provide the info about it.
     */
    if (skill == P_EXPERT &&
        (otmp->rknown || (otmp->oclass != ARMOR_CLASS &&
                          otmp->oclass != WEAPON_CLASS &&
                          !is_weptool(otmp) &&    /* (redunant) */
                          otmp->oclass != BALL_CLASS)))   /* (useless) */
        return FALSE;
    else        /* lack of `rknown' only matters for vulnerable objects */
        return (boolean) ((is_rustprone(otmp) || is_corrodeable(otmp) ||
                           is_flammable(otmp)) && skill == P_EXPERT);
}


boolean
not_fully_identified(const struct obj * otmp)
{
    return not_fully_identified_core(otmp, FALSE,
                                     P_EXPERT);
}

const char *
corpse_xname(const struct obj *otmp, boolean ignore_oquan)
{       /* to force singular */
    const char *nambuf = msgcat(mons[otmp->corpsenm].mname, " corpse");

    if (ignore_oquan || otmp->quan < 2)
        return nambuf;
    else
        return makeplural(nambuf);
}


/* xname, unless it's a corpse, then corpse_xname(obj, FALSE) */
const char *
cxname(const struct obj *obj)
{
    if (obj->otyp == CORPSE)
        return corpse_xname(obj, FALSE);
    return xname(obj);
}


/* xname, unless it's a corpse, then corpse_xname(obj, TRUE); but ignore the
   quantity in either case */
const char *
cxname2(const struct obj *obj)
{
    if (obj->otyp == CORPSE)
        return corpse_xname(obj, TRUE);
    return xname2(obj, TRUE, FALSE);
}


/* treat an object as fully ID'd when it might be used as reason for death */
const char *
killer_xname(const struct obj *obj_orig)
{
    struct obj *obj;
    unsigned save_ocknown;
    const char *buf;
    char *save_ocuname;
    int osize;

    /* Copy the object. */
    osize = sizeof (struct obj);
    unsigned char objcopy[osize];
    obj = (void *)objcopy;
    memcpy(obj, obj_orig, osize);

    /* killer name should be more specific than general xname; however, exact
       info like blessed/cursed and rustproof makes things be too verbose */
    obj->known = obj->dknown = 1;
    obj->bknown = obj->rknown = obj->greased = 0;
    /* if character is a priest[ess], bknown will get toggled back on */
    obj->blessed = obj->cursed = 0;
    /* "killed by poisoned <obj>" would be misleading when poison is not the
       cause of death and "poisoned by poisoned <obj>" would be redundant when
       it is, so suppress "poisoned" prefix */
    obj->opoisoned = 0;
    /* strip user-supplied name; artifacts keep theirs */
    if (!obj->oartifact && ox_name(obj)) {
        /* Not christen_obj, that will mess with the original obj's pointer (since
           they're the same). We currently don't use obj->oextra for anything unless we
           want the artifact name, so just dispose of it entirely. */
        obj->oextra = NULL;
    }

    /* temporarily identify the type of object */
    save_ocknown = objects[obj->otyp].oc_name_known;
    objects[obj->otyp].oc_name_known = 1;
    save_ocuname = objects[obj->otyp].oc_uname;
    objects[obj->otyp].oc_uname = 0;    /* avoid "foo called bar" */

    if (obj->otyp == CORPSE)
        buf = corpse_xname(obj, FALSE);
    else
        buf = xname(obj);

    if (obj->quan == 1L)
        buf = obj_is_pname(obj) ? the(buf) : an(buf);

    objects[obj->otyp].oc_name_known = save_ocknown;
    objects[obj->otyp].oc_uname = save_ocuname;

    return buf;
}

/*
 * Used if only one of a collection of objects is named (e.g. in eat.c).
 */
const char *
singular(struct obj *otmp, const char *(*func) (const struct obj *))
{
    long savequan;
    const char *nam;

    /* Note: using xname for corpses will not give the monster type */
    if (otmp->otyp == CORPSE && func == xname)
        return corpse_xname(otmp, TRUE);

    savequan = otmp->quan;
    otmp->quan = 1L;
    nam = (*func) (otmp);
    otmp->quan = savequan;
    return nam;
}

const char *
an(const char *str)
{
    const char *buf = "";

    if (strncmpi(str, "the ", 4) && strcmp(str, "molten lava") &&
        strcmp(str, "iron bars") && strcmp(str, "ice") &&
        strcmp(str, "solid rock") && strcmp(str, "eyewear") &&
        strcmp(str, "boots") && strcmp(str, "gloves")) {
        if (strchr(vowels, *str) && strncmp(str, "one-", 4) &&
            strncmp(str, "useful", 6) && strncmp(str, "unicorn", 7) &&
            strncmp(str, "uranium", 7) && strncmp(str, "eucalyptus", 10))
            buf = "an ";
        else
            buf = "a ";
    }

    return msgcat(buf, str);
}

const char *
An(const char *str)
{
    return msgupcasefirst(an(str));
}

/*
 * Prepend "the" if necessary; assumes str is a subject derived from xname.
 * Use type_is_pname() for monster names, not the().  the() is idempotent.
 */
const char *
the(const char *str)
{
    boolean insert_the = FALSE;

    if (!strncmpi(str, "the ", 4)) {
        return msgcat("the ", str+4);
    } else if (*str < 'A' || *str > 'Z') {
        /* not a proper name, needs an article */
        insert_the = TRUE;
    } else {
        /* Probably a proper name, might not need an article */
        const char *tmp, *named, *called;
        int l;

        /* some objects have capitalized adjectives in their names */
        if ((((tmp = strrchr(str, ' '))) || ((tmp = strrchr(str, '-')))) &&
            (tmp[1] < 'A' || tmp[1] > 'Z'))
            insert_the = TRUE;
        else if (tmp && strchr(str, ' ') < tmp) {       /* has spaces */
            /* it needs an article if the name contains "of" */
            tmp = strstri(str, " of ");
            named = strstri(str, " named ");
            called = strstri(str, " called ");
            if (called && (!named || called < named))
                named = called;

            if (tmp && (!named || tmp < named)) /* found an "of" */
                insert_the = TRUE;
            /* stupid special case: lacks "of" but needs "the" */
            else if (!named && (l = strlen(str)) >= 31 &&
                     !strcmp(&str[l - 31], "Platinum Yendorian Express Card"))
                insert_the = TRUE;
        }
    }
    if (insert_the)
        return msgcat("the ", str);
    else
        return str;
}

const char *
The(const char *str)
{
    return msgupcasefirst(the(str));
}

/* returns "count cxname(otmp)" or just cxname(otmp) if count == 1 */
const char *
aobjnam(const struct obj *otmp, const char *verb)
{
    const char *buf = cxname(otmp);

    if (otmp->quan != 1L)
        buf = msgprintf("%d %s", otmp->quan, buf);

    if (verb)
        return msgcat_many(buf, " ", otense(otmp, verb), NULL);
    else
        return buf;
}

/* like aobjnam, but prepend "The", not count, and use xname */
const char *
Tobjnam(const struct obj *otmp, const char *verb)
{
    const char *buf = The(xname(otmp));

    if (verb)
        return msgcat_many(buf, " ", otense(otmp, verb), NULL);
    else
        return buf;
}

/* return form of the verb (input plural) if xname(otmp) were the subject */
const char *
otense(const struct obj *otmp, const char *verb)
{
    /* The verb is given in the plural (without trailing s).  Return as input if
       the result of xname(otmp) would be plural.  Don't bother recomputing
       xname(otmp) at this time. */

    if (!is_plural(otmp))
        return vtense(NULL, verb);
    else
        return verb;
}

/* various singular words that vtense would otherwise categorize as plural */
static const char *const special_subjs[] = {
    "erinys",
    "manes",    /* this one is ambiguous */
    "Cyclops",
    "Hippocrates",
    "Pelias",
    "aklys",
    "amnesia",
    "paralysis",
    0
};

/* return form of the verb (input plural) for present tense 3rd person subj */
const char *
vtense(const char *subj, const char *verb)
{
    int len, ltmp;
    const char *sp, *spot;
    const char *const *spec;

    /* 
     * verb is given in plural (without trailing s).  Return as input
     * if subj appears to be plural.  Add special cases as necessary.
     * Many hard cases can already be handled by using otense() instead.
     * If this gets much bigger, consider decomposing makeplural.
     * Note: monster names are not expected here (except before corpse).
     *
     * special case: allow null sobj to get the singular 3rd person
     * present tense form so we don't duplicate this code elsewhere.
     */
    if (subj) {
        if (!strncmpi(subj, "a ", 2) || !strncmpi(subj, "an ", 3))
            goto sing;
        spot = NULL;
        for (sp = subj; (sp = strchr(sp, ' ')) != 0; ++sp) {
            if (!strncmp(sp, " of ", 4) || !strncmp(sp, " from ", 6) ||
                !strncmp(sp, " called ", 8) || !strncmp(sp, " named ", 7) ||
                !strncmp(sp, " labeled ", 9)) {
                if (sp != subj)
                    spot = sp - 1;
                break;
            }
        }
        len = (int)strlen(subj);
        if (!spot)
            spot = subj + len - 1;

        /* 
         * plural: anything that ends in 's', but not '*us' or '*ss'.
         * Guess at a few other special cases that makeplural creates.
         */
        if ((*spot == 's' && spot != subj &&
             (*(spot - 1) != 'u' && *(spot - 1) != 's')) ||
            ((spot - subj) >= 4 && !strncmp(spot - 3, "eeth", 4)) ||
            ((spot - subj) >= 3 && !strncmp(spot - 3, "feet", 4)) ||
            ((spot - subj) >= 2 && !strncmp(spot - 1, "ia", 2)) ||
            ((spot - subj) >= 2 && !strncmp(spot - 1, "ae", 2))) {
            /* check for special cases to avoid false matches */
            len = (int)(spot - subj) + 1;
            for (spec = special_subjs; *spec; spec++) {
                ltmp = strlen(*spec);
                if (len == ltmp && !strncmpi(*spec, subj, len))
                    goto sing;
                /* also check for <prefix><space><special_subj> to catch things 
                   like "the invisible erinys" */
                if (len > ltmp && *(spot - ltmp) == ' ' &&
                    !strncmpi(*spec, spot - ltmp + 1, ltmp))
                    goto sing;
            }

            return verb;
        }
        /* 
         * 3rd person plural doesn't end in telltale 's';
         * 2nd person singular behaves as if plural.
         */
        if (!strcmpi(subj, "they") || !strcmpi(subj, "you"))
            return verb;
    }

sing:
    len = strlen(verb);
    spot = verb + len - 1;

    if (!strcmp(verb, "are"))
        return "is";
    else if (!strcmp(verb, "have"))
        return "has";
    else if (strchr("zxs", *spot) ||
             (len >= 2 && *spot == 'h' && strchr("cs", *(spot - 1))) ||
             (len == 2 && *spot == 'o'))
        /* Ends in z, x, s, ch, sh; add an "es" */
        return msgcat(verb, "es");
    else if (*spot == 'y' && (!strchr(vowels, *(spot - 1))))
        /* like "y" case in makeplural */
        return msgcat(msgchop(verb, -1), "ies");
    else
        return msgcat(verb, "s");
}

/* capitalized variant of doname() */
const char *
Doname2(const struct obj *obj)
{
    return msgupcasefirst(doname(obj));
}

/* returns "your xname(obj)" or "Foobar's xname(obj)" or "the xname(obj)" */
const char *
yname(const struct obj *obj)
{
    return msgcat_many(shk_your(obj), " ", cxname(obj), NULL);
}

/* capitalized variant of yname() */
const char *
Yname2(const struct obj *obj)
{
    return msgupcasefirst(yname(obj));
}

/*
 * Returns "your simple_typename(obj->otyp)"
 *  or "Foobar's simple_typename(obj->otyp)"
 *  or      "the simple_typename(obj->otyp)".
 */
const char *
ysimple_name(const struct obj *obj)
{
    return msgcat_many(shk_your(obj), " ", simple_typename(obj->otyp), NULL);
}

static const char *const wrp[] = {
    "wand", "ring", "potion", "scroll", "gem", "amulet",
    "spellbook", "spell book",
    /* for non-specific wishes */
    "weapon", "armor", "armour", "tool", "food", "comestible",
};

static const char wrpsym[] = {
    WAND_CLASS, RING_CLASS, POTION_CLASS, SCROLL_CLASS, GEM_CLASS,
    AMULET_CLASS, SPBOOK_CLASS, SPBOOK_CLASS,
    WEAPON_CLASS, ARMOR_CLASS, ARMOR_CLASS, TOOL_CLASS, FOOD_CLASS,
    FOOD_CLASS
};


/* Plural routine; chiefly used for user-defined fruits. We have to try to
   account for everything reasonable the player has; something unreasonable can
   still break the code. However, it's still a lot more accurate than "just add
   an s at the end", which Rogue uses...
  
   Also used for plural monster names ("Wiped out all homunculi.")
   and body parts.
  
   Also misused by muse.c to convert 1st person present verbs to 2nd person.
  
   The code here uses pretty much the same algorithm as 3.4.3, but has been
   rewritten to avoid code duplication and string mutation. The main body of the
   code now works on a "plurals dictionary" that gives suffixes and what they
   pluralize to. A ^ at the start means that we have to match an entire word,
   not just a suffix of it. (Something that could be implemented if this ever
   somehow became a performance bottleneck is indexing by the last letter of
   the suffix.)

   Matches listed earlier in the dictionary take precedence over matches listed
   later; this allows exceptions to be handled the same way as the rules they
   are exceptions to.

   There are also a few special cases not present in the dictionary, handed in
   the body of makeplural itself.

   TODO: This function currently does not handle case correctly (and didn't in
   3.4.3), and should be given only lowercase input as a result.
 */
static const char *plurals_dictionary[][2] = {

    /* Same singular and plural; most of these are Japanese loanwords */
    {"^ya", "ya"},
    {"ai", "ai"},             /* e.g. "samurai" */
    {"fish", "fish"},
    {"tuna", "tuna"},
    {"deer", "deer"},
    {"yaki", "yaki"},
    {"sheep", "sheep"},
    {"ninja", "ninja"},
    {"ronin", "ronin"},
    {"shito", "shito"},
    {"tengu", "tengu"},
    {"manes", "manes"},
    {"ki-rin", "ki-rin"},
    {"Nazgul", "Nazgul"},
    {"gunyoki", "gunyoki"},
    {"shuriken", "shuriken"},

    /* Irregular endings */
    {"shaman", "shamans"},    /* -man exception */
    {"human", "humans"},      /* -man exception */
    {"man", "men"},
    {"tooth", "teeth"},
    {"foot", "feet"},
    {"erinys", "erinyes"},
    {"mouse", "mice"},
    {"louse", "lice"},
    {"child", "children"},
    {"^ox", "oxen"},
    {"vax", "vaxen"},         /* according to the NetHack < 4 devteam */
    {"goose", "geese"},

    /* Latin and Greek */
    {"ium", "ia"},            /* e.g. "mycelium" */
    {"alga", "algae"},
    {"hypha", "hyphae"},
    {"larva", "larvae"},
    {"lotus", "lotuses"},     /* -us exception */
    {"virus", "viruses"},     /* -us exception */
    {"wumpus", "wumpuses"},   /* -us exception */
    {"^bus", "buses"},        /* -us exception */
    {"us", "i"},              /* e.g. "fungus" */
    {"rtex", "rtices"},       /* probably only matches "vortex" */
    {"djinni", "djinn"},
    {"efreet", "efreeti"},
    {"mumak", "mumakil"},
    {"sis", "ses"},           /* e.g. "nemesis" */

    /* Other loanwords */
    {"matzoh", "matzot"},
    {"matzah", "matzot"},
    {"matzo", "matzot"},
    {"matza", "matzot"},
    {"eau", "eaux"},          /* e.g. "gateau" */

    /* -f endings */
    {"fe", "ves"},            /* e.g. "knife" */
    {"lf", "lves"},           /* e.g. "wolf", "elf" */
    {"rf", "rves"},           /* e.g. "scarf" */
    /* Quite a few false positives here :-( */
    {"af", "aves"},           /* e.g. "leaf", "loaf" */
    {"ef", "eves"},           /* e.g. "thieves" but "reefs", "briefs" */
    /* "if" case that is present in 3.4.3 was removed; it's very rare for -if to
       end nouns, and for every such noun in my dictionary (e.g. "motif",
       "coif", "waif"), the plural is "-ifs" */
    {"proof", "proofs"},      /* -of exception */
    {"of", "oves"},           /* e.g. "roof", "hoof" but "woofs" */
    {"uf", "uves"},           /* actually no words end -uf, so we don't know
                                 how they pluralize */
    {"staff", "staves"},

    /* -es plurals */
    {"z", "zes"},             /* e.g. "quartz" */
    {"x", "xes"},             /* e.g. "box" */
    {"s", "ses"},             /* e.g. "priestess" */
    {"ch", "ches"},           /* e.g. "witch" */
    {"sh", "shes"},           /* e.g. "marsh" */
    {"ato", "atoes"},         /* e.g. "tomato", "potato" */

    /* -ies plurals */
    {"ay", "ays"},            /* -ies exception */
    {"ey", "eys"},            /* -ies exception */
    {"iy", "iys"},            /* -ies exception; actually no words match */
    {"oy", "oys"},            /* -ies exception */
    {"uy", "uys"},            /* -ies exception */
    {"y", "ies"},             /* e.g. "discovery" */

    /* -s plurals: everything not handled above */
    {"", "s"},
    /* We don't need a fencepost because the above match is guaranteed to
       succeed. */
};
const char *
makeplural(const char *str)
{
    /* Note: cannot use strcmpi here -- it'd give MATZot, CAVEMeN,... */
    const char *spot;
    const char *suffix = NULL;
    int len = strlen(str);
    const char *((*dictentry)[2]);

    while (*str == ' ')
        str++;
    if (!*str) {
        impossible("plural of null?");
        return "";
    }

    /* 
     * Skip changing "pair of" to "pairs of".  According to Webster, usual
     * English usage is use pairs for humans, e.g. 3 pairs of dancers,
     * and pair for objects and non-humans, e.g. 3 pair of boots.  We don't
     * refer to pairs of humans in this game so just skip to the bottom.
     */
    if (!strncmp(str, "pair of ", 8))
        goto bottom;

    /* Search for common compounds, ex. lump of royal jelly. If we find one,
       we pluralize just the bit at the start, and attach the rest of the
       string to the end later. */
    for (spot = str; *spot; spot++) {
        if (!strncmp(spot, " of ", 4)
            || !strncmp(spot, " labeled ", 9)
            || !strncmp(spot, " called ", 8)
            || !strncmp(spot, " named ", 7)
            ||  !strcmp(spot, " above")         /* lurkers above */
            || !strncmp(spot, " versus ", 8)
            || !strncmp(spot, " from ", 6)
            || !strncmp(spot, " in ", 4)
            || !strncmp(spot, "-in-", 4)        /* mothers-in-law */
            || !strncmp(spot, " on ", 4)
            || !strncmp(spot, " a la ", 6)
            || !strncmp(spot, " with", 5)       /* " with "? */
            || !strncmp(spot, " de ", 4)
            || !strncmp(spot, " d'", 3)
            || !strncmp(spot, " du ", 4)) {
            suffix = spot;
            len = spot - str;
            str = msgchop(str, len);
            spot = str + len;
            break;
        }
    }
    spot--;
    while (*spot == ' ') {
        spot--; /* Strip blanks from end */
    }
    spot++;
    len = spot - str;
    str = msgchop(str, len);
    spot = str + len;

    /* spot is now the trailing NUL */

    /* Single letters and nonletters pluralize with 's. */
    if (len <= 1 || !letter(spot[-1])) {
        str = msgcat(str, "'s");
        goto bottom;
    }

    /* Check the dictionary for a match. (The last case of the dictionary
       adds an 's' to any word not otherwise matched.) */
    for (dictentry = plurals_dictionary;; dictentry++)
    {
        if ((*dictentry)[0][0] == '^') {

            /* We need an exact match. */
            if (!strcmp((*dictentry)[0] + 1, str)) {
                str = (*dictentry)[1];
                break;
            }

        } else {

            /* We need a suffix match. */
            int dictlen = strlen((*dictentry)[0]);
            if (len >= dictlen &&
                !strcmp(spot - dictlen, (*dictentry)[0])) {
                if (dictlen == 0)
                    str = msgcat(str, (*dictentry)[1]);
                else
                    str = msgcat(msgchop(str, -dictlen), (*dictentry)[1]);
                break;
            }
        }
    }

bottom:
    if (suffix)
        return msgcat(str, suffix);
    else
        return str;
}


struct o_range {
    const char *name, oclass;
    int f_o_range, l_o_range;
};

/* wishable subranges of objects */
static const struct o_range o_ranges[] = {
    {"bag", TOOL_CLASS, SACK, BAG_OF_TRICKS},
    {"lamp", TOOL_CLASS, OIL_LAMP, MAGIC_LAMP},
    {"candle", TOOL_CLASS, TALLOW_CANDLE, WAX_CANDLE},
    {"horn", TOOL_CLASS, TOOLED_HORN, HORN_OF_PLENTY},
    {"shield", ARMOR_CLASS, SMALL_SHIELD, SHIELD_OF_REFLECTION},
    {"helm", ARMOR_CLASS, ELVEN_LEATHER_HELM, HELM_OF_TELEPATHY},
    {"gloves", ARMOR_CLASS, LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY},
    {"gauntlets", ARMOR_CLASS, LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY},
    /* If you're adding more boots synonyms, make sure to add to the checks that
       ensure that boots of speed/fumbling is parsed correctly. */
    {"boots", ARMOR_CLASS, LOW_BOOTS, LEVITATION_BOOTS},
    {"shoes", ARMOR_CLASS, LOW_BOOTS, IRON_SHOES},
    {"cloak", ARMOR_CLASS, MUMMY_WRAPPING, CLOAK_OF_DISPLACEMENT},
    {"shirt", ARMOR_CLASS, HAWAIIAN_SHIRT, T_SHIRT},
    {"dragon scales",
     ARMOR_CLASS, GRAY_DRAGON_SCALES, YELLOW_DRAGON_SCALES},
    {"dragon scale mail",
     ARMOR_CLASS, GRAY_DRAGON_SCALE_MAIL, YELLOW_DRAGON_SCALE_MAIL},
    {"sword", WEAPON_CLASS, SHORT_SWORD, KATANA},
    {"gray stone", GEM_CLASS, LUCKSTONE, FLINT},
    {"grey stone", GEM_CLASS, LUCKSTONE, FLINT},
};

/* TODO: I don't think these are technically legal according to the standard.
   In particular, a compiler is well within its rights to optimize away the
   "ptr < base" check. */
#define BSTRCMP(base,ptr,string) ((ptr) < base || strcmp((ptr),string))
#define BSTRCMPI(base,ptr,string) ((ptr) < base || strcmpi((ptr),string))
#define BSTRNCMP(base,ptr,string,num) ((ptr)<base || strncmp((ptr),string,num))
#define BSTRNCMPI(base,ptr,string,num) ((ptr)<base||strncmpi((ptr),string,num))

/* Singularize a string the user typed in; this helps reduce the complexity of
   readobjnam, and is also used in pager.c to singularize the string for which
   help is sought. */
const char *
makesingular(const char *str)
{
    const char *spot;

    if (!str || !*str) {
        impossible("singular of null?");
        return "";
    }

    while (*str == ' ')
        str++;

    /* find "cloves of garlic", "worthless pieces of blue glass" */
    if ((spot = strstri(str, "s of ")) != 0) {
        /* but don't singularize "gauntlets", "boots", "Eyes of the.." */
        if (BSTRNCMPI(str, spot - 3, "Eye", 3) &&
            BSTRNCMP(str, spot - 4, "boot", 4) &&
            BSTRNCMP(str, spot - 8, "gauntlet", 8))
            return msgcat(msgchop(str, spot - str), spot+1);
        else
            return str;
    }

    /* remove -s or -es (boxes) or -ies (rubies) */
    spot = str + strlen(str);
    if (spot >= str + 1 && spot[-1] == 's') {
        if (spot >= str + 2 && spot[-2] == 'e') {
            if (spot >= str + 3 && spot[-3] == 'i') {
                /* -ies */

                if (!BSTRCMP(str, spot - 7, "cookies") ||
                    !BSTRCMP(str, spot - 4, "pies"))
                    goto mins;

                return msgcat(msgchop(str, -3), "y");
            }

            /* note: cloves / knives from clove / knife */
            if (!BSTRCMP(str, spot - 6, "knives"))
                return msgcat(msgchop(str, -3), "fe");
            if (!BSTRCMP(str, spot - 6, "staves"))
                return msgcat(msgchop(str, -3), "ff");
            if (!BSTRCMPI(str, spot - 6, "leaves"))
                return msgcat(msgchop(str, -3), "f");
            if (!BSTRCMP(str, spot - 8, "vortices"))
                return msgcat(msgchop(str, -4), "ex");

            /* note: nurses, axes but boxes */
            if (!BSTRCMP(str, spot - 5, "boxes") ||
                !BSTRCMP(str, spot - 4, "ches"))
                return msgchop(str, -2);

            if (!BSTRCMP(str, spot - 6, "gloves") ||
                !BSTRCMP(str, spot - 6, "lenses") ||
                !BSTRCMP(str, spot - 5, "shoes") ||
                !BSTRCMP(str, spot - 6, "scales"))
                return str;

        } else if (!BSTRCMP(str, spot - 5, "boots") ||
                   !BSTRCMP(str, spot - 9, "gauntlets") ||
                   !BSTRCMP(str, spot - 6, "tricks") ||
                   !BSTRCMP(str, spot - 9, "paralysis") ||
                   !BSTRCMP(str, spot - 5, "glass") ||
                   !BSTRCMP(str, spot - 4, "ness") ||
                   !BSTRCMP(str, spot - 14, "shape changers") ||
                   !BSTRCMP(str, spot - 15, "detect monsters") ||
                   !BSTRCMPI(str, spot - 11, "Aesculapius") || /* staff */
                   !BSTRCMP(str, spot - 10, "eucalyptus") ||
                   !BSTRCMP(str, spot - 9, "iron bars") ||
                   !BSTRCMP(str, spot - 5, "aklys") ||
                   !BSTRCMP(str, spot - 6, "fungus"))
            return str;
    mins:
        str = msgchop(str, -1);

    } else {

        if (!BSTRCMP(str, spot - 5, "teeth")) {
            return msgcat(msgchop(str, -5), "tooth");
        }

        if (!BSTRCMP(str, spot - 5, "fungi")) {
            return msgcat(msgchop(str, -5), "fungus");
        }

        /* here we cannot find the plural suffix */
    }
    return str;
}

/* compare user string against object name string using fuzzy matching */
static boolean
wishymatch(const char *u_str,   /* from user, so might be variant spelling */
           const char *o_str,   /* from objects[], so is in canonical form */
           boolean retry_inverted)         /* optional extra "of" handling */
{
    /* Wizards can wish for traps.  For 3.4.3 compatibility, we don't fuzzily
       match "beartrap" to "bear trap".  TODO: The correct fix probably involves
       the new unidentified appearances of these traps ("toothed trap",
       "explosive trap"), which were added to as a workaround for this sort of
       clash. */
    if (wizard && !strcmp(o_str, "beartrap"))
        return !strncmpi(o_str, u_str, 8);

    /* Ignore spaces & hyphens and upper/lower case when comparing. */
    if (fuzzymatch(u_str, o_str, " -", TRUE))
        return TRUE;

    if (retry_inverted) {
        const char *u_of, *o_of;

        /* when just one of the strings is in the form "foo of bar", convert it 
           into "bar foo" and perform another comparison */
        u_of = strstri(u_str, " of ");
        o_of = strstri(o_str, " of ");
        if (u_of && !o_of)
            return fuzzymatch(
                msgcat_many(
                    u_of + 4, " ", msgchop(u_str, u_of - u_str), NULL),
                o_str, " -", TRUE);
        else if (o_of && !u_of)
            return fuzzymatch(
                u_str, msgcat_many(
                    o_of + 4, " ", msgchop(o_str, o_of - o_str), NULL),
                " -", TRUE);
    }

    /* [note: if something like "elven speed boots" ever gets added, these
       special cases should be changed to call wishymatch() recursively in
       order to get the "of" inversion handling] */
    if (!strncmp(o_str, "dwarvish ", 9)) {
        if (!strncmpi(u_str, "dwarven ", 8))
            return fuzzymatch(u_str + 8, o_str + 9, " -", TRUE);
    } else if (!strncmp(o_str, "elven ", 6)) {
        if (!strncmpi(u_str, "elvish ", 7))
            return fuzzymatch(u_str + 7, o_str + 6, " -", TRUE);
        else if (!strncmpi(u_str, "elfin ", 6))
            return fuzzymatch(u_str + 6, o_str + 6, " -", TRUE);
    } else if (!strcmp(o_str, "aluminum")) {
        /* this special case doesn't really fit anywhere else... */
        /* (note that " wand" will have been stripped off by now) */
        if (!strcmpi(u_str, "aluminium"))
            return fuzzymatch(u_str + 9, o_str + 8, " -", TRUE);
    }

    return FALSE;
}

/* alternate spellings; if the difference is only the presence or
   absence of spaces and/or hyphens (such as "pickaxe" vs "pick axe"
   vs "pick-axe") then there is no need for inclusion in this list;
   likewise for ``"of" inversions'' ("boots of speed" vs "speed boots") */
static const struct alt_spellings {
    const char *sp;
    int ob;
} spellings[] = {
    {"pickax", PICK_AXE},
    {"whip", BULLWHIP},
    {"saber", SILVER_SABER},
    {"silver sabre", SILVER_SABER},
    {"smooth shield", SHIELD_OF_REFLECTION},
    {"grey dragon scale mail", GRAY_DRAGON_SCALE_MAIL},
    {"grey dragon scales", GRAY_DRAGON_SCALES},
    {"enchant armour", SCR_ENCHANT_ARMOR},
    {"destroy armour", SCR_DESTROY_ARMOR},
    {"scroll of enchant armour", SCR_ENCHANT_ARMOR},
    {"scroll of destroy armour", SCR_DESTROY_ARMOR},
    {"leather armour", LEATHER_ARMOR},
    {"studded leather armour", STUDDED_LEATHER_ARMOR},
    {"iron ball", HEAVY_IRON_BALL},
    {"lantern", BRASS_LANTERN},
    {"mattock", DWARVISH_MATTOCK},
    {"amulet of poison resistance", AMULET_VERSUS_POISON},
    {"stone", ROCK},
    {"camera", EXPENSIVE_CAMERA},
    {"tee shirt", T_SHIRT},
    {"can", TIN},
    {"can opener", TIN_OPENER},
    {"kelp", KELP_FROND},
    {"eucalyptus", EUCALYPTUS_LEAF},
    {"grapple", GRAPPLING_HOOK},
    {NULL, 0}};

/*
 * Return something wished for.  Specifying a null pointer for
 * the user request string results in a random object.  Otherwise,
 * if asking explicitly for "nothing" (or "nil") return no_wish;
 * if not an object return &zeroobj; if an error (no matching object),
 * return null.
 * If from_user is false, we're reading from the wizkit, nothing was typed in.
 *
 * bp must be writable, as is usual for strings input from the user.
 *
 * TODO: This function assumes writable space beyond the end of bp, and
 * making it work on a message rather than buffer would be nice.
 */
struct obj *
readobjnam(char *bp, struct obj *no_wish, boolean from_user)
{
    char *p;
    int i;
    struct obj *otmp;
    int cnt, spe, spesgn, typ, very, rechrg;
    int blessed, uncursed, iscursed, ispoisoned, isgreased;
    int eroded, eroded2, erodeproof;

#ifdef INVISIBLE_OBJECTS
    int isinvisible;
#endif
    int halfeaten, mntmp, contents;
    int islit, unlabeled, ishistoric, isdiluted;
    const struct alt_spellings *as = spellings;
    struct fruit *f;
    int ftype = gamestate.fruits.current;
    const char *fruitbuf;

    /* Fruits may not mess up the ability to wish for real objects (since you
       can leave a fruit in a bones file and it will be added to another
       person's game), so they must be checked for last, after stripping all
       the possible prefixes and seeing if there's a real name in there.  So we 
       have to save the full original name.  However, it's still possible to do 
       things like "uncursed burnt Alaska", or worse yet, "2 burned 5 course
       meals", so we need to loop to strip off the prefixes again, this time
       stripping only the ones possible on food. We could get even more
       detailed so as to allow food names with prefixes that _are_ possible on
       food, so you could wish for "2 3 alarm chilis".  Currently this isn't
       allowed; options.c automatically sticks 'candied' in front of such
       names. */

    char oclass, invlet;
    char *un, *dn, *actualn;
    const char *name = 0;
    uint64_t props = 0;
    boolean magical = FALSE; /* generic term for object properties */

    cnt = spe = spesgn = typ = very = rechrg = blessed = uncursed = iscursed =
#ifdef INVISIBLE_OBJECTS
        isinvisible =
#endif
        ispoisoned = isgreased = eroded = eroded2 = erodeproof = halfeaten =
        islit = unlabeled = ishistoric = isdiluted = 0;
    mntmp = NON_PM;
#define UNDEFINED 0
#define EMPTY 1
#define SPINACH 2
    contents = UNDEFINED;
    oclass = 0;
    invlet = 0;
    actualn = dn = un = 0;

    if (!bp)
        goto any;
    /* first, remove extra whitespace they may have typed */
    mungspaces(bp);
    /* allow wishing for "nothing" to preserve wishless conduct... [now
       requires "wand of nothing" if that's what was really wanted] */
    if (!strcmpi(bp, "nothing") || !strcmpi(bp, "nil") || !strcmpi(bp, "none"))
        return no_wish;
    /* strip off an inventory letter, if one is specified */
    if ((('a' <= *bp && *bp <= 'z') || ('A' <= *bp && *bp <= 'Z') || *bp == '$')
        && bp[1] == ' ' && bp[2] == '-' && bp[3] == ' ') {
        if (*bp != '$')
            invlet = *bp;
        bp += 4;
    }
    /* save the [nearly] unmodified choice string */
    fruitbuf = msg_from_string(bp);

    for (;;) {
        int l;

        if (!bp || !*bp)
            goto any;
        if (!strncmpi(bp, "an ", l = 3) || !strncmpi(bp, "a ", l = 2)) {
            cnt = 1;
        } else if (!strncmpi(bp, "the ", l = 4)) {
            ;   /* just increment `bp' by `l' below */
        } else if (!cnt && digit(*bp) && strcmp(bp, "0")) {
            cnt = atoi(bp);
            while (digit(*bp))
                bp++;
            while (*bp == ' ')
                bp++;
            l = 0;
        } else if (*bp == '+' || *bp == '-') {
            spesgn = (*bp++ == '+') ? 1 : -1;
            spe = atoi(bp);
            while (digit(*bp))
                bp++;
            while (*bp == ' ')
                bp++;
            l = 0;
        } else if (!strncmpi(bp, "blessed ", l = 8) ||
                   !strncmpi(bp, "holy ", l = 5)) {
            blessed = 1;
        } else if (!strncmpi(bp, "cursed ", l = 7) ||
                   !strncmpi(bp, "unholy ", l = 7)) {
            iscursed = 1;
        } else if (!strncmpi(bp, "uncursed ", l = 9)) {
            uncursed = 1;
#ifdef INVISIBLE_OBJECTS
        } else if (!strncmpi(bp, "invisible ", l = 10)) {
            isinvisible = 1;
#endif
        } else if (!strncmpi(bp, "rustproof ", l = 10) ||
                   !strncmpi(bp, "erodeproof ", l = 11) ||
                   !strncmpi(bp, "corrodeproof ", l = 13) ||
                   !strncmpi(bp, "fixed ", l = 6) ||
                   !strncmpi(bp, "fireproof ", l = 10) ||
                   !strncmpi(bp, "rotproof ", l = 9)) {
            erodeproof = 1;
        } else if (!strncmpi(bp, "lit ", l = 4) ||
                   !strncmpi(bp, "burning ", l = 8)) {
            islit = 1;
        } else if (!strncmpi(bp, "unlit ", l = 6) ||
                   !strncmpi(bp, "extinguished ", l = 13)) {
            islit = 0;
            /* "unlabeled" and "blank" are synonymous */
        } else if (!strncmpi(bp, "unlabeled ", l = 10) ||
                   !strncmpi(bp, "unlabelled ", l = 11) ||
                   !strncmpi(bp, "blank ", l = 6)) {
            unlabeled = 1;
        } else if (!strncmpi(bp, "poisoned ", l = 9)
                   || (wizard && !strncmpi(bp, "trapped ", l = 8))) {
            ispoisoned = 1;
        } else if (!strncmpi(bp, "greased ", l = 8)) {
            isgreased = 1;
        } else if (!strncmpi(bp, "very ", l = 5)) {
            /* very rusted very heavy iron ball */
            very = 1;
        } else if (!strncmpi(bp, "thoroughly ", l = 11)) {
            very = 2;
        } else if (!strncmpi(bp, "rusty ", l = 6) ||
                   !strncmpi(bp, "rusted ", l = 7) ||
                   !strncmpi(bp, "burnt ", l = 6) ||
                   !strncmpi(bp, "burned ", l = 7)) {
            eroded = 1 + very;
            very = 0;
        } else if (!strncmpi(bp, "corroded ", l = 9) ||
                   !strncmpi(bp, "rotted ", l = 7)) {
            eroded2 = 1 + very;
            very = 0;
        } else if (!strncmpi(bp, "partly eaten ", l = 13)) {
            halfeaten = 1;
        } else if (!strncmpi(bp, "historic ", l = 9)) {
            ishistoric = 1;
        } else if (!strncmpi(bp, "diluted ", l = 8)) {
            isdiluted = 1;
        } else if (!strncmpi(bp, "empty ", l = 6)) {
            contents = EMPTY;
        } else if (!strncmpi(bp, "vorpal ", l = 7) &&
                   strncmpi(bp, "vorpal blade", 12)) {
            props |= opm_vorpal;
        } else if (!strncmpi(bp, "thirsty ", l = 8)) {
            props |= opm_drain;
        } else if (!strncmpi(bp, "nasty ", l = 6)) {
            props |= opm_nasty;
        } else if (!strncmpi(bp, "magical ", l = 8)) {
            magical = TRUE;
        } else if (!strncmpi(bp, "oilskin ", l = 8) &&
                   /* Skip oilskin cloak except for the ones given
                      below since otherwise the wishing parser would
                      try to read them as object suffixes, failing.
                      DynaHack included "of displacement" here
                      for consistency, but that causes "oilskin cloak
                      of displacement" to be disallowed as a non-wizmode
                      wish since you're not allowed to wish for
                      properties on magical armor (so you had to use
                      "slippery cloak of displacement"). */
                   (strncmpi(bp, "oilskin cloak", 13) ||
                    !strncmpi(bp+13, " of protection", 14) ||
                    !strncmpi(bp+13, " of invisibility", 16) ||
                    !strncmpi(bp+13, " of magic resistance", 20)) &&
                   strncmpi(bp, "oilskin sack", 12)) {
            props |= opm_oilskin;
        } else
            break;
        bp += l;
    }
    if (!cnt)
        cnt = 1;        /* %% what with "gems" etc. ? */
    if (strlen(bp) > 1) {
        if ((p = strrchr(bp, '(')) != 0) {
            if (p > bp && p[-1] == ' ')
                p[-1] = 0;
            else
                *p = 0;
            p++;
            if (!strcmpi(p, "lit)")) {
                islit = 1;
            } else {
                spe = atoi(p);
                while (digit(*p))
                    p++;
                if (*p == ':') {
                    p++;
                    rechrg = spe;
                    spe = atoi(p);
                    while (digit(*p))
                        p++;
                }
                if (*p != ')') {
                    spe = rechrg = 0;
                } else {
                    spesgn = 1;
                    p++;
                    if (*p)
                        strcat(bp, p);
                }
            }
        }
    }
/*
   otmp->spe is type schar; so we don't want spe to be any bigger or smaller.
   also, spe should always be positive  -- some cheaters may try to confuse
   atoi()
*/
    if (spe < 0) {
        spesgn = -1;    /* cheaters get what they deserve */
        spe = abs(spe);
    }
    if (spe > SCHAR_LIM)
        spe = SCHAR_LIM;
    if (rechrg < 0 || rechrg > 7)
        rechrg = 7;     /* recharge_limit */

    /* now we have the actual name, as delivered by xname, say green potions
       called whisky scrolls labeled "QWERTY" egg fortune cookies very heavy
       iron ball named hoei wand of wishing elven cloak */
    if ((p = strstri_mutable(bp, " named ")) != 0) {
        *p = 0;
        name = p + 7;
    }
    if ((p = strstri_mutable(bp, " called ")) != 0) {
        *p = 0;
        un = p + 8;
        /* "helmet called telepathy" is not "helmet" (a specific type) "shield
           called reflection" is not "shield" (a general type) */
        for (i = 0; i < SIZE(o_ranges); i++)
            if (!strcmpi(bp, o_ranges[i].name)) {
                oclass = o_ranges[i].oclass;
                goto srch;
            }
    }
    if ((p = strstri_mutable(bp, " labeled ")) != 0) {
        *p = 0;
        dn = p + 9;
    } else if ((p = strstri_mutable(bp, " labelled ")) != 0) {
        *p = 0;
        dn = p + 10;
    }
    if ((p = strstri_mutable(bp, " of spinach")) != 0) {
        *p = 0;
        contents = SPINACH;
    }

    /* 
       Skip over "pair of ", "pairs of", "set of" and "sets of".

       Accept "3 pair of boots" as well as "3 pairs of boots". It is valid
       English either way.  See makeplural() for more on pair/pairs.

       We should only double count if the object in question is not refered to
       as a "pair of".  E.g. We should double if the player types "pair of
       spears", but not if the player types "pair of lenses".  Luckily (?) all
       objects that are refered to as pairs -- boots, gloves, and lenses -- are 
       also not mergable, so cnt is ignored anyway. */
    if (!strncmpi(bp, "pair of ", 8)) {
        bp += 8;
        cnt *= 2;
    } else if (cnt > 1 && !strncmpi(bp, "pairs of ", 9)) {
        bp += 9;
        cnt *= 2;
    } else if (!strncmpi(bp, "set of ", 7)) {
        bp += 7;
    } else if (!strncmpi(bp, "sets of ", 8)) {
        bp += 8;
    }

    /* 
     * Find corpse type using "of" (figurine of an orc, tin of orc meat)
     * Don't check if it's a wand or spellbook.
     * (avoid "wand/finger of death" confusion or
     * scroll(s) of fire).
     */
    if (!strstri(bp, "wand ") &&
        !strstri(bp, "spellbook ") &&
        !strstri(bp, "finger ") &&
        !strstri(bp, "scroll ") &&
        !strstri(bp, "scrolls ") &&
        !strstri(bp, "potion ") &&
        !strstri(bp, "potions ")) {
        int l = 0;
        int of = 4;
        char *tmpp;

        p = bp;
        while (p != NULL) {
            tmpp = strstri_mutable(p, " of ");
            if (tmpp) {
                if ((tmpp - 6 == bp && !strncmpi(tmpp - 6, "amulet", 6)) ||
                    (tmpp - 5 == bp && !strncmpi(tmpp - 5, "cloak", 5)) ||
                    (tmpp - 6 == bp && !strncmpi(tmpp - 6, "shield", 6)) ||
                    (tmpp - 9 == bp && !strncmpi(tmpp - 9, "gauntlets", 9)) ||
                    (tmpp - 4 == bp && !strncmpi(tmpp - 4, "helm", 4)) ||
                    (tmpp - 4 == bp && !strncmpi(tmpp - 4, "ring", 4))) {
                    p = tmpp + 4;
                    continue;
                }
                of = 4;
                p = tmpp;
            } else {
                tmpp = strstri_mutable(p, " and ");
                if (tmpp) {
                    of = 5;
                    p = tmpp;
                } else if ((tmpp = strstri_mutable(p, ", "))) {
                    of = 2;
                    p = tmpp;
                } else {
                    p = NULL;
                    break;
                }
            }

            l = 0;

            if ((mntmp = name_to_mon(p + of)) >= LOW_PM) {
                *p = '\0';
                p = NULL;
            } else if (!strncmpi(p + of, "fire", l=4)) {
                props |= opm_fire;
            } else if (!strncmpi(p + of, "frost", l=5) ||
                       !strncmpi(p + of, "cold", l=4)) {
                props |= opm_frost;
            } else if (!strncmpi(p + of, "shock", l=5) ||
                       !strncmpi(p + of, "lightning", l=9)) {
                props |= opm_shock;
            } else if (!strncmpi(p + of, "drain", l=5)) {
                props |= opm_drain;
                /* no "of vorpal"; require the prefix instead */
            } else if (!strncmpi(p + of, "reflection", l=10)) {
                props |= opm_reflects;
            } else if (!strncmpi(p + of, "speed", l=5)) {
                props |= opm_speed;
                /* no "of oilskin"; require the prefix instead */
            } else if (!strncmpi(p + of, "power", l=5)) {
                props |= opm_power;
            } else if (!strncmpi(p + of, "dexterity", l=9)) {
                props |= opm_dexterity;
            } else if (!strncmpi(p + of, "brilliance", l=10)) {
                props |= opm_brilliance;
            } else if (!strncmpi(p + of, "telepathy", l=9) ||
                       !strncmpi(p + of, "ESP", l=3)) {
                props |= opm_telepat;
            } else if (!strncmpi(p + of, "displacement", l=12)) {
                props |= opm_displacement;
            } else if (!strncmpi(p + of, "warning", l=7)) {
                props |= opm_warn;
            } else if (!strncmpi(p + of, "searching", l=9)) {
                props |= opm_search;
            } else if (!strncmpi(p + of, "stealth", l=7)) {
                props |= opm_stealth;
            } else if (!strncmpi(p + of, "fumbling", l=8)) {
                props |= opm_fumble;
            } else if (!strncmpi(p + of, "clairvoyance", l=12)) {
                props |= opm_clairvoyant;
            } else if (!strncmpi(p + of, "detonations", l=11) ||
                       !strncmpi(p + of, "detonation", l=10)) {
                props |= opm_detonate;
            } else if (!strncmpi(p + of, "hunger", l=6)) {
                props |= opm_hunger;
            } else if (!strncmpi(p + of, "aggravation", l=11)) {
                props |= opm_aggravate;
            } else if (!strncmpi(p + of, "mercy", l=5)) {
                props |= opm_mercy;
            } else if (!strncmpi(p + of, "carrying", l=8)) {
                props |= opm_carrying;
            } else
                l = 0;

            if (l > 0)
                *p = '\0';

            if (p)
                p += of + l;
        }
    }

    /* Find corpse type w/o "of" (red dragon scale mail, yeti corpse) */
    if (strncmpi(bp, "samurai sword", 13) &&    /* not the "samurai" monster! */
        strncmpi(bp, "wizard lock", 11) &&      /* not the "wizard" monster! */
        strncmpi(bp, "ninja-to", 8) &&          /* not the "ninja" rank */
        strncmpi(bp, "master key", 10) &&       /* not the "master" rank */
        strncmpi(bp, "magenta", 7) &&           /* not the "mage" rank */
        mntmp < LOW_PM && strlen(bp) > 2 &&
        (mntmp = name_to_mon(bp)) >= LOW_PM) {
        int mntmptoo, mntmplen; /* double check for rank title */
        char *obp = bp;

        mntmptoo = title_to_mon(bp, NULL, &mntmplen);
        bp += mntmp != mntmptoo ?
            (int)strlen(mons[mntmp].mname) : mntmplen;
        if (*bp == ' ')
            bp++;
        else if (!strncmpi(bp, "s ", 2))
            bp += 2;
        else if (!strncmpi(bp, "es ", 3))
            bp += 3;
        else if (!*bp && !actualn && !dn && !un &&
                 !oclass) {
            /* no referent; they don't really mean a
               monster type */
            bp = obp;
            mntmp = NON_PM;
        }
    }

    /* in debug mode, we allow wishing for otyps by number; this is intended for
       programmatic wishing (it's a pain to find a list of item names) */
    if (wizard && sscanf(bp, "otyp #%d", &typ) == 1 &&
        typ > STRANGE_OBJECT && typ < NUM_OBJECTS)
        goto typfnd;
    typ = 0;

    /* first change to singular if necessary */
    if (*bp) {
        const char *sng = makesingular(bp);

        if (strcmp(bp, sng)) {
            if (cnt == 1)
                cnt = 2;
            strcpy(bp, sng);
        }
    }

    /* Alternate spellings (pick-ax, silver sabre, &c) */
    while (as->sp) {
        if (fuzzymatch(bp, as->sp, " -", TRUE)) {
            typ = as->ob;
            goto typfnd;
        }
        as++;
    }
    /* can't use spellings list for this one due to shuffling */
    if (!strncmpi(bp, "grey spell", 10))
        *(bp + 2) = 'a';

    /* dragon scales - assumes order of dragons */
    if (!strcmpi(bp, "scales") && mntmp >= PM_GRAY_DRAGON &&
        mntmp <= PM_YELLOW_DRAGON) {
        typ = GRAY_DRAGON_SCALES + mntmp - PM_GRAY_DRAGON;
        mntmp = NON_PM; /* no monster */
        goto typfnd;
    }

    p = bp + strlen(bp);
    if (!BSTRCMPI(bp, p - 10, "holy water")) {
        typ = POT_WATER;
        if ((p - bp) >= 12 && *(p - 12) == 'u')
            iscursed = 1;       /* unholy water */
        else
            blessed = 1;
        goto typfnd;
    }
    if (unlabeled && !BSTRCMPI(bp, p - 6, "scroll")) {
        typ = SCR_BLANK_PAPER;
        goto typfnd;
    }
    if (unlabeled && !BSTRCMPI(bp, p - 9, "spellbook")) {
        typ = SPE_BLANK_PAPER;
        goto typfnd;
    }
    /* 
     * NOTE: Gold pieces are handled as objects nowadays, and therefore
     * this section should probably be reconsidered as well as the entire
     * gold/money concept.  Maybe we want to add other monetary units as
     * well in the future. (TH)
     */
    if (!BSTRCMPI(bp, p - 10, "gold piece") || !BSTRCMPI(bp, p - 7, "zorkmid")
        || !strcmpi(bp, "gold") || !strcmpi(bp, "money") || !strcmpi(bp, "coin")
        || *bp == GOLD_SYM) {
        if (cnt > 5000 && !wizard)
            cnt = 5000;
        if (cnt < 1)
            cnt = 1;
        otmp = mksobj(level, GOLD_PIECE, FALSE, FALSE, rng_main);
        otmp->quan = cnt;
        otmp->owt = weight(otmp);
        return otmp;
    }
    if (strlen(bp) == 1 && (i = def_char_to_objclass(*bp)) < MAXOCLASSES &&
        i > ILLOBJ_CLASS && i != VENOM_CLASS) {
        oclass = i;
        goto any;
    }

    /* Search for class names: XXXXX potion, scroll of XXXXX.  Avoid */
    /* false hits on, e.g., rings for "ring mail". */
    if (strncmpi(bp, "enchant ", 8) && strncmpi(bp, "destroy ", 8) &&
        strncmpi(bp, "food detection", 14) && strncmpi(bp, "ring mail", 9) &&
        strncmpi(bp, "studded leather arm", 19) &&
        strncmpi(bp, "leather arm", 11) && strncmpi(bp, "tooled horn", 11) &&
        strncmpi(bp, "food ration", 11) && strncmpi(bp, "meat ring", 9)
        )
        for (i = 0; i < (int)(sizeof wrpsym); i++) {
            int j = strlen(wrp[i]);

            if (!strncmpi(bp, wrp[i], j)) {
                oclass = wrpsym[i];
                if (oclass != AMULET_CLASS) {
                    bp += j;
                    if (!strncmpi(bp, " of ", 4))
                        actualn = bp + 4;
                    /* else if (*bp) ?? */
                } else
                    actualn = bp;
                goto srch;
            }
            if (!BSTRCMPI(bp, p - j, wrp[i])) {
                oclass = wrpsym[i];
                p -= j;
                *p = 0;
                if (p > bp && p[-1] == ' ')
                    p[-1] = 0;
                actualn = dn = bp;
                goto srch;
            }
        }

    /* "grey stone" check must be before general "stone" */
    for (i = 0; i < SIZE(o_ranges); i++)
        if (!strcmpi(bp, o_ranges[i].name)) {
            if ((!strcmpi(bp, "boots") ||
                 !strcmpi(bp, "shoes")) &&
                props) {
                typ = 0;
                if (props & opm_speed)
                    typ = SPEED_BOOTS;
                else if (props & opm_fumble)
                    typ = FUMBLE_BOOTS;
            }

            if (!typ)
                typ = rnd_class(o_ranges[i].f_o_range, o_ranges[i].l_o_range,
                                rng_main);
            goto typfnd;
        }

    if (!BSTRCMPI(bp, p - 6, " stone")) {
        p[-6] = 0;
        oclass = GEM_CLASS;
        dn = actualn = bp;
        goto srch;
    } else if (!strcmpi(bp, "looking glass")) {
        ;       /* avoid false hit on "* glass" */
    } else if (!BSTRCMPI(bp, p - 6, " glass") || !strcmpi(bp, "glass")) {
        char *g = bp;

        if (strstri(g, "broken"))
            return NULL;
        if (!strncmpi(g, "worthless ", 10))
            g += 10;
        if (!strncmpi(g, "piece of ", 9))
            g += 9;
        if (!strncmpi(g, "colored ", 8))
            g += 8;
        else if (!strncmpi(g, "coloured ", 9))
            g += 9;
        if (!strcmpi(g, "glass")) {     /* choose random color */
            /* 9 different kinds */
            typ = LAST_GEM + rnd(9);
            if (objects[typ].oc_class == GEM_CLASS)
                goto typfnd;
            else
                typ = 0;        /* somebody changed objects[]? punt */
        } else {        /* try to construct canonical form */
            char tbuf[BUFSZ];

            strcpy(tbuf, "worthless piece of ");
            strcat(tbuf, g);    /* assume it starts with the color */
            strcpy(bp, tbuf);
        }
    }

    actualn = bp;
    if (!dn)
        dn = actualn;   /* ex. "skull cap" */
srch:
    /* check real names of gems first */
    if (!oclass && actualn) {
        for (i = bases[GEM_CLASS]; i <= LAST_GEM; i++) {
            const char *zn;

            if ((zn = OBJ_NAME(objects[i])) && !strcmpi(actualn, zn)) {
                typ = i;
                goto typfnd;
            }
        }
    }
    i = oclass ? bases[(int)oclass] : 1;
    while (i < NUM_OBJECTS && (!oclass || objects[i].oc_class == oclass)) {
        const char *zn;

        if (actualn && (zn = OBJ_NAME(objects[i])) != 0 &&
            wishymatch(actualn, zn, TRUE)) {
            typ = i;
            goto typfnd;
        }
        if (dn && (zn = OBJ_DESCR(objects[i])) != 0 &&
            wishymatch(dn, zn, FALSE)) {
            /* don't match extra descriptions (w/o real name) */
            if (!OBJ_NAME(objects[i]))
                return NULL;
            typ = i;
            goto typfnd;
        }
        if (un && (zn = objects[i].oc_uname) != 0 &&
            wishymatch(un, zn, FALSE)) {
            typ = i;
            goto typfnd;
        }
        i++;
    }
    if (actualn) {
        const struct Jitem *j = Japanese_items;

        while (j->item) {
            if (actualn && !strcmpi(actualn, j->name)) {
                typ = j->item;
                goto typfnd;
            }
            j++;
        }
    }
    if (!strcmpi(bp, "spinach")) {
        contents = SPINACH;
        typ = TIN;
        goto typfnd;
    }
    /* Note: not strncmpi.  2 fruits, one capital, one not, are possible. */
    {
        const char *fp;
        int l, cntf;
        int blessedf, iscursedf, uncursedf, halfeatenf;

        blessedf = iscursedf = uncursedf = halfeatenf = 0;
        cntf = 0;

        fp = fruitbuf;
        for (;;) {
            if (!fp || !*fp)
                break;
            if (!strncmpi(fp, "an ", l = 3) || !strncmpi(fp, "a ", l = 2)) {
                cntf = 1;
            } else if (!cntf && digit(*fp)) {
                cntf = atoi(fp);
                while (digit(*fp))
                    fp++;
                while (*fp == ' ')
                    fp++;
                l = 0;
            } else if (!strncmpi(fp, "blessed ", l = 8)) {
                blessedf = 1;
            } else if (!strncmpi(fp, "cursed ", l = 7)) {
                iscursedf = 1;
            } else if (!strncmpi(fp, "uncursed ", l = 9)) {
                uncursedf = 1;
            } else if (!strncmpi(fp, "partly eaten ", l = 13)) {
                halfeatenf = 1;
            } else
                break;
            fp += l;
        }

        for (f = gamestate.fruits.chain; f; f = f->nextf) {
            const char *f1 = f->fname, *f2 = makeplural(f->fname);

            if (!strncmp(fp, f1, strlen(f1)) || !strncmp(fp, f2, strlen(f2))) {
                typ = SLIME_MOLD;
                blessed = blessedf;
                iscursed = iscursedf;
                uncursed = uncursedf;
                halfeaten = halfeatenf;
                cnt = cntf;
                ftype = f->fid;
                goto typfnd;
            }
        }

        /* make it possible to wish for "fruit" */
        if (!strcmpi(bp, "fruit")) {
            typ = SLIME_MOLD;
            goto typfnd;
        }
    }

    if (!oclass && actualn) {
        short objtyp;

        /* Perhaps it's an artifact specified by name, not type */
        name = artifact_name(actualn, &objtyp);
        if (name) {
            typ = objtyp;
            goto typfnd;
        }
    }

    /* Let wizards wish for traps --KAA */
    /* must come after objects check so wizards can still wish for trap objects 
       like beartraps */
    if (wizard && from_user) {
        int trap;

        for (trap = NO_TRAP + 1; trap < TRAPNUM; trap++) {
            const char *tname;

            tname = trapexplain[trap - 1];
            if (!strncmpi(tname, bp, strlen(tname))) {
                /* avoid stupid mistakes */
                if ((trap == TRAPDOOR || trap == HOLE)
                    && !can_fall_thru(level))
                    trap = ROCKTRAP;
                maketrap(level, u.ux, u.uy, trap, rng_main);
                pline(msgc_info, "%s.", An(tname));
                return &zeroobj;
            }
        }
        /* or some other dungeon features -dlc */
        p = bp + strlen(bp);
        if (!BSTRCMP(bp, p - 8, "fountain")) {
            level->locations[u.ux][u.uy].typ = FOUNTAIN;
            if (!strncmpi(bp, "magic ", 6))
                level->locations[u.ux][u.uy].blessedftn = 1;
            pline(msgc_info, "A %sfountain.",
                  level->locations[u.ux][u.uy].blessedftn ? "magic " : "");
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 6, "throne")) {
            level->locations[u.ux][u.uy].typ = THRONE;
            pline(msgc_info, "A throne.");
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 4, "sink")) {
            level->locations[u.ux][u.uy].typ = SINK;
            pline(msgc_info, "A sink.");
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 4, "pool")) {
            level->locations[u.ux][u.uy].typ = POOL;
            del_engr_at(level, u.ux, u.uy);
            pline(msgc_info, "A pool.");
            /* Must manually make kelp! */
            water_damage_chain(level->objects[u.ux][u.uy], TRUE);
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
        if (!BSTRCMP(bp, p - 4, "lava")) {      /* also matches "molten lava" */
            level->locations[u.ux][u.uy].typ = LAVAPOOL;
            del_engr_at(level, u.ux, u.uy);
            pline(msgc_info, "A pool of molten lava.");
            if (!(Levitation || Flying))
                lava_effects();
            newsym(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 5, "altar")) {
            aligntyp al;

            level->locations[u.ux][u.uy].typ = ALTAR;
            if (!strncmpi(bp, "chaotic ", 8))
                al = A_CHAOTIC;
            else if (!strncmpi(bp, "neutral ", 8))
                al = A_NEUTRAL;
            else if (!strncmpi(bp, "lawful ", 7))
                al = A_LAWFUL;
            else if (!strncmpi(bp, "unaligned ", 10))
                al = A_NONE;
            else        /* -1 - A_CHAOTIC, 0 - A_NEUTRAL, 1 - A_LAWFUL */
                al = (!rn2(6)) ? A_NONE : rn2((int)A_LAWFUL + 2) - 1;
            level->locations[u.ux][u.uy].altarmask = Align2amask(al);
            pline(msgc_info, "%s altar.", An(align_str(al)));
            newsym(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 5, "grave") || !BSTRCMP(bp, p - 9, "headstone")) {
            make_grave(level, u.ux, u.uy, NULL);
            pline(msgc_info, "A grave.");
            newsym(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 4, "tree")) {
            level->locations[u.ux][u.uy].typ = TREE;
            pline(msgc_info, "A tree.");
            newsym(u.ux, u.uy);
            block_point(u.ux, u.uy);
            return &zeroobj;
        }

        if (!BSTRCMP(bp, p - 4, "bars")) {
            level->locations[u.ux][u.uy].typ = IRONBARS;
            pline(msgc_info, "Iron bars.");
            newsym(u.ux, u.uy);
            return &zeroobj;
        }
    }

    if (!oclass)
        return NULL;
any:
    if (!oclass)
        oclass = wrpsym[rn2((int)sizeof (wrpsym))];
typfnd:
    if (typ)
        oclass = objects[typ].oc_class;

    /* check for some objects that are not allowed */
    if (typ && objects[typ].oc_unique) {
        if (!wizard)
            switch (typ) {
            case AMULET_OF_YENDOR:
                typ = FAKE_AMULET_OF_YENDOR;
                break;
            case CANDELABRUM_OF_INVOCATION:
                typ = rnd_class(TALLOW_CANDLE, WAX_CANDLE, rng_main);
                break;
            case BELL_OF_OPENING:
                typ = BELL;
                break;
            case SPE_BOOK_OF_THE_DEAD:
                typ = SPE_BLANK_PAPER;
                break;
            }
    }

    /* catch any other non-wishable objects */
    if (objects[typ].oc_nowish && !wizard)
        return NULL;

    if (typ) {
        otmp = mksobj(level, typ, TRUE, FALSE, rng_main);
    } else {
        otmp = mkobj(level, oclass, FALSE, rng_main);
        if (otmp)
            typ = otmp->otyp;
        else
            panic("Could not produce object of class in wish");
    }

    if (invlet)
        otmp->invlet = invlet;

    if (islit &&
        (typ == OIL_LAMP || typ == MAGIC_LAMP || typ == BRASS_LANTERN ||
         Is_candle(otmp) || typ == POT_OIL)) {
        place_object(otmp, level, u.ux, u.uy); /* make it viable light source */
        begin_burn(otmp, FALSE);
        obj_extract_self(otmp); /* now release it for caller's use */
    }

    if (cnt > 0 && objects[typ].oc_merge && oclass != SPBOOK_CLASS &&
        (wizard || (cnt <= 7 && Is_candle(otmp)) ||
         (cnt <= 20 && ((oclass == WEAPON_CLASS && is_ammo(otmp))
                        || typ == ROCK || is_missile(otmp))) ||
         cnt <= rn2_on_rng(6, rng_wish_quantity)))
        otmp->quan = (long)cnt;

    if (spesgn == 0)
        spe = otmp->spe;
    else if (wizard)    /* no alteration to spe */
        ;
    else if (oclass == ARMOR_CLASS || oclass == WEAPON_CLASS || is_weptool(otmp)
             || (oclass == RING_CLASS && objects[typ].oc_charged)) {
        if (spe > 1 + rn2_on_rng(5, rng_wish_quality))
            spe = 0;
        if (spe > 2 && Luck < 0)
            spesgn = -1;
    } else {
        if (oclass == WAND_CLASS) {
            if (spe > 1 && spesgn == -1)
                spe = 1;
        } else {
            if (spe > 0 && spesgn == -1)
                spe = 0;
        }
        if (spe > otmp->spe)
            spe = otmp->spe;
    }

    if (spesgn == -1)
        spe = -spe;

    /* set otmp->spe.  This may, or may not, use spe... */
    switch (typ) {
    case TIN:
        if (contents == EMPTY) {
            otmp->corpsenm = NON_PM;
            otmp->spe = 0;
        } else if (contents == SPINACH) {
            otmp->corpsenm = NON_PM;
            otmp->spe = 1;
        }
        break;
    case SLIME_MOLD:
        otmp->spe = ftype;
        /* Fall through */
    case SKELETON_KEY:
    case CHEST:
    case LARGE_BOX:
    case HEAVY_IRON_BALL:
    case IRON_CHAIN:
    case STATUE:
        /* otmp->cobj already done in mksobj() */
        break;
    case WAN_WISHING:
        if (!wizard) {
            otmp->spe = (rn2(10) ? -1 : 0);
            break;
        }
        /* fall through, if wizard */
    default:
        otmp->spe = spe;
    }

    /* set otmp->corpsenm or dragon scale [mail] */
    if (mntmp >= LOW_PM) {
        if (mntmp == PM_LONG_WORM_TAIL)
            mntmp = PM_LONG_WORM;

        switch (typ) {
        case TIN:
            otmp->spe = 0;      /* No spinach */
            if (dead_species(mntmp, FALSE)) {
                otmp->corpsenm = NON_PM;        /* it's empty */
            } else if (!(mons[mntmp].geno & G_UNIQ) &&
                       !(mvitals[mntmp].mvflags & G_NOCORPSE) &&
                       mons[mntmp].cnutrit != 0) {
                otmp->corpsenm = mntmp;
            }
            break;
        case CORPSE:
            /* Undead like zombies and vampires all have G_NOCORPSE as their
               corpses are special-cased.  Note that undead_to_corpse
               returns its own input if it doesn't correspond to a valid
               undead. */
            if (undead_to_corpse(mntmp) != mntmp) {
                mntmp = undead_to_corpse(mntmp);
                otmp->age -= 100;
            }
            if (!(mons[mntmp].geno & G_UNIQ) &&
                !(mvitals[mntmp].mvflags & G_NOCORPSE)) {
                /* beware of random troll or lizard corpse, or of ordinary one
                   being forced to such */
                if (otmp->timed)
                    obj_stop_timers(otmp);
                if (mons[mntmp].msound == MS_GUARDIAN)
                    otmp->corpsenm = genus(mntmp, 1);
                else
                    otmp->corpsenm = mntmp;
                start_corpse_timeout(otmp);
            }
            break;
        case FIGURINE:
            if (!(mons[mntmp].geno & G_UNIQ)
                && !is_human(&mons[mntmp]))
                otmp->corpsenm = mntmp;
            break;
        case EGG:
            mntmp = can_be_hatched(mntmp);
            if (mntmp != NON_PM) {
                otmp->corpsenm = mntmp;
                if (!dead_species(mntmp, TRUE))
                    attach_egg_hatch_timeout(otmp);
                else
                    kill_egg(otmp);
            }
            break;
        case STATUE:
            otmp->corpsenm = mntmp;
            if (Has_contents(otmp) && verysmall(&mons[mntmp]))
                delete_contents(otmp);  /* no spellbook */
            otmp->spe = ishistoric ? STATUE_HISTORIC : 0;
            break;
        case SCALE_MAIL:
            /* Dragon mail - depends on the order of objects & dragons. */
            if (mntmp >= PM_GRAY_DRAGON && mntmp <= PM_YELLOW_DRAGON)
                otmp->otyp = GRAY_DRAGON_SCALE_MAIL + mntmp - PM_GRAY_DRAGON;
            break;
        }
    }

    /* set blessed/cursed -- setting the fields directly is safe since weight() 
       is called below and addinv() will take care of luck */
    if (iscursed) {
        curse(otmp);
    } else if (uncursed) {
        otmp->blessed = 0;
        otmp->cursed = (Luck < 0 && !wizard);
    } else if (blessed) {
        otmp->blessed = (Luck >= 0 || wizard);
        otmp->cursed = (Luck < 0 && !wizard);
    } else if (spesgn < 0) {
        curse(otmp);
    }

    /* Kill existing object properties if any generated */
    otmp->oprops = 0LLU;

    /* In wizard mode only, allow wishing for properties */
    if (!otmp->oartifact && wizard) {
        otmp->oprops |= props;
        otmp->oprops = obj_properties(otmp); /* filter invalid ones */
        props = otmp->oprops;

        if (magical && !props) {
            /* Ensure that properties are allowed on this */
            otmp->oprops = opm_all;
            otmp->oprops = obj_properties(otmp);
            if (otmp->oprops) {
                otmp->oprops = 0LLU;
                while (!otmp->oprops)
                    assign_oprops(level, otmp, rng_main, FALSE);
                props = otmp->oprops;
            }
        }

        /* Restrict properties, in case we allow it in the future */
        if (props && !wizard) {
            /* Don't allow properties at all on magical items */
            if (otmp->oclass == TOOL_CLASS && !is_weptool(otmp))
                props = 0LLU;
            if (objects[otmp->otyp].oc_magic ||
                objects[otmp->otyp].oc_oprop)
                props = 0LLU;

            uint64_t prop = 0LLU;
            if (props) {
                do {
                    prop = ((uint64_t)1 << rn2(64));
                } while (!(prop & props));
            }
            props = prop;
            otmp->oprops = prop;
        }
    }

#ifdef INVISIBLE_OBJECTS
    if (isinvisible)
        otmp->oinvis = 1;
#endif

    /* set eroded */
    if (is_damageable(otmp) || otmp->otyp == CRYSKNIFE) {
        if (eroded && (is_flammable(otmp) || is_rustprone(otmp)))
            otmp->oeroded = eroded;
        if (eroded2 && (is_corrodeable(otmp) || is_rottable(otmp)))
            otmp->oeroded2 = eroded2;

        /* set erodeproof */
        if (erodeproof && !eroded && !eroded2)
            otmp->oerodeproof = (Luck >= 0 || wizard);
    }

    /* set otmp->recharged */
    if (oclass == WAND_CLASS) {
        /* prevent wishing abuse */
        if (otmp->otyp == WAN_WISHING && !wizard)
            rechrg = 1;
        otmp->recharged = (unsigned)rechrg;
    }

    /* set poisoned */
    if (ispoisoned) {
        if (is_poisonable(otmp))
            otmp->opoisoned = (Luck >= 0);
        else if (Is_box(otmp) || typ == TIN)
            otmp->otrapped = 1;
        else if (oclass == FOOD_CLASS)
            /* try to taint by making it as old as possible */
            otmp->age = 1L;
    }

    if (isgreased)
        otmp->greased = 1;

    if (isdiluted && otmp->oclass == POTION_CLASS && otmp->otyp != POT_WATER)
        otmp->odiluted = 1;

    if (name) {
        const char *aname;
        short objtyp;

        /* an artifact name might need capitalization fixing */
        aname = artifact_name(name, &objtyp);
        if (aname && objtyp == otmp->otyp)
            name = aname;

        otmp = oname(otmp, name);
        if (otmp->oartifact) {
            artifact_exists(otmp, ox_name(otmp), ag_wish);
            otmp->quan = 1L;
            if (!flags.mon_moving)
                break_conduct(conduct_artiwish);      /* KMH, conduct */
        }
    }

    /* more wishing abuse: don't allow wishing for certain artifacts */
    /* and make them pay; charge them for the wish anyway! */
    if ((is_quest_artifact(otmp) ||
         (otmp->oartifact &&
          rn2_on_rng(nartifact_wished(), rng_artifact_wish))) && !wizard) {
        artifact_exists(otmp, ox_name(otmp), ag_none);
        obfree(otmp, NULL);
        otmp = &zeroobj;
        if (!flags.mon_moving)
            pline(msgc_nospoil,
                  "For a moment, you feel something in your %s, but it disappears!",
                  makeplural(body_part(HAND)));
    }

    if (halfeaten && otmp->oclass == FOOD_CLASS) {
        if (otmp->otyp == CORPSE)
            otmp->oeaten = mons[otmp->corpsenm].cnutrit / 2;
        else
            otmp->oeaten = objects[otmp->otyp].oc_nutrition / 2;
        /* (do this adjustment before setting up object's weight) */
    }
    otmp->owt = weight(otmp);
    if (very && otmp->otyp == HEAVY_IRON_BALL)
        otmp->owt += 160;

    return otmp;
}

int
rnd_class(int first, int last, enum rng rng)
{
    int i, x, sum = 0;

    if (first == last)
        return first;
    for (i = first; i <= last; i++)
        sum += objects[i].oc_prob;
    if (!sum)   /* all zero */
        return first + rn2_on_rng(last - first + 1, rng);
    x = rn2_on_rng(sum, rng) + 1;
    for (i = first; i <= last; i++)
        if (objects[i].oc_prob && (x -= objects[i].oc_prob) <= 0)
            return i;
    return 0;
}

static const char *
Japanese_item_name(int i)
{
    const struct Jitem *j = Japanese_items;

    while (j->item) {
        if (i == j->item)
            return j->name;
        j++;
    }
    return NULL;
}

const char *
cloak_simple_name(const struct obj *cloak)
{
    if (cloak) {
        switch (cloak->otyp) {
        case ROBE:
            return "robe";
        case MUMMY_WRAPPING:
            return "wrapping";
        case ALCHEMY_SMOCK:
            return (objects[cloak->otyp].oc_name_known &&
                    cloak->dknown) ? "smock" : "apron";
        default:
            break;
        }
    }
    return "cloak";
}

const char *
mimic_obj_name(const struct monst *mtmp)
{
    if (mtmp->m_ap_type == M_AP_OBJECT && mtmp->mappearance != STRANGE_OBJECT) {
        int idx = objects[mtmp->mappearance].oc_descr_idx;

        if (mtmp->mappearance == GOLD_PIECE)
            return "gold";
        return obj_descr[idx].oc_name;
    }
    return "whatcha-may-callit";
}

/*objnam.c*/

