//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2005 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2007 Eduardo Hernandez Munoz
//  Copyright (C) 2008      Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


//The AI debugging works best with just 1 AI kart, so set the number of karts
//to 2 in main.cpp with quickstart and run supertuxkart with the arg -N.
#undef AI_DEBUG

#include "karts/controller/default_ai_controller.hpp"

#ifdef AI_DEBUG
#  include "irrlicht.h"
   using namespace irr;
#endif

#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <iostream>

#ifdef AI_DEBUG
#include "graphics/irr_driver.hpp"
#endif
#include "modes/linear_world.hpp"
#include "network/network_manager.hpp"
#include "race/race_manager.hpp"
#include "tracks/quad_graph.hpp"
#include "tracks/track.hpp"
#include "utils/constants.hpp"

DefaultAIController::DefaultAIController(Kart *kart) : Controller(kart)
{
    m_kart        = kart;
    m_kart_length = m_kart->getKartProperties()->getKartModel()->getLength();
    m_kart_width  = m_kart->getKartProperties()->getKartModel()->getWidth();
    m_world       = dynamic_cast<LinearWorld*>(World::getWorld());    
    m_track       = m_world->getTrack();
    m_quad_graph  = &m_track->getQuadGraph();
    m_next_node_index.reserve(m_quad_graph->getNumNodes());
    m_successor_index.reserve(m_quad_graph->getNumNodes());

    // Initialise the fields with -1
    for(unsigned int i=0; i<m_quad_graph->getNumNodes(); i++)
    {
        m_next_node_index.push_back(-1);
        m_successor_index.push_back(-1);
    }
    // For now pick one part on random, which is not adjusted during the run
    std::vector<unsigned int> next;
    int count=0;
    int current_node=0;
    while (1)
    {
        next.clear();
        m_quad_graph->getSuccessors(current_node, next);
        int indx = rand() % next.size();
        m_successor_index[current_node] = indx;
        m_next_node_index[current_node] = next[indx];
        current_node = next[indx];
        if(current_node==0) break;
        count++;
        if(count>(int)m_quad_graph->getNumNodes())
        {
            fprintf(stderr, "AI can't find a loop going back to node 0, aborting.\n");
            exit(-1);
        }
    };

    const unsigned int look_ahead=10;
    // Now compute for each node in the graph the list of the next 'look_ahead'
    // graph nodes. This is the list of node that is tested in checkCrashes.
    // If the look_ahead is too big, the AI can skip loops (see 
    // QuadGraph::findRoadSector for details), if it's too short the AI won't
    // find too good a driveline. Note that in general this list should
    // be computed recursively, but since the AI for now is using only 
    // (randomly picked) path this is fine
    m_all_look_aheads.reserve(m_quad_graph->getNumNodes());
    for(unsigned int i=0; i<m_quad_graph->getNumNodes(); i++)
    {
        std::vector<int> l;
        int current = i;
        for(unsigned int j=0; j<look_ahead; j++)
        {
            int next = m_next_node_index[current];
            if(next==-1) break;
            l.push_back(m_next_node_index[current]);
            current = m_next_node_index[current];
        }   // for j<look_ahead
        m_all_look_aheads.push_back(l);
    }
    // Reset must be called after m_quad_graph etc. is set up        
    reset();

    switch( race_manager->getDifficulty())
    {
    case RaceManager::RD_EASY:
        m_wait_for_players   = true;
        m_max_handicap_accel = 0.9f;
        m_fallback_tactic    = FT_AVOID_TRACK_CRASH;
        m_item_tactic        = IT_TEN_SECONDS;
        m_max_start_delay    = 0.5f;
        m_min_steps          = 0;
        m_skidding_threshold = 4.0f;
        m_nitro_level        = NITRO_NONE;
        m_handle_bomb        = false;
        break;
    case RaceManager::RD_MEDIUM:
        m_wait_for_players   = true;
        m_max_handicap_accel = 0.95f;
        // FT_PARALLEL had problems on some tracks when suddenly a smaller 
        // section occurred (e.g. bridge in stone track): the AI would drive
        // over and over into the river
        m_fallback_tactic    = FT_FAREST_POINT;
        m_item_tactic        = IT_CALCULATE;
        m_max_start_delay    = 0.4f;
        m_min_steps          = 1;
        m_skidding_threshold = 2.0f;
        m_nitro_level        = NITRO_SOME;
        m_handle_bomb        = true;
        break;
    case RaceManager::RD_HARD:
        m_wait_for_players   = false;
        m_max_handicap_accel = 1.0f;
        m_fallback_tactic    = FT_FAREST_POINT;
        m_item_tactic        = IT_CALCULATE;
        m_max_start_delay    = 0.1f;
        m_min_steps          = 2;
        m_skidding_threshold = 1.3f;
        m_nitro_level        = NITRO_ALL;
        m_handle_bomb        = true;
        break;
    }

#ifdef AI_DEBUG
    m_debug_sphere = irr_driver->getSceneManager()->addSphereSceneNode(1);
#endif
}   // DefaultAIController

