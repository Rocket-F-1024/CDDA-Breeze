#include "do_turn.h"

#include "action.h"
#include "avatar.h"
#include "bionics.h"
#include "cached_options.h"
#include "calendar.h"
#include "creature_tracker.h"
#include "event_bus.h"
#include "explosion.h"
#include "game.h"
#include "gamemode.h"
#include "help.h"
#include "kill_tracker.h"
#include "make_static.h"
#include "map.h"
#include "mapbuffer.h"
#include "memorial_logger.h"
#include "messages.h"
#include "mission.h"
#include "monattack.h"
#include "mtype.h"
#include "music.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "popup.h"
#include "scent_map.h"
#include "sdlsound.h"
#include "string_input_popup.h"
#include "timed_event.h"
#include "ui_manager.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "wcwidth.h"
#include "worldfactory.h"

static const activity_id ACT_AUTODRIVE( "ACT_AUTODRIVE" );
static const activity_id ACT_FIRSTAID( "ACT_FIRSTAID" );
static const activity_id ACT_OPERATION( "ACT_OPERATION" );

static const bionic_id bio_alarm( "bio_alarm" );

static const efftype_id effect_controlled( "controlled" );
static const efftype_id effect_npc_suspend( "npc_suspend" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_sleep( "sleep" );

static const itype_id itype_holybook_bible1( "holybook_bible1" );
static const itype_id itype_holybook_bible2( "holybook_bible2" );
static const itype_id itype_holybook_bible3( "holybook_bible3" );

static const trait_id trait_CANNIBAL( "CANNIBAL" );
static const trait_id trait_HAS_NEMESIS( "HAS_NEMESIS" );
static const trait_id trait_PSYCHOPATH( "PSYCHOPATH" );

static const species_id species_ZOMBIE("ZOMBIE");
static const species_id species_ROBOT("ROBOT");

static const faction_id faction_no_faction("no_faction");
static const faction_id faction_free_merchants("free_merchants");

static const efftype_id effect_just_trade("just_trade");
static const efftype_id effect_just_guard("just_guard");
static const efftype_id effect_want_trade("want_trade");
static const efftype_id effect_npc_run_away("npc_run_away");
static const efftype_id effect_npc_flee_player("npc_flee_player");

static const zone_type_id zone_type_trade_area("trade_area");

#if defined(__ANDROID__)
extern std::map<std::string, std::list<input_event>> quick_shortcuts_map;
extern bool add_best_key_for_action_to_quick_shortcuts( action_id action,
        const std::string &category, bool back );
#endif

#define dbg(x) DebugLog((x),D_GAME) << __FILE__ << ":" << __LINE__ << ": "



void make_assassins() {


    // 如果玩家没有睡着，不要生成刺客
    if ( ! get_player_character().has_effect(effect_sleep)) {

        return;

    }

    // 如果玩家的z轴位置不为0，就不用生成刺客了
    if (get_player_character().posz() !=0 ) {

        return;
        
    }

    int chance = rng(1,2);

    shared_ptr_fast<npc> temp = make_shared_fast<npc>();
    temp->normalize();
    temp->randomize();

    if (chance == 1) {

        temp->spawn_at_precise(get_player_character().get_location() + point(-49, -49));
    
    }
    else {

        temp->spawn_at_precise(get_player_character().get_location() + point(49, 49));
    
    }
    
    
    overmap_buffer.insert_npc(temp);
    temp->form_opinion(get_player_character());
    temp->mission = NPC_MISSION_NULL;
    temp->add_new_mission(mission::reserve_random(ORIGIN_ANY_NPC, temp->global_omt_location(),
        temp->getID()));
    std::string new_fac_id = "solo_";
    new_fac_id += temp->name;
    // create a new "lone wolf" faction for this one NPC
    faction* new_solo_fac = g->faction_manager_ptr->add_new_faction(temp->name,
        faction_id(new_fac_id), faction_no_faction);
    temp->set_fac(new_solo_fac ? new_solo_fac->id : faction_no_faction);

    temp->get_faction()->likes_u = temp->get_faction()->likes_u - 1000;
    
    temp->set_attitude(NPCATT_KILL);
    
    g->load_npcs();

}

void make_trade_caravan() {


    // 如果玩家的z轴位置不为0，就不用生成商队
    if (get_player_character().posz() != 0) {
        return;
    }

    int chance = rng(1, 2);

    shared_ptr_fast<npc> temp = make_shared_fast<npc>();
    temp->normalize();
    temp->randomize();

    if (chance == 1) {
        temp->spawn_at_precise(get_player_character().get_location() + point(45+rng(-5,5), 0));
    }
    else {
        temp->spawn_at_precise(get_player_character().get_location() + point(-45+rng(-5, 5), 0));
    }

    overmap_buffer.insert_npc(temp);
    temp->form_opinion(get_player_character());
    temp->mission = NPC_MISSION_NULL;
    
    // 指定生成的派系为商人派系
    temp->set_fac(faction_free_merchants);
       
    // 添加 仅仅交易效果,持续时间为永久
    temp->add_effect(effect_just_trade,1_turns,true);
    
    // 添加 want_trade 效果，正常10分钟
    temp->add_effect(effect_want_trade, 10_minutes);
    
    
    temp->set_attitude(NPCATT_NULL);
 

    g->load_npcs();


    



}





namespace turn_handler
{
bool cleanup_at_end()
{
    avatar &u = get_avatar();
    if( g->uquit == QUIT_DIED || g->uquit == QUIT_SUICIDE ) {
        // // Put (non-hallucinations) into the overmap so they are not lost.
        // for( monster &critter : g->all_monsters() ) {
        //     g->despawn_monster( critter );
        // }
        // // if player has "hunted" trait, remove their nemesis monster on death
        // if( u.has_trait( trait_HAS_NEMESIS ) ) {
        //     overmap_buffer.remove_nemesis();
        // }
        // // Reset NPC factions and disposition
        // g->reset_npc_dispositions();
        // // Save the factions', missions and set the NPC's overmap coordinates
        // // Npcs are saved in the overmap.
        //g->save_factions_missions_npcs(); //missions need to be saved as they are global for all saves.

        // and the overmap, and the local map.
        //g->save_maps(); //Omap also contains the npcs who need to be saved.
    }

    if( g->uquit == QUIT_DIED || g->uquit == QUIT_SUICIDE ) {
        std::vector<std::string> vRip;

        int iMaxWidth = 0;
        int iNameLine = 0;
        int iInfoLine = 0;

        if( u.has_amount( itype_holybook_bible1, 1 ) || u.has_amount( itype_holybook_bible2, 1 ) ||
            u.has_amount( itype_holybook_bible3, 1 ) ) {
            if( !( u.has_trait( trait_CANNIBAL ) || u.has_trait( trait_PSYCHOPATH ) ) ) {
                vRip.emplace_back( "               _______  ___" );
                vRip.emplace_back( "              <       `/   |" );
                vRip.emplace_back( "               >  _     _ (" );
                vRip.emplace_back( "              |  |_) | |_) |" );
                vRip.emplace_back( "              |  | \\ | |   |" );
                vRip.emplace_back( "   ______.__%_|            |_________  __" );
                vRip.emplace_back( " _/                                  \\|  |" );
                iNameLine = vRip.size();
                vRip.emplace_back( "|                                        <" );
                vRip.emplace_back( "|                                        |" );
                iMaxWidth = utf8_width( vRip.back() );
                vRip.emplace_back( "|                                        |" );
                vRip.emplace_back( "|_____.-._____              __/|_________|" );
                vRip.emplace_back( "              |            |" );
                iInfoLine = vRip.size();
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |           <" );
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |   _        |" );
                vRip.emplace_back( "              |__/         |" );
                vRip.emplace_back( "             % / `--.      |%" );
                vRip.emplace_back( "         * .%%|          -< @%%%" ); // NOLINT(cata-text-style)
                vRip.emplace_back( "         `\\%`@|            |@@%@%%" );
                vRip.emplace_back( "       .%%%@@@|%     `   % @@@%%@%%%%" );
                vRip.emplace_back( "  _.%%%%%%@@@@@@%%%__/\\%@@%%@@@@@@@%%%%%%" );

            } else {
                vRip.emplace_back( "               _______  ___" );
                vRip.emplace_back( "              |       \\/   |" );
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |            |" );
                iInfoLine = vRip.size();
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |            |" );
                vRip.emplace_back( "              |           <" );
                vRip.emplace_back( "              |   _        |" );
                vRip.emplace_back( "              |__/         |" );
                vRip.emplace_back( "   ______.__%_|            |__________  _" );
                vRip.emplace_back( " _/                                   \\| \\" );
                iNameLine = vRip.size();
                vRip.emplace_back( "|                                         <" );
                vRip.emplace_back( "|                                         |" );
                iMaxWidth = utf8_width( vRip.back() );
                vRip.emplace_back( "|                                         |" );
                vRip.emplace_back( "|_____.-._______            __/|__________|" );
                vRip.emplace_back( "             % / `_-.   _  |%" );
                vRip.emplace_back( "         * .%%|  |_) | |_)< @%%%" ); // NOLINT(cata-text-style)
                vRip.emplace_back( "         `\\%`@|  | \\ | |   |@@%@%%" );
                vRip.emplace_back( "       .%%%@@@|%     `   % @@@%%@%%%%" );
                vRip.emplace_back( "  _.%%%%%%@@@@@@%%%__/\\%@@%%@@@@@@@%%%%%%" );
            }
        } else {
            vRip.emplace_back( R"(           _________  ____           )" );
            vRip.emplace_back( R"(         _/         `/    \_         )" );
            vRip.emplace_back( R"(       _/      _     _      \_.      )" );
            vRip.emplace_back( R"(     _%\      |_) | |_)       \_     )" );
            vRip.emplace_back( R"(   _/ \/      | \ | |           \_   )" );
            vRip.emplace_back( R"( _/                               \_ )" );
            vRip.emplace_back( R"(|                                   |)" );
            iNameLine = vRip.size();
            vRip.emplace_back( R"( )                                 < )" );
            vRip.emplace_back( R"(|                                   |)" );
            vRip.emplace_back( R"(|                                   |)" );
            vRip.emplace_back( R"(|   _                               |)" );
            vRip.emplace_back( R"(|__/                                |)" );
            iMaxWidth = utf8_width( vRip.back() );
            vRip.emplace_back( R"( / `--.                             |)" );
            vRip.emplace_back( R"(|                                  ( )" );
            iInfoLine = vRip.size();
            vRip.emplace_back( R"(|                                   |)" );
            vRip.emplace_back( R"(|                                   |)" );
            vRip.emplace_back( R"(|     %                         .   |)" );
            vRip.emplace_back( R"(|  @`                            %% |)" );
            vRip.emplace_back( R"(| %@%@%\                *      %`%@%|)" );
            vRip.emplace_back( R"(%%@@@.%@%\%%            `\  %%.%%@@%@)" );
            vRip.emplace_back( R"(@%@@%%%%%@@@@@@%%%%%%%%@@%%@@@%%%@%%@)" );
        }

        const point iOffset( TERMX > FULL_SCREEN_WIDTH ? ( TERMX - FULL_SCREEN_WIDTH ) / 2 : 0,
                             TERMY > FULL_SCREEN_HEIGHT ? ( TERMY - FULL_SCREEN_HEIGHT ) / 2 : 0 );

        catacurses::window w_rip = catacurses::newwin( FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH,
                                   iOffset );
        draw_border( w_rip );

        sfx::do_player_death_hurt( get_player_character(), true );
        sfx::fade_audio_group( sfx::group::weather, 2000 );
        sfx::fade_audio_group( sfx::group::time_of_day, 2000 );
        sfx::fade_audio_group( sfx::group::context_themes, 2000 );
        sfx::fade_audio_group( sfx::group::fatigue, 2000 );

        for( size_t iY = 0; iY < vRip.size(); ++iY ) {
            size_t iX = 0;
            const char *str = vRip[iY].data();
            for( int slen = vRip[iY].size(); slen > 0; ) {
                const uint32_t cTemp = UTF8_getch( &str, &slen );
                if( cTemp != U' ' ) {
                    nc_color ncColor = c_light_gray;

                    if( cTemp == U'%' ) {
                        ncColor = c_green;

                    } else if( cTemp == U'_' || cTemp == U'|' ) {
                        ncColor = c_white;

                    } else if( cTemp == U'@' ) {
                        ncColor = c_brown;

                    } else if( cTemp == U'*' ) {
                        ncColor = c_red;
                    }

                    mvwputch( w_rip, point( iX + FULL_SCREEN_WIDTH / 2 - ( iMaxWidth / 2 ), iY + 1 ), ncColor,
                              cTemp );
                }
                iX += mk_wcwidth( cTemp );
            }
        }

        std::string sTemp;

        center_print( w_rip, iInfoLine++, c_white, _( "Survived:" ) );

        const time_duration survived = calendar::turn - calendar::start_of_game;
        const int minutes = to_minutes<int>( survived ) % 60;
        const int hours = to_hours<int>( survived ) % 24;
        const int days = to_days<int>( survived );

        if( days > 0 ) {
            // NOLINTNEXTLINE(cata-translate-string-literal)
            sTemp = string_format( "%dd %dh %dm", days, hours, minutes );
        } else if( hours > 0 ) {
            // NOLINTNEXTLINE(cata-translate-string-literal)
            sTemp = string_format( "%dh %dm", hours, minutes );
        } else {
            // NOLINTNEXTLINE(cata-translate-string-literal)
            sTemp = string_format( "%dm", minutes );
        }

        center_print( w_rip, iInfoLine++, c_white, sTemp );

        const int iTotalKills = g->get_kill_tracker().monster_kill_count();

        sTemp = _( "Kills:" );
        mvwprintz( w_rip, point( FULL_SCREEN_WIDTH / 2 - 5, 1 + iInfoLine++ ), c_light_gray,
                   ( sTemp + " " ) );
        wprintz( w_rip, c_magenta, "%d", iTotalKills );

        sTemp = _( "In memory of:" );
        mvwprintz( w_rip, point( FULL_SCREEN_WIDTH / 2 - utf8_width( sTemp ) / 2, iNameLine++ ),
                   c_light_gray,
                   sTemp );

        sTemp = u.get_name();
        mvwprintz( w_rip, point( FULL_SCREEN_WIDTH / 2 - utf8_width( sTemp ) / 2, iNameLine++ ), c_white,
                   sTemp );

        sTemp = _( "Last Words:" );
        mvwprintz( w_rip, point( FULL_SCREEN_WIDTH / 2 - utf8_width( sTemp ) / 2, iNameLine++ ),
                   c_light_gray,
                   sTemp );

        int iStartX = FULL_SCREEN_WIDTH / 2 - ( ( iMaxWidth - 4 ) / 2 );
        std::string sLastWords = string_input_popup()
                                 .window( w_rip, point( iStartX, iNameLine ), iStartX + iMaxWidth - 4 - 1 )
                                 .max_length( iMaxWidth - 4 - 1 )
                                 .query_string();
        g->death_screen();
        const bool is_suicide = g->uquit == QUIT_SUICIDE;
        std::chrono::seconds time_since_load =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - g->time_of_last_load );
        std::chrono::seconds total_time_played = g->time_played_at_last_load + time_since_load;
        get_event_bus().send<event_type::game_over>( is_suicide, sLastWords, total_time_played );
        // Struck the save_player_data here to forestall Weirdness
        // g->move_save_to_graveyard();
        g->write_memorial_file( sLastWords );
        get_memorial().clear();
        std::vector<std::string> characters = g->list_active_saves();
        // remove current player from the active characters list, as they are dead
        // std::vector<std::string>::iterator curchar = std::find( characters.begin(),
        //         characters.end(), u.get_save_id() );
        // if( curchar != characters.end() ) {
        //     characters.erase( curchar );
        // }

        if( characters.empty() ) {
            bool queryDelete = false;
            bool queryReset = false;

            if( get_option<std::string>( "WORLD_END" ) == "query" ) {
                bool decided = false;
                std::string buffer = _( "Warning: NPC interactions and some other global flags "
                                        "will not all reset when starting a new character in an "
                                        "already-played world.  This can lead to some strange "
                                        "behavior.\n\n"
                                        "Are you sure you wish to keep this world?"
                                      );

                while( !decided ) {
                    uilist smenu;
                    smenu.allow_cancel = false;
                    smenu.addentry( 0, true, 'r', "%s", _( "Reset world" ) );
                    smenu.addentry( 1, true, 'd', "%s", _( "Delete world" ) );
                    smenu.addentry( 2, true, 'k', "%s", _( "Keep world" ) );
                    smenu.query();

                    switch( smenu.ret ) {
                        case 0:
                            queryReset = true;
                            decided = true;
                            break;
                        case 1:
                            queryDelete = true;
                            decided = true;
                            break;
                        case 2:
                            decided = query_yn( buffer );
                            break;
                    }
                }
            }

            if( queryDelete || get_option<std::string>( "WORLD_END" ) == "delete" ) {
                world_generator->delete_world( world_generator->active_world->world_name, true );

            } else if( queryReset || get_option<std::string>( "WORLD_END" ) == "reset" ) {
                world_generator->delete_world( world_generator->active_world->world_name, false );
            }
        } else if( get_option<std::string>( "WORLD_END" ) != "keep" ) {
            std::string tmpmessage;
            for( auto &character : characters ) {
                tmpmessage += "\n  ";
                tmpmessage += character;
            }
            popup( _( "World retained.  Characters remaining:%s" ), tmpmessage );
        }
        if( g->gamemode ) {
            g->gamemode = std::make_unique<special_game>(); // null gamemode or something..
        }
    }

    //Reset any offset due to driving
    g->set_driving_view_offset( point_zero );

    //clear all sound channels
    sfx::fade_audio_channel( sfx::channel::any, 300 );
    sfx::fade_audio_group( sfx::group::weather, 300 );
    sfx::fade_audio_group( sfx::group::time_of_day, 300 );
    sfx::fade_audio_group( sfx::group::context_themes, 300 );
    sfx::fade_audio_group( sfx::group::fatigue, 300 );

    zone_manager::get_manager().clear();

    MAPBUFFER.clear();
    overmap_buffer.clear();

#if defined(__ANDROID__)
    quick_shortcuts_map.clear();
#endif
    return true;
}

} // namespace turn_handler