//-----------------------------------------------------------------------------
/** The destructor deletes the shared TrackInfo objects if no more DefaultAIController
 *  instances are around.
 */
DefaultAIController::~DefaultAIController()
{
#ifdef AI_DEBUG
    irr_driver->removeNode(m_debug_sphere);
#endif
}   // ~DefaultAIController

//-----------------------------------------------------------------------------
const irr::core::stringw& DefaultAIController::getNamePostfix() const 
{
    // Static to avoid returning the address of a temporary stringq
    static irr::core::stringw name="(default)";
    return name;
}   // getNamePostfix

//-----------------------------------------------------------------------------
//TODO: if the AI is crashing constantly, make it move backwards in a straight
//line, then move forward while turning.
void DefaultAIController::update(float dt)
{
    // This is used to enable firing an item backwards.
    m_controls->m_look_back = false;
    m_controls->m_nitro     = false;

    // Update the current node:
    if(m_track_node!=QuadGraph::UNKNOWN_SECTOR)
    {
        int old_node = m_track_node;
        m_quad_graph->findRoadSector(m_kart->getXYZ(), &m_track_node, 
                                     &m_all_look_aheads[m_track_node]);
        // IF the AI is off track (or on a branch of the track it did not
        // select to be on), keep the old position.
        if(m_track_node==QuadGraph::UNKNOWN_SECTOR ||
            m_next_node_index[m_track_node]==-1)
            m_track_node = old_node;
    }
    if(m_track_node==QuadGraph::UNKNOWN_SECTOR)
    {
        m_track_node = m_quad_graph->findOutOfRoadSector(m_kart->getXYZ());
    }
    // The client does not do any AI computations.
    if(network_manager->getMode()==NetworkManager::NW_CLIENT) 
    {
        Controller::update(dt);
        return;
    }

    if( m_world->isStartPhase() )
    {
        handleRaceStart();
        Controller::update(dt);
        return;
    }

    /*Get information that is needed by more than 1 of the handling funcs*/
    //Detect if we are going to crash with the track and/or kart
    int steps = 0;

    steps = calcSteps();

    computeNearestKarts();
    checkCrashes( steps, m_kart->getXYZ() );
    findCurve();

    // Special behaviour if we have a bomb attach: try to hit the kart ahead 
    // of us.
    bool commands_set = false;
    if(m_handle_bomb && m_kart->getAttachment()->getType()==ATTACH_BOMB && 
        m_kart_ahead )
    {
        // Use nitro if the kart is far ahead, or faster than this kart
        m_controls->m_nitro = m_distance_ahead>10.0f || 
                             m_kart_ahead->getSpeed() > m_kart->getSpeed();
        // If we are close enough, try to hit this kart
        if(m_distance_ahead<=10)
        {
            Vec3 target = m_kart_ahead->getXYZ();

            // If we are faster, try to predict the point where we will hit
            // the other kart
            if(m_kart_ahead->getSpeed() < m_kart->getSpeed())
            {
                float time_till_hit = m_distance_ahead
                                    / (m_kart->getSpeed()-m_kart_ahead->getSpeed());
                target += m_kart_ahead->getVelocity()*time_till_hit;
            }
            float steer_angle = steerToPoint(m_kart_ahead->getXYZ(), 
                                             dt);
            setSteering(steer_angle, dt);
            commands_set = true;
        }
        handleRescue(dt);
    }
    if(!commands_set)
    {
        /*Response handling functions*/
        handleAcceleration(dt);
        handleSteering(dt);
        handleItems(dt, steps);
        handleRescue(dt);
        handleBraking();
        // If a bomb is attached, nitro might already be set.
        if(!m_controls->m_nitro)
            handleNitroAndZipper();
    }
    // If we are supposed to use nitro, but have a zipper, 
    // use the zipper instead
    if(m_controls->m_nitro && m_kart->getPowerup()->getType()==POWERUP_ZIPPER && 
        m_kart->getSpeed()>1.0f && m_kart->getZipperTimeLeft()<=0)
    {
        // Make sure that not all AI karts use the zipper at the same
        // time in time trial at start up, so during the first 5 seconds
        // this is done at random only.
        if(race_manager->getMinorMode()!=RaceManager::MINOR_MODE_TIME_TRIAL ||
            (m_world->getTime()<3.0f && rand()%50==1) )
        {
            m_controls->m_nitro = false;
            m_controls->m_fire  = true;
        }
    }

    /*And obviously general kart stuff*/
    Controller::update(dt);
    m_collided = false;
}   // update

//-----------------------------------------------------------------------------
void DefaultAIController::handleBraking()
{
    // In follow the leader mode, the kart should brake if they are ahead of
    // the leader (and not the leader, i.e. don't have initial position 1)
    if(race_manager->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER &&
        m_kart->getPosition() < m_world->getKart(0)->getPosition()           &&
        m_kart->getInitialPosition()>1                                         )
    {
        m_controls->m_brake = true;
        return;
    }
        
    const float MIN_SPEED = 5.0f;
    //We may brake if we are about to get out of the road, but only if the
    //kart is on top of the road, and if we won't slow down below a certain
    //limit.
    if (m_crashes.m_road && m_kart->getVelocityLC().getZ() > MIN_SPEED && 
        m_world->isOnRoad(m_kart->getWorldKartId()) )
    {
        float kart_ang_diff = 
            m_quad_graph->getAngleToNext(m_track_node,
                                         m_successor_index[m_track_node])
          - m_kart->getHeading();
        kart_ang_diff = normalizeAngle(kart_ang_diff);
        kart_ang_diff = fabsf(kart_ang_diff);

        const float MIN_TRACK_ANGLE = DEGREE_TO_RAD*20.0f;
        const float CURVE_INSIDE_PERC = 0.25f;

        //Brake only if the road does not goes somewhat straight.
        if(m_curve_angle > MIN_TRACK_ANGLE) //Next curve is left
        {
            //Avoid braking if the kart is in the inside of the curve, but
            //if the curve angle is bigger than what the kart can steer, brake
            //even if we are in the inside, because the kart would be 'thrown'
            //out of the curve.
            if(!(m_world->getDistanceToCenterForKart(m_kart->getWorldKartId()) 
                 > m_quad_graph->getNode(m_track_node).getPathWidth() *
                 -CURVE_INSIDE_PERC || m_curve_angle > RAD_TO_DEGREE*m_kart->getMaxSteerAngle()))
            {
                m_controls->m_brake = false;
                return;
            }
        }
        else if( m_curve_angle < -MIN_TRACK_ANGLE ) //Next curve is right
        {
            if(!(m_world->getDistanceToCenterForKart( m_kart->getWorldKartId() ) 
                < m_quad_graph->getNode(m_track_node).getPathWidth() *
                 CURVE_INSIDE_PERC || m_curve_angle < -RAD_TO_DEGREE*m_kart->getMaxSteerAngle()))
            {
                m_controls->m_brake = false;
                return;
            }
        }

        //Brake if the kart's speed is bigger than the speed we need
        //to go through the curve at the widest angle, or if the kart
        //is not going straight in relation to the road.
        if(m_kart->getVelocityLC().getZ() > m_curve_target_speed ||
           kart_ang_diff          > MIN_TRACK_ANGLE         )
        {
#ifdef AI_DEBUG
        std::cout << "BRAKING" << std::endl;
#endif
            m_controls->m_brake = true;
            return;
        }

    }

    m_controls->m_brake = false;
}   // handleBraking

//-----------------------------------------------------------------------------
void DefaultAIController::handleSteering(float dt)
{
    const int next = m_next_node_index[m_track_node];
    
    float steer_angle = 0.0f;

    /*The AI responds based on the information we just gathered, using a
     *finite state machine.
     */
    //Reaction to being outside of the road
    if( fabsf(m_world->getDistanceToCenterForKart( m_kart->getWorldKartId() ))  >
       0.5f* m_quad_graph->getNode(m_track_node).getPathWidth()+0.5f )
    {
        steer_angle = steerToPoint(m_quad_graph->getQuad(next).getCenter(), 
                                   dt );

#ifdef AI_DEBUG
        m_debug_sphere->setPosition(m_quad_graph->getQuad(next).getCenter().toIrrVector());
        std::cout << "- Outside of road: steer to center point." <<
            std::endl;
#endif
    }
    //If we are going to crash against a kart, avoid it if it doesn't
    //drives the kart out of the road
    else if( m_crashes.m_kart != -1 && !m_crashes.m_road )
    {
        //-1 = left, 1 = right, 0 = no crash.
        if( m_start_kart_crash_direction == 1 )
        {
            steer_angle = steerToAngle(next, -M_PI*0.5f );
            m_start_kart_crash_direction = 0;
        }
        else if(m_start_kart_crash_direction == -1)
        {
            steer_angle = steerToAngle(next, M_PI*0.5f);
            m_start_kart_crash_direction = 0;
        }
        else
        {
            if(m_world->getDistanceToCenterForKart( m_kart->getWorldKartId() ) >
               m_world->getDistanceToCenterForKart( m_crashes.m_kart ))
            {
                steer_angle = steerToAngle(next, -M_PI*0.5f );
                m_start_kart_crash_direction = 1;
            }
            else
            {
                steer_angle = steerToAngle(next, M_PI*0.5f );
                m_start_kart_crash_direction = -1;
            }
        }

#ifdef AI_DEBUG
        std::cout << "- Velocity vector crashes with kart and doesn't " <<
            "crashes with road : steer 90 degrees away from kart." <<
            std::endl;
#endif

    }
    else
    {
        m_start_kart_crash_direction = 0;
        switch( m_fallback_tactic )
        {
        case FT_FAREST_POINT:
            {
                Vec3 straight_point;
                findNonCrashingPoint(&straight_point);
#ifdef AI_DEBUG
                m_debug_sphere->setPosition(straight_point.toIrrVector());
#endif
                steer_angle = steerToPoint(straight_point, dt);
            }
            break;

        case FT_PARALLEL:
            steer_angle = steerToAngle(next, 0.0f );
            break;

        case FT_AVOID_TRACK_CRASH:
            if( m_crashes.m_road )
            {
                steer_angle = steerToAngle(m_track_node, 0.0f );
            }
            else steer_angle = 0.0f;

            break;
        }

#ifdef AI_DEBUG
        std::cout << "- Fallback."  << std::endl;
#endif

    }
    // avoid steer vibrations
    //if (fabsf(steer_angle) < 1.0f*3.1415/180.0f)
    //    steer_angle = 0.f;

    setSteering(steer_angle, dt);
}   // handleSteering