void handle_key_blocking_activity()
{
    if( test_mode ) {
        return;
    }
    avatar &u = get_avatar();
    const bool has_unfinished_activity = u.activity && (
            u.activity.id()->based_on() == based_on_type::NEITHER
            || u.activity.moves_left > 0 );
    if( has_unfinished_activity || u.has_destination() ) {
        input_context ctxt = get_default_mode_input_context();
        const std::string action = ctxt.handle_input( 0 );
        bool refresh = true;
        if( action == "pause" ) {
            if( u.activity.is_interruptible_with_kb() ) {
                g->cancel_activity_query( _( "Confirm:" ) );
            }
        } else if( action == "player_data" ) {
            u.disp_info( true );
        } else if( action == "messages" ) {
            Messages::display_messages();
        } else if( action == "help" ) {
            get_help().display_help();
        } else if( action != "HELP_KEYBINDINGS" ) {
            refresh = false;
        }
        if( refresh ) {
            ui_manager::redraw();
            refresh_display();
        }
    } else {
        refresh_display();
        inp_mngr.pump_events();
    }
}

namespace
{
void monmove()
{
    g->cleanup_dead();
    map &m = get_map();
    avatar &u = get_avatar();

    for( monster &critter : g->all_monsters() ) {
        // Critters in impassable tiles get pushed away, unless it's not impassable for them
        if( !critter.is_dead() && m.impassable( critter.pos() ) && !critter.can_move_to( critter.pos() ) ) {
            dbg( D_ERROR ) << "game:monmove: " << critter.name()
                           << " can't move to its location!  (" << critter.posx()
                           << ":" << critter.posy() << ":" << critter.posz() << "), "
                           << m.tername( critter.pos() );
            add_msg_debug( debugmode::DF_MONSTER, "%s can't move to its location!  (%d,%d,%d), %s",
                           critter.name(),
                           critter.posx(), critter.posy(), critter.posz(), m.tername( critter.pos() ) );
            bool okay = false;
            for( const tripoint &dest : m.points_in_radius( critter.pos(), 3 ) ) {
                if( critter.can_move_to( dest ) && g->is_empty( dest ) ) {
                    critter.setpos( dest );
                    okay = true;
                    break;
                }
            }
            if( !okay ) {
                // die of "natural" cause (overpopulation is natural)
                critter.die( nullptr );
            }
        }

        if( !critter.is_dead() ) {
            critter.process_turn();
        }

        m.creature_in_field( critter );
        if( calendar::once_every( 1_days ) ) {

            if( critter.has_flag( MF_MILKABLE ) ) {
                critter.refill_udders();
            }
            critter.try_biosignature();
            critter.try_reproduce();

            // 首先判断怪物有没有受伤
            if (critter.get_hp() < critter.get_hp_max()) {
                
                // 丧尸与机器人之外的怪物每天自动回复生命值
                if ( !critter.in_species(species_ZOMBIE) && !critter.in_species(species_ROBOT)) {

                    // 每天自动回复怪物最大血量的百分之5的血量，至少回复1点
                    if ( (0.05 * critter.get_hp_max() + critter.get_hp()) > critter.get_hp_max() ) {
                        critter.set_hp(critter.get_hp_max());                   
                    }
                    else {                      
                        if (0.05 * critter.get_hp_max()<1) {
                        
                            critter.set_hp( 1 + critter.get_hp() );
                        
                        }
                        else {
                            critter.set_hp(0.05 * critter.get_hp_max() + critter.get_hp());
                        }
                    
                    
                    }

                }
          
            }

                        
           
            

        }
        
        


        while( critter.moves > 0 && !critter.is_dead() && !critter.has_effect( effect_ridden ) ) {
            critter.made_footstep = false;
            // Controlled critters don't make their own plans
            if( !critter.has_effect( effect_controlled ) ) {
                // Formulate a path to follow
                critter.plan();
            } else {
                critter.moves = 0;
                break;
            }
            critter.move(); // Move one square, possibly hit u
            critter.process_triggers();
            m.creature_in_field( critter );
        }

        if( !critter.is_dead() &&
            u.has_active_bionic( bio_alarm ) &&
            u.get_power_level() >= bio_alarm->power_trigger &&
            rl_dist( u.pos(), critter.pos() ) <= 5 &&
            !critter.is_hallucination() ) {
            u.mod_power_level( -bio_alarm->power_trigger );
            add_msg( m_warning, _( "Your motion alarm goes off!" ) );
            g->cancel_activity_or_ignore_query( distraction_type::motion_alarm,
                                                _( "Your motion alarm goes off!" ) );
            if( u.has_effect( effect_sleep ) ) {
                u.wake_up();
            }
        }
    }

    g->cleanup_dead();
   
    // The remaining monsters are all alive, but may be outside of the reality bubble.
    // If so, despawn them. This is not the same as dying, they will be stored for later and the
    // monster::die function is not called.
    for( monster &critter : g->all_monsters() ) {
        if( critter.posx() < 0 - MAPSIZE_X / 6 ||
            critter.posy() < 0 - MAPSIZE_Y / 6 ||
            critter.posx() > ( MAPSIZE_X * 7 ) / 6 ||
            critter.posy() > ( MAPSIZE_Y * 7 ) / 6 ) {
            g->despawn_monster( critter );
        }
    }

    // Now, do active NPCs.
    for( npc &guy : g->all_npcs() ) {
        int turns = 0;
        int real_count = 0;
        const int count_limit = std::max( 10, guy.moves / 64 );
        if( guy.is_mounted() ) {
            guy.check_mount_is_spooked();
        }
        m.creature_in_field( guy );
        if( !guy.has_effect( effect_npc_suspend ) ) {
            guy.process_turn();
        }
        while( !guy.is_dead() && ( !guy.in_sleep_state() || guy.activity.id() == ACT_OPERATION ) &&
               guy.moves > 0 && turns < 10 ) {
            const int moves = guy.moves;
            const bool has_destination = guy.has_destination_activity();
            guy.move();
            if( moves == guy.moves ) {
                // Count every time we exit npc::move() without spending any moves.
                real_count++;
                if( has_destination == guy.has_destination_activity() || real_count > count_limit ) {
                    turns++;
                }
            }
            // Turn on debug mode when in infinite loop
            // It has to be done before the last turn, otherwise
            // there will be no meaningful debug output.
            if( turns == 9 ) {
                debugmsg( "NPC %s entered infinite loop.  Turning on debug mode",
                          guy.get_name() );
                // 目前及时是进入无限循环也不要开启debug_mode
                //debug_mode = true;
                // make sure the filter is active
                if( std::find(
                        debugmode::enabled_filters.begin(), debugmode::enabled_filters.end(),
                        debugmode::DF_NPC ) == debugmode::enabled_filters.end() ) {
                    debugmode::enabled_filters.emplace_back( debugmode::DF_NPC );
                }
            }
        }

        // If we spun too long trying to decide what to do (without spending moves),
        // Invoke cognitive suspension to prevent an infinite loop.
        if( turns == 10 ) {
            add_msg( _( "%s faints!" ), guy.get_name() );
            guy.reboot();
        }

        if( !guy.is_dead() ) {
            guy.npc_update_body();
        } 

        // 这是用来检查从其他派系来的这个npc是不是还想要继续交易，不想的话，就离开玩家
        if ( guy.has_effect(effect_just_trade) && !guy.has_effect(effect_want_trade)) {
            
            guy.set_attitude(NPCATT_NULL);
            // 清空这个npc的任务
            guy.set_mission(NPC_MISSION_NULL);
            // 让这个npc离开玩家
            guy.add_effect(effect_npc_flee_player, 24_hours);

            int dx = guy.get_location().raw().x - u.get_location().raw().x;
            int dy = guy.get_location().raw().y - u.get_location().raw().y;
        // 这里取巧，仅仅看看这个npc与玩家的距离是不是大于了58，是的话就直接将其设置为幻觉，就此消失。 
            if (static_cast<int>(std::sqrt(dx*dx+dy*dy)) >58 ) {
                guy.hallucination = true;
            }
        }
        


    }
    g->cleanup_dead();


    // 派系的处理
    bool assassin_attack = false;
    if (calendar::once_every(1_days)) {

        for (auto& f : g->faction_manager_ptr->all()) {

            faction* f_ptr = g->faction_manager_ptr->get(f.first);
            // 每天十分之一的概率
            int chance = rng(1, 10);

            // 如果 距离下一次拿物资的日子为0，那么重置它为30
            if (f_ptr->conquer_degree == 0) {

                f_ptr->days_required_to_submit_resources = 30;

            }

            if (chance == 1) {
                // 征服度作为唯一的判断，因为征服度的上升仅仅只有威压的方式，大于0已经是威压成功了

                if (f_ptr->conquer_degree > 0) {
                    // 命中概率后，降低派系征服度，降低1   
                    f_ptr->conquer_degree--;
                }

            }

            if (f_ptr->conquer_degree > 0 && f_ptr->conquer_degree < 50) {
                // 此种情况下，产生刺客的概率为百分之20
                int chance_assassin_attack = rng(1, 5);
                if (chance_assassin_attack == 1) {
                    assassin_attack = true;
                }

            }


            // 距离下一次拿取物资的日子减少
            if (f_ptr->conquer_degree > 0) {

                f_ptr->days_required_to_submit_resources--;


            }


        }

        // 如果 assassin_attack 为 true，生成刺客
        if (assassin_attack == true) {

            make_assassins();
            make_assassins();
            make_assassins();
            make_assassins();
            make_assassins();
            make_assassins();
            make_assassins();
            make_assassins();
            make_assassins();

        }

    }


        if (get_option<bool>("其他派系的访问") == true) {

                if (calendar::once_every(907200_turns)) {

                    faction* free_merchants = g->faction_manager_ptr->get(faction_free_merchants);

                    // 好感度越高，来的交易员越多
                    int base_count = free_merchants->likes_u / 40;

                    if (free_merchants->known_by_u == true && free_merchants->likes_u >= 0) {

                    tripoint_abs_omt omt_pos = u.global_omt_location();
                    
                    cata::optional<basecamp*> camp = overmap_buffer.find_camp(omt_pos.xy());

                    // 如果玩家不处于营地的范围内，不要生成交易人员
                    if (camp) {
                        catacurses::window w_01;
                        input_context ctxt("");
                        ctxt.register_action("QUIT");

                        ui_adaptor ui_01;
                        ui_01.on_screen_resize([&](ui_adaptor&) {
                            const std::pair<point, point> beg_and_max = {
                            point(TERMX / 5, TERMY / 8),
                            point(TERMX / 2, TERMY / 2)
                            };
                            const point& beg = beg_and_max.first;
                            const point& max = beg_and_max.second;
                            w_01 = catacurses::newwin(10, 40, beg);
                            ui_01.position_from_window(w_01);
                            });


                        ui_01.mark_resize();


                        ui_01.on_redraw([&](const ui_adaptor&) {
                            werase(w_01);
                            draw_border(w_01);
                            fold_and_print(w_01, point(1, 1), getmaxx(w_01) - 1 - 1, c_white,
                                "自由商会派人来访问了");
                            wnoutrefresh(w_01);
                            });


                        while (true) {
                            ui_01.invalidate_ui();
                            ui_manager::redraw_invalidated();
                            const std::string action = ctxt.handle_input();
                            if (action == "QUIT") {
                                break;
                            }


                        }
                        add_msg(m_good, _("自由商会派人来访问了。"));

                        for (int i = 0; i < base_count + 1; i++) {

                            make_trade_caravan();

                        }


                    }
                        
                                              
                    }


                    
                }

            
        }

    




}

void overmap_npc_move()
{
    avatar &u = get_avatar();
    std::vector<npc *> travelling_npcs;
    static constexpr int move_search_radius = 600;
    for( auto &elem : overmap_buffer.get_npcs_near_player( move_search_radius ) ) {
        if( !elem ) {
            continue;
        }
        npc *npc_to_add = elem.get();
        if( ( !npc_to_add->is_active() || rl_dist( u.pos(), npc_to_add->pos() ) > SEEX * 2 ) &&
            npc_to_add->mission == NPC_MISSION_TRAVELLING ) {
            travelling_npcs.push_back( npc_to_add );
        }
    }
    bool npcs_need_reload = false;
    for( npc *&elem : travelling_npcs ) {
        if( elem->has_omt_destination() ) {
            if( !elem->omt_path.empty() ) {
                if( rl_dist( elem->omt_path.back(), elem->global_omt_location() ) > 2 ) {
                    // recalculate path, we got distracted doing something else probably
                    elem->omt_path.clear();
                } else if( elem->omt_path.back() == elem->global_omt_location() ) {
                    elem->omt_path.pop_back();
                }
            }
            if( elem->omt_path.empty() ) {
                elem->omt_path = overmap_buffer.get_travel_path( elem->global_omt_location(), elem->goal,
                                 overmap_path_params::for_npc() );
                if( elem->omt_path.empty() ) { // goal is unreachable, or already reached goal, reset it
                    elem->goal = npc::no_goal_point;
                }
            } else {
                elem->travel_overmap( elem->omt_path.back() );
                npcs_need_reload = true;
            }
        }
        if( !elem->has_omt_destination() && calendar::once_every( 1_hours ) && one_in( 3 ) ) {
            // travelling destination is reached/not set, try different one
            elem->set_omt_destination();
        }
    }
    if( npcs_need_reload ) {
        g->reload_npcs();
    }
}

} // namespace