//-----------------------------------------------------------------------------
void DefaultAIController::handleItems( const float DELTA, const int STEPS )
{
    m_controls->m_fire = false;
    if(m_kart->isRescue() || m_kart->getPowerup()->getType() == POWERUP_NOTHING ) return;

    m_time_since_last_shot += DELTA;

    // Tactic 1: wait ten seconds, then use item
    // -----------------------------------------
    if(m_item_tactic==IT_TEN_SECONDS)
    {
        if( m_time_since_last_shot > 10.0f )
        {
            m_controls->m_fire = true;
            m_time_since_last_shot = 0.0f;
        }
        return;
    }

    // Tactic 2: calculate
    // -------------------
    switch( m_kart->getPowerup()->getType() )
    {
    case POWERUP_ZIPPER:
        // Do nothing. Further up a zipper is used if nitro should be selected,
        // saving the (potential more valuable nitro) for later
        break;

    case POWERUP_BUBBLEGUM:
        // Either use the bubble gum after 10 seconds, or if the next kart 
        // behind is 'close' but not too close (too close likely means that the
        // kart is not behind but more to the side of this kart and so won't 
        // be hit by the bubble gum anyway). Should we check the speed of the
        // kart as well? I.e. only drop if the kart behind is faster? Otoh 
        // this approach helps preventing an overtaken kart to overtake us 
        // again.
        m_controls->m_fire = (m_distance_behind < 15.0f &&
                               m_distance_behind > 3.0f   ) || 
                            m_time_since_last_shot>10.0f;
        if(m_distance_behind < 10.0f && m_distance_behind > 2.0f   )
            m_distance_behind *= 1.0f;
        break;
    // All the thrown/fired items might be improved by considering the angle
    // towards m_kart_ahead. And some of them can fire backwards, too - which
    // isn't yet supported for AI karts.
    case POWERUP_CAKE:
        m_controls->m_fire = (m_kart_ahead && m_distance_ahead < 20.0f) ||
                             m_time_since_last_shot > 10.0f;
        break;
    case POWERUP_BOWLING:
        {
            // Bowling balls slower, so only fire on closer karts - but when
            // firing backwards, the kart can be further away, since the ball
            // acts a bit like a mine (and the kart is racing towards it, too)
            bool fire_backwards = (m_kart_behind && m_kart_ahead && 
                                   m_distance_behind < m_distance_ahead) ||
                                  !m_kart_ahead;
            float distance = fire_backwards ? m_distance_behind 
                                            : m_distance_ahead;
            m_controls->m_fire = (fire_backwards && distance < 30.0f)  || 
                                (!fire_backwards && distance <10.0f)  ||
                                m_time_since_last_shot > 10.0f;
            if(m_controls->m_fire)
                m_controls->m_look_back = fire_backwards;
            break;
        }
    case POWERUP_PLUNGER:
        {
            // Plungers can be fired backwards and are faster,
            // so allow more distance for shooting.
            bool fire_backwards = (m_kart_behind && m_kart_ahead && 
                                   m_distance_behind < m_distance_ahead) ||
                                  !m_kart_ahead;
            float distance = fire_backwards ? m_distance_behind 
                                            : m_distance_ahead;
            m_controls->m_fire = distance < 30.0f                 || 
                                m_time_since_last_shot > 10.0f;
            if(m_controls->m_fire)
                m_controls->m_look_back = fire_backwards;
            break;
        }
    case POWERUP_ANVIL:
        if(race_manager->getMinorMode()==RaceManager::MINOR_MODE_FOLLOW_LEADER)
        {
            m_controls->m_fire = m_world->getTime()<1.0f && 
                                 m_kart->getPosition()>2;
        }
        else
        {
            m_controls->m_fire = m_time_since_last_shot > 3.0f && 
                                 m_kart->getPosition()>1;
        }
    default:
        m_controls->m_fire = true;
    }
    if(m_controls->m_fire)  m_time_since_last_shot = 0.0f;
    return;
}   // handleItems

//-----------------------------------------------------------------------------
/** Determines the closest karts just behind and in front of this kart. The
 *  'closeness' is for now simply based on the position, i.e. if a kart is
 *  more than one lap behind or ahead, it is not considered to be closest.
 */
void DefaultAIController::computeNearestKarts()
{
    bool need_to_check = false;
    int my_position    = m_kart->getPosition();
    // See if the kart ahead has changed:
    if( ( m_kart_ahead && m_kart_ahead->getPosition()+1!=my_position ) ||
        (!m_kart_ahead && my_position>1                              )    )
       need_to_check = true;
    // See if the kart behind has changed:
    if( ( m_kart_behind && m_kart_behind->getPosition()-1!=my_position   ) ||
        (!m_kart_behind && my_position<(int)m_world->getCurrentNumKarts())    )
        need_to_check = true;
    if(!need_to_check) return;

    m_kart_behind    = m_kart_ahead      = NULL;
    m_distance_ahead = m_distance_behind = 9999999.9f;
    float my_dist = m_world->getDistanceDownTrackForKart(m_kart->getWorldKartId());
    for(unsigned int i=0; i<m_world->getNumKarts(); i++)
    {
        Kart *k = m_world->getKart(i);
        if(k->isEliminated() || k==m_kart) continue;
        if(k->getPosition()==my_position+1) 
        {
            m_kart_behind = k;
            m_distance_behind = my_dist - m_world->getDistanceDownTrackForKart(i);
            if(m_distance_behind<0.0f)
                m_distance_behind += m_track->getTrackLength();
        }
        else 
            if(k->getPosition()==my_position-1)
            {
                m_kart_ahead = k;
                m_distance_ahead = m_world->getDistanceDownTrackForKart(i) - my_dist;
                if(m_distance_ahead<0.0f)
                    m_distance_ahead += m_track->getTrackLength();
            }
    }   // for i<world->getNumKarts()
}   // computeNearestKarts

//-----------------------------------------------------------------------------
void DefaultAIController::handleAcceleration( const float DELTA )
{
    //Do not accelerate until we have delayed the start enough
    if( m_time_till_start > 0.0f )
    {
        m_time_till_start -= DELTA;
        m_controls->m_accel = 0.0f;
        return;
    }

    if( m_controls->m_brake == true )
    {
        m_controls->m_accel = 0.0f;
        return;
    }

    if(m_kart->hasViewBlockedByPlunger())
    {
        if(!(m_kart->getSpeed() > m_kart->getMaxSpeedOnTerrain() / 2))
            m_controls->m_accel = 0.05f;
        else 
            m_controls->m_accel = 0.0f;
        return;
    }
    
    if( m_wait_for_players )
    {
        //Find if any player is ahead of this kart
        bool player_winning = false;
        for(unsigned int i = 0; i < race_manager->getNumPlayers(); ++i )
            if( m_kart->getPosition() > m_world->getPlayerKart(i)->getPosition() )
            {
                player_winning = true;
                break;
            }

        if( player_winning )
        {
            m_controls->m_accel = m_max_handicap_accel;
            return;
        }
    }

    m_controls->m_accel = 1.0f;
}   // handleAcceleration

//-----------------------------------------------------------------------------
void DefaultAIController::handleRaceStart()
{
    //FIXME: make karts able to get a penalty for accelerating too soon
    //like players, should happen to about 20% of the karts in easy,
    //5% in medium and less than 1% of the karts in hard.
    if( m_time_till_start <  0.0f )
    {
        srand(( unsigned ) time( 0 ));

        //Each kart starts at a different, random time, and the time is
        //smaller depending on the difficulty.
        m_time_till_start = ( float ) rand() / RAND_MAX * m_max_start_delay;
    }
}   // handleRaceStart

//-----------------------------------------------------------------------------
void DefaultAIController::handleRescue(const float DELTA)
{
    // check if kart is stuck
    if(m_kart->getSpeed()<2.0f && !m_kart->isRescue() && !m_world->isStartPhase())
    {
        m_time_since_stuck += DELTA;
        if(m_time_since_stuck > 2.0f)
        {
            m_kart->forceRescue();
            m_time_since_stuck=0.0f;
        }   // m_time_since_stuck > 2.0f
    }
    else
    {
        m_time_since_stuck = 0.0f;
    }
}   // handleRescue

//-----------------------------------------------------------------------------
/** Decides wether to use nitro or not.
 */