// MAIN GAME LOOP
// Returns true if game is over (death, saved, quit, etc)
bool do_turn()
{
    if( g->is_game_over() ) {
        return turn_handler::cleanup_at_end();
    }

    weather_manager &weather = get_weather();
    // Actual stuff
    if( g->new_game ) {
        g->new_game = false;
        if( get_option<std::string>( "ETERNAL_WEATHER" ) != "normal" ) {
            weather.weather_override = static_cast<weather_type_id>
                                       ( get_option<std::string>( "ETERNAL_WEATHER" ) );
            weather.set_nextweather( calendar::turn );
        } else {
            weather.weather_override = WEATHER_NULL;
            weather.set_nextweather( calendar::turn );
        }
    } else {
        g->gamemode->per_turn();
        calendar::turn += 1_turns;
    }

    play_music( music::get_music_id_string() );

    // starting a new turn, clear out temperature cache
    weather.temperature_cache.clear();

    if( g->npcs_dirty ) {
        g->load_npcs();
    }

    timed_event_manager &timed_events = get_timed_events();
    timed_events.process();
    mission::process_all();
    avatar &u = get_avatar();
    map &m = get_map();
    // If controlling a vehicle that is owned by someone else
    if( u.in_vehicle && u.controlling_vehicle ) {
        vehicle *veh = veh_pointer_or_null( m.veh_at( u.pos() ) );
        if( veh && !veh->handle_potential_theft( dynamic_cast<Character &>( u ), true ) ) {
            veh->handle_potential_theft( dynamic_cast<Character &>( u ), false, false );
        }
    }

    // Make sure players cant defy gravity by standing still, Looney tunes style.
    u.gravity_check();

    // If riding a horse - chance to spook
    if( u.is_mounted() ) {
        u.check_mount_is_spooked();
    }
    if( calendar::once_every( 1_days ) ) {
        overmap_buffer.process_mongroups();
    }

    // Move hordes every 2.5 min
    if( calendar::once_every( time_duration::from_minutes( 2.5 ) ) ) {
        overmap_buffer.move_hordes();
        if( u.has_trait( trait_HAS_NEMESIS ) ) {
            overmap_buffer.move_nemesis();
        }
        // Hordes that reached the reality bubble need to spawn,
        // make them spawn in invisible areas only.
        m.spawn_monsters( false );
    }

    g->debug_hour_timer.print_time();

    u.update_body();

    // Auto-save if autosave is enabled
    if( get_option<bool>( "AUTOSAVE" ) &&
        calendar::once_every( 1_turns * get_option<int>( "AUTOSAVE_TURNS" ) ) &&
        !u.is_dead_state() ) {
        g->autosave();
    }

    weather.update_weather();
    g->reset_light_level();

    g->perhaps_add_random_npc( /* ignore_spawn_timers_and_rates = */ false );
    while( u.moves > 0 && u.activity ) {
        u.activity.do_turn( u );
    }
    // FIXME: hack needed due to the legacy code in advanced_inventory::move_all_items()
    if( !u.activity ) {
        kill_advanced_inv();
    }

    // Process NPC sound events before they move or they hear themselves talking
    for( npc &guy : g->all_npcs() ) {
        if( rl_dist( guy.pos(), u.pos() ) < MAX_VIEW_DISTANCE ) {
            sounds::process_sound_markers( &guy );
        }
    }

    music::deactivate_music_id( music::music_id::sound );

    // Process sound events into sound markers for display to the player.
    sounds::process_sound_markers( &u );

    if( u.is_deaf() ) {
        sfx::do_hearing_loss();
    }

    if( !u.has_effect( effect_sleep ) || g->uquit == QUIT_WATCH ) {
        if( u.moves > 0 || g->uquit == QUIT_WATCH ) {
            while( u.moves > 0 || g->uquit == QUIT_WATCH ) {
                g->cleanup_dead();
                g->mon_info_update();
                // Process any new sounds the player caused during their turn.
                for( npc &guy : g->all_npcs() ) {
                    if( rl_dist( guy.pos(), u.pos() ) < MAX_VIEW_DISTANCE ) {
                        sounds::process_sound_markers( &guy );
                    }
                }
                explosion_handler::process_explosions();
                sounds::process_sound_markers( &u );
                if( !u.activity && g->uquit != QUIT_WATCH
                    && ( !u.has_distant_destination() || calendar::once_every( 10_seconds ) ) ) {
                    g->wait_popup.reset();
                    ui_manager::redraw();
                }

                if( g->queue_screenshot ) {
                    g->take_screenshot();
                    g->queue_screenshot = false;
                }

                if( g->handle_action() ) {
                    ++g->moves_since_last_save;
                    u.action_taken();
                }

                if( g->is_game_over() ) {
                    return turn_handler::cleanup_at_end();
                }

                if( g->uquit == QUIT_WATCH ) {
                    break;
                }
                while( u.moves > 0 && u.activity ) {
                    u.activity.do_turn( u );
                }
            }
            // Reset displayed sound markers now that the turn is over.
            // We only want this to happen if the player had a chance to examine the sounds.
            sounds::reset_markers();
        } else {
            // Rate limit key polling to 10 times a second.
            static auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now() );
            const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now() );
            if( ( now - start ).count() > 100 ) {
                handle_key_blocking_activity();
                start = now;
            }

            g->mon_info_update();

            // If player is performing a task, a monster is dangerously close,
            // and monster can reach to the player or it has some sort of a ranged attack,
            // warn them regardless of previous safemode warnings
            if( u.activity ) {
                for( std::pair<const distraction_type, std::string> &dist : u.activity.get_distractions() ) {
                    if( g->cancel_activity_or_ignore_query( dist.first, dist.second ) ) {
                        break;
                    }
                }
            }
        }
    }

    if( g->driving_view_offset.x != 0 || g->driving_view_offset.y != 0 ) {
        // Still have a view offset, but might not be driving anymore,
        // or the option has been deactivated,
        // might also happen when someone dives from a moving car.
        // or when using the handbrake.
        vehicle *veh = veh_pointer_or_null( m.veh_at( u.pos() ) );
        g->calc_driving_offset( veh );
    }

    scent_map &scent = get_scent();
    // No-scent debug mutation has to be processed here or else it takes time to start working
    if( !u.has_flag( STATIC( json_character_flag( "NO_SCENT" ) ) ) ) {
        scent.set( u.pos(), u.scent, u.get_type_of_scent() );
        overmap_buffer.set_scent( u.global_omt_location(),  u.scent );
    }
    scent.update( u.pos(), m );

    // We need floor cache before checking falling 'n stuff
    m.build_floor_caches();

    m.process_falling();
    m.vehmove();
    m.process_fields();
    m.process_items();
    explosion_handler::process_explosions();
    m.creature_in_field( u );

    // Apply sounds from previous turn to monster and NPC AI.
    sounds::process_sounds();
    const int levz = m.get_abs_sub().z();
    // Update vision caches for monsters. If this turns out to be expensive,
    // consider a stripped down cache just for monsters.
    m.build_map_cache( levz, true );
    monmove();
    if( calendar::once_every( 5_minutes ) ) {
        overmap_npc_move();
    }
    if( calendar::once_every( 10_seconds ) ) {
        for( const tripoint &elem : m.get_furn_field_locations() ) {
            const furn_t &furn = *m.furn( elem );
            for( const emit_id &e : furn.emissions ) {
                m.emit_field( elem, e );
            }
        }
        for( const tripoint &elem : m.get_ter_field_locations() ) {
            const ter_t &ter = *m.ter( elem );
            for( const emit_id &e : ter.emissions ) {
                m.emit_field( elem, e );
            }
        }
    }
    g->mon_info_update();
    u.process_turn();
    if( u.moves < 0 && get_option<bool>( "FORCE_REDRAW" ) ) {
        ui_manager::redraw();
        refresh_display();
    }

    if( levz >= 0 && !u.is_underwater() ) {
        handle_weather_effects( weather.weather_id );
    }

    const bool player_is_sleeping = u.has_effect( effect_sleep );
    bool wait_redraw = false;
    std::string wait_message;
    time_duration wait_refresh_rate;
    if( player_is_sleeping ) {
        wait_redraw = true;
        wait_message = _( "Wait till you wake up…" );
        wait_refresh_rate = 30_minutes;
    } else if( const cata::optional<std::string> progress = u.activity.get_progress_message( u ) ) {
        wait_redraw = true;
        wait_message = *progress;
        if( u.activity.is_interruptible() && u.activity.interruptable_with_kb ) {
            wait_message += string_format( _( "\n%s to interrupt" ), press_x( ACTION_PAUSE ) );
        }
        if( u.activity.id() == ACT_AUTODRIVE ) {
            wait_refresh_rate = 1_turns;
        } else if( u.activity.id() == ACT_FIRSTAID ) {
            wait_refresh_rate = 5_turns;
        } else {
            wait_refresh_rate = 5_minutes;
        }
    }
    if( wait_redraw ) {
        if( g->first_redraw_since_waiting_started ||
            calendar::once_every( std::min( 1_minutes, wait_refresh_rate ) ) ) {
            if( g->first_redraw_since_waiting_started || calendar::once_every( wait_refresh_rate ) ) {
                ui_manager::redraw();
            }

            // Avoid redrawing the main UI every time due to invalidation
            ui_adaptor dummy( ui_adaptor::disable_uis_below {} );
            g->wait_popup = std::make_unique<static_popup>();
            g->wait_popup->on_top( true ).wait_message( "%s", wait_message );
            ui_manager::redraw();
            refresh_display();
            g->first_redraw_since_waiting_started = false;
        }
    } else {
        // Nothing to wait for now
        g->wait_popup.reset();
        g->first_redraw_since_waiting_started = true;
    }

    u.update_bodytemp();
    u.update_body_wetness( *weather.weather_precise );
    u.apply_wetness_morale( weather.temperature );

    if( calendar::once_every( 1_minutes ) ) {
        u.update_morale();
    }

    if( calendar::once_every( 9_turns ) ) {
        u.check_and_recover_morale();
    }

    if( !u.is_deaf() ) {
        sfx::remove_hearing_loss();
    }
    sfx::do_danger_music();
    sfx::do_vehicle_engine_sfx();
    sfx::do_vehicle_exterior_engine_sfx();
    sfx::do_fatigue();

    // reset player noise
    u.volume = 0;

    // Calculate bionic power balance
    u.power_balance = u.get_power_level() - u.power_prev_turn;
    u.power_prev_turn = u.get_power_level();

    return false;
}