void DefaultAIController::handleNitroAndZipper()
{
    m_controls->m_nitro = false;
    // If we are already very fast, save nitro.
    if(m_kart->getSpeed() > 0.95f*m_kart->getMaxSpeedOnTerrain())
        return;
    // Don't use nitro when the AI has a plunger in the face!
    if(m_kart->hasViewBlockedByPlunger()) return;
    
    // Don't use nitro if the kart doesn't have any or is not on ground.
    if(!m_kart->isOnGround() || m_kart->hasFinishedRace()) return;
    
    // Don't compute nitro usage if we don't have nitro or are not supposed
    // to use it, and we don't have a zipper or are not supposed to use
    // it (calculated).
    if( (m_kart->getEnergy()==0                          || m_nitro_level==NITRO_NONE)  &&
        (m_kart->getPowerup()->getType()!=POWERUP_ZIPPER || m_item_tactic==IT_TEN_SECONDS) )
        return;

    // If a parachute or anvil is attached, the nitro doesn't give much
    // benefit. Better wait till later.
    const bool has_slowdown_attachment = 
                    m_kart->getAttachment()->getType()==ATTACH_PARACHUTE ||
                    m_kart->getAttachment()->getType()==ATTACH_ANVIL;
    if(has_slowdown_attachment) return;

    // If the kart is very slow (e.g. after rescue), use nitro
    if(m_kart->getSpeed()<5)
    {
        m_controls->m_nitro = true;
        return;
    }

    // If this kart is the last kart, and we have enough 
    // (i.e. more than 2) nitro, use it.
    // -------------------------------------------------
    const unsigned int num_karts = m_world->getCurrentNumKarts();
    if(m_kart->getPosition()== (int)num_karts && m_kart->getEnergy()>2.0f)
    {
        m_controls->m_nitro = true;
        return;
    }

    // On the last track shortly before the finishing line, use nitro 
    // anyway. Since the kart is faster with nitro, estimate a 50% time
    // decrease (additionally some nitro will be saved when top speed
    // is reached).
    if(m_world->getLapForKart(m_kart->getWorldKartId())==race_manager->getNumLaps()-1 &&
        m_nitro_level == NITRO_ALL)
    {
        float finish = m_world->getEstimatedFinishTime(m_kart->getWorldKartId());
        if( 1.5f*m_kart->getEnergy() >= finish - m_world->getTime() )
        {
            m_controls->m_nitro = true;
            return;
        }
    }

    // A kart within this distance is considered to be overtaking (or to be
    // overtaken).
    const float overtake_distance = 10.0f;

    // Try to overtake a kart that is close ahead, except 
    // when we are already much faster than that kart
    // --------------------------------------------------
    if(m_kart_ahead                                       && 
        m_distance_ahead < overtake_distance              &&
        m_kart_ahead->getSpeed()+5.0f > m_kart->getSpeed()   )
    {
            m_controls->m_nitro = true;
            return;
    }

    if(m_kart_behind                                   &&
        m_distance_behind < overtake_distance          &&
        m_kart_behind->getSpeed() > m_kart->getSpeed()    )
    {
        // Only prevent overtaking on highest level
        m_controls->m_nitro = m_nitro_level==NITRO_ALL;
        return;
    }
    
}   // handleNitroAndZipper

//-----------------------------------------------------------------------------
float DefaultAIController::steerToAngle(const size_t SECTOR, const float ANGLE)
{
    float angle = m_quad_graph->getAngleToNext(SECTOR,
                                               m_successor_index[SECTOR]);

    //Desired angle minus current angle equals how many angles to turn
    float steer_angle = angle - m_kart->getHeading();

    if(m_kart->hasViewBlockedByPlunger())
        steer_angle += ANGLE/5;
    else
        steer_angle += ANGLE;
    steer_angle = normalizeAngle( steer_angle );

    return steer_angle;
}   // steerToAngle

//-----------------------------------------------------------------------------
/** Computes the steering angle to reach a certain point. Note that the
 *  steering angle depends on the velocity of the kart (simple setting the
 *  steering angle towards the angle the point has is not correct: a slower
 *  kart will obviously turn less in one time step than a faster kart).
 *  \param point Point to steer towards.
 *  \param dt    Time step.
 */
float DefaultAIController::steerToPoint(const Vec3 &point, float dt)
{
    // No sense steering if we are not driving.
    if(m_kart->getSpeed()==0) return 0.0f;
    const float dx        = point.getX() - m_kart->getXYZ().getX();
    const float dz        = point.getZ() - m_kart->getXYZ().getZ();
    /** Angle from the kart position to the point in world coordinates. */
    float theta           = atan2(dx, dz);

    // Angle is the point is relative to the heading - but take the current
    // angular velocity into account, too. The value is multiplied by two
    // to avoid 'oversteering' - experimentally found.
    float angle_2_point   = theta - m_kart->getHeading() 
                          - dt*m_kart->getBody()->getAngularVelocity().getY()*2.0f;
    angle_2_point         = normalizeAngle(angle_2_point);
    if(fabsf(angle_2_point)<0.1) return 0.0f;

    /** To understand this code, consider how a given steering angle determines
     *  the angle the kart is facing after one timestep:
     *  sin(steer_angle) = wheel_base / radius;  --> compute radius of turn
     *  circumference    = radius * 2 * M_PI;    --> circumference of turn circle
     *  The kart drives dt*V units during a timestep of size dt. So the ratio
     *  of the driven distance to the circumference is the same as the angle
     *  the whole circle, or:
     *  angle / (2*M_PI) = dt*V / circumference
     *  Reversly, if the angle to drive to is given, the circumference can be
     *  computed, and from that the turn radius, and then the steer angle.
     *  (note: the 2*M_PI can be removed from the computations)
     */
    float radius          = dt*m_kart->getSpeed()/angle_2_point;
    float sin_steer_angle = m_kart->getKartProperties()->getWheelBase()/radius;
#ifdef DEBUG_OUTPUT
    printf("theta %f a2p %f angularv %f radius %f ssa %f\n",
        theta, angle_2_point, m_body->getAngularVelocity().getY(),
        radius, sin_steer_angle);
#endif
    // Add 0.1 since rouding errors will otherwise result in the kart
    // not using drifting.
    if(sin_steer_angle <= -1.0f)
        return -m_kart->getMaxSteerAngle()*m_skidding_threshold-0.1f;
    if(sin_steer_angle >=  1.0f) 
        return  m_kart->getMaxSteerAngle()*m_skidding_threshold+0.1f;
    float steer_angle     = asin(sin_steer_angle);    
    return steer_angle;
}   // steerToPoint

//-----------------------------------------------------------------------------
void DefaultAIController::checkCrashes( const int STEPS, const Vec3& pos )
{
    //Right now there are 2 kind of 'crashes': with other karts and another
    //with the track. The sight line is used to find if the karts crash with
    //each other, but the first step is twice as big as other steps to avoid
    //having karts too close in any direction. The crash with the track can
    //tell when a kart is going to get out of the track so it steers.
    m_crashes.clear();

    const size_t NUM_KARTS = m_world->getNumKarts();

    //Protection against having vel_normal with nan values
    const Vec3 &VEL = m_kart->getVelocity();
    Vec3 vel_normal(VEL.getX(), VEL.getZ(), 0.0);
    float speed=vel_normal.length();
    // If the velocity is zero, no sense in checking for crashes in time
    if(speed==0) return;

    // Time it takes to drive for m_kart_length units.
    float dt = m_kart_length / speed; 
    vel_normal/=speed;

    int current_node = m_track_node;
    for(int i = 1; STEPS > i; ++i)
    {
        Vec3 step_coord = pos + vel_normal* m_kart_length * float(i);

        /* Find if we crash with any kart, as long as we haven't found one
         * yet
         */
        if( m_crashes.m_kart == -1 )
        {
            for( unsigned int j = 0; j < NUM_KARTS; ++j )
            {
                const Kart* kart = m_world->getKart(j);
                if(kart==m_kart||kart->isEliminated()) continue;   // ignore eliminated karts
                const Kart *other_kart = m_world->getKart(j);
                // Ignore karts ahead that are faster than this kart.
                if(m_kart->getVelocityLC().getZ() < other_kart->getVelocityLC().getZ())
                    continue;
                Vec3 other_kart_xyz = other_kart->getXYZ() + other_kart->getVelocity()*(i*dt);
                float kart_distance = (step_coord - other_kart_xyz).length_2d();

                if( kart_distance < m_kart_length)
                    m_crashes.m_kart = j;
            }
        }

        /*Find if we crash with the drivelines*/
        if(current_node!=QuadGraph::UNKNOWN_SECTOR &&
            m_next_node_index[current_node]!=-1)
            m_quad_graph->findRoadSector(step_coord, &current_node,
                        /* sectors to test*/ &m_all_look_aheads[current_node]);

        if( current_node == QuadGraph::UNKNOWN_SECTOR)
        {
            m_crashes.m_road = true;
            return;
        }
    }
}   // checkCrashes

//-----------------------------------------------------------------------------
/** Find the sector that at the longest distance from the kart, that can be
 *  driven to without crashing with the track, then find towards which of
 *  the two edges of the track is closest to the next curve after wards,
 *  and return the position of that edge.
 */
void DefaultAIController::findNonCrashingPoint(Vec3 *result)
{    
    unsigned int sector = m_next_node_index[m_track_node];
    int target_sector;

    Vec3 direction;
    Vec3 step_track_coord;
    float distance;
    int steps;

    //We exit from the function when we have found a solution
    while( 1 )
    {
        //target_sector is the sector at the longest distance that we can drive
        //to without crashing with the track.
        target_sector = m_next_node_index[sector];

        //direction is a vector from our kart to the sectors we are testing
        direction = m_quad_graph->getQuad(target_sector).getCenter() 
                  - m_kart->getXYZ();

        float len=direction.length_2d();
        steps = int( len / m_kart_length );
        if( steps < 3 ) steps = 3;

        //Protection against having vel_normal with nan values
        if(len>0.0f) {
            direction*= 1.0f/len;
        }

        Vec3 step_coord;
        //Test if we crash if we drive towards the target sector
        for( int i = 2; i < steps; ++i )
        {
            step_coord = m_kart->getXYZ()+direction*m_kart_length * float(i);

            m_quad_graph->spatialToTrack(&step_track_coord, step_coord,
                                                   sector );
 
            distance = fabsf(step_track_coord[0]);

            //If we are outside, the previous sector is what we are looking for
            if ( distance + m_kart_width * 0.5f 
                 > m_quad_graph->getNode(sector).getPathWidth() )
            {
                *result = m_quad_graph->getQuad(sector).getCenter();
                return;
            }
        }
        sector = target_sector;
    }
}   // findNonCrashingPoint

//-----------------------------------------------------------------------------
void DefaultAIController::reset()
{
    m_time_since_last_shot       = 0.0f;
    m_start_kart_crash_direction = 0;
    m_curve_target_speed         = m_kart->getMaxSpeedOnTerrain();
    m_curve_angle                = 0.0;
    m_time_till_start            = -1.0f;
    m_crash_time                 = 0.0f;
    m_collided                   = false;
    m_time_since_stuck           = 0.0f;
    m_kart_ahead                 = NULL;
    m_distance_ahead             = 0.0f;
    m_kart_behind                = NULL;
    m_distance_behind            = 0.0f;

    Controller::reset();
    m_track_node               = QuadGraph::UNKNOWN_SECTOR;
    m_quad_graph->findRoadSector(m_kart->getXYZ(), &m_track_node);
    if(m_track_node==QuadGraph::UNKNOWN_SECTOR)
    {
        fprintf(stderr, "Invalid starting position for '%s' - not on track - can be ignored.\n",
                m_kart->getIdent().c_str());
        m_track_node = m_quad_graph->findOutOfRoadSector(m_kart->getXYZ());
    }

}   // reset

//-----------------------------------------------------------------------------
inline float DefaultAIController::normalizeAngle(float angle)
{
    while( angle >  2*M_PI ) angle -= 2*M_PI;
    while( angle < -2*M_PI ) angle += 2*M_PI;

    if( angle > M_PI ) angle -= 2*M_PI;
    else if( angle < -M_PI ) angle += 2*M_PI;

    return angle;
}

//-----------------------------------------------------------------------------
/** calc_steps() divides the velocity vector by the lenght of the kart,
 *  and gets the number of steps to use for the sight line of the kart.
 *  The calling sequence guarantees that m_future_sector is not UNKNOWN.
 */
int DefaultAIController::calcSteps()
{
    int steps = int( m_kart->getVelocityLC().getZ() / m_kart_length );
    if( steps < m_min_steps ) steps = m_min_steps;

    //Increase the steps depending on the width, if we steering hard,
    //mostly for curves.
#if 0
    // FIXME: I don't understand this: if we are steering hard, we check
    //        for more steps if we hit another kart?? If we steer hard,
    //        the approximation used (pos + velocity*dt) will be even
    //        worse, since it doesn't take steering into account.
    if( fabsf(m_controls->m_steer) > 0.95 )
    {
        const int WIDTH_STEPS = 
            (int)( m_quad_graph->getNode(m_future_sector).getPathWidth()
                   /( m_kart_length * 2.0 ) );

        steps += WIDTH_STEPS;
    }
#endif
    return steps;
}   // calcSteps

//-----------------------------------------------------------------------------
/** Converts the steering angle to a lr steering in the range of -1 to 1. 
 *  If the steering angle is too great, it will also trigger skidding. This 
 *  function uses a 'time till full steer' value specifying the time it takes
 *  for the wheel to reach full left/right steering similar to player karts 
 *  when using a digital input device. This is done to remove shaking of
 *  AI karts (which happens when the kart frequently changes the direction
 *  of a turn). The parameter is defined in the kart properties.
 *  \param angle Steering angle.
 *  \param dt Time step.
 */
void DefaultAIController::setSteering(float angle, float dt)
{
    float steer_fraction = angle / m_kart->getMaxSteerAngle();
    m_controls->m_drift   = fabsf(steer_fraction)>=m_skidding_threshold;
    if(m_kart->hasViewBlockedByPlunger()) m_controls->m_drift = false;
    float old_steer      = m_controls->m_steer;

    if     (steer_fraction >  1.0f) steer_fraction =  1.0f;
    else if(steer_fraction < -1.0f) steer_fraction = -1.0f;

    if(m_kart->hasViewBlockedByPlunger())
    {
        if     (steer_fraction >  0.5f) steer_fraction =  0.5f;
        else if(steer_fraction < -0.5f) steer_fraction = -0.5f;
    }
    
    // The AI has its own 'time full steer' value (which is the time
    float max_steer_change = dt/m_kart->getKartProperties()->getTimeFullSteerAI();
    if(old_steer < steer_fraction)
    {
        m_controls->m_steer = (old_steer+max_steer_change > steer_fraction) 
                           ? steer_fraction : old_steer+max_steer_change;
    }
    else
    {
        m_controls->m_steer = (old_steer-max_steer_change < steer_fraction) 
                           ? steer_fraction : old_steer-max_steer_change;
    }
}   // setSteering

//-----------------------------------------------------------------------------
/**FindCurve() gathers info about the closest sectors ahead: the curve
 * angle, the direction of the next turn, and the optimal speed at which the
 * curve can be travelled at it's widest angle.
 *
 * The number of sectors that form the curve is dependant on the kart's speed.
 */
void DefaultAIController::findCurve()
{
    float total_dist = 0.0f;
    int i;
    for(i = m_track_node; total_dist < m_kart->getVelocityLC().getZ(); 
        i = m_next_node_index[i])
    {
        total_dist += m_quad_graph->getDistanceToNext(i, m_successor_index[i]);
    }


    m_curve_angle = 
        normalizeAngle(m_quad_graph->getAngleToNext(i, m_successor_index[i])
                      -m_quad_graph->getAngleToNext(m_track_node, 
                                                    m_successor_index[m_track_node]) );
    
    m_curve_target_speed = m_kart->getMaxSpeedOnTerrain();
}   // findCurve
