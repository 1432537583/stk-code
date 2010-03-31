//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006 SuperTuxKart-Team
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

#include "modes/three_strikes_battle.hpp"

#include <string>

#include "states_screens/race_gui.hpp"
#include "audio/sound_manager.hpp"
#include "tracks/track.hpp"

//-----------------------------------------------------------------------------
ThreeStrikesBattle::ThreeStrikesBattle() : World()
{
    WorldStatus::setClockMode(CLOCK_CHRONO);
    m_use_highscores = false;
}
void ThreeStrikesBattle::init()
{
    World::init();
    
    // check for possible problems if AI karts were incorrectly added
    if(getNumKarts() > race_manager->getNumPlayers())
    {
        fprintf(stderr, "No AI exists for this game mode\n");
        exit(1);
    }
 
    const unsigned int kart_amount = m_karts.size();
    m_kart_display_info = new RaceGUI::KartIconDisplayInfo[kart_amount];
    
    for(unsigned int n=0; n<kart_amount; n++)
    {
        // create the struct that ill hold each player's lives
        BattleInfo info;
        info.m_lives         = 3;
        m_kart_info.push_back(info);
        
        // no positions in this mode
        m_karts[n]->setPosition(-1);
    }// next kart
    
    
    BattleEvent evt;
    evt.m_time = 0.0f;
    evt.m_kart_info = m_kart_info;
    m_battle_events.push_back(evt);    
    
}   // ThreeStrikesBattle

//-----------------------------------------------------------------------------
ThreeStrikesBattle::~ThreeStrikesBattle()
{
    delete[] m_kart_display_info;
}   // ~ThreeStrikesBattle

//-----------------------------------------------------------------------------
void ThreeStrikesBattle::kartHit(const int kart_id)
{
    assert(kart_id >= 0);
    assert(kart_id < (int)m_karts.size());
    
    // make kart lose a life
    m_kart_info[kart_id].m_lives--;
    
    // record event
    BattleEvent evt;
    evt.m_time = getTime();
    evt.m_kart_info = m_kart_info;
    m_battle_events.push_back(evt);   
    
    updateKartRanks();
        
    // check if kart is 'dead'
    if (m_kart_info[kart_id].m_lives < 1)
    {
        m_karts[kart_id]->finishedRace(WorldStatus::getTime());
        removeKart(kart_id);
    }
    
    const unsigned int NUM_KARTS = getNumKarts();
    int num_karts_many_lives = 0;
 
    for (unsigned int n = 0; n < NUM_KARTS; ++n)
    {
        if (m_kart_info[n].m_lives > 1) num_karts_many_lives++;
    }
    
    // when almost over, use fast music
    if (num_karts_many_lives<=1 && !m_faster_music_active)
    {
        sound_manager->switchToFastMusic();
        m_faster_music_active = true;
    }
}   // kartHit

//-----------------------------------------------------------------------------
/** Returns the internal identifier for this race.
 */
std::string ThreeStrikesBattle::getIdent() const
{
    return STRIKES_IDENT;
}   // getIdent

//-----------------------------------------------------------------------------
void ThreeStrikesBattle::updateKartRanks()
{
    // sort karts by their times then give each one its position.
    // in battle-mode, long time = good (meaning he survived longer)
    
    const unsigned int NUM_KARTS = getNumKarts();
    
    int *karts_list = new int[NUM_KARTS];
    for( unsigned int n = 0; n < NUM_KARTS; ++n ) karts_list[n] = n;
    
    bool sorted=false;
    do
    {
        sorted = true;
        for( unsigned int n = 0; n < NUM_KARTS-1; ++n )
        {
            const int this_karts_time = m_karts[karts_list[n]]->hasFinishedRace() ?
                (int)m_karts[karts_list[n]]->getFinishTime() : (int)WorldStatus::getTime();
            const int next_karts_time = m_karts[karts_list[n+1]]->hasFinishedRace() ?
                (int)m_karts[karts_list[n+1]]->getFinishTime() : (int)WorldStatus::getTime();

            bool swap = false;
            
            // if next kart survived longer...
            if( next_karts_time > this_karts_time) swap = true;
            // if next kart has more lives...
            else if(m_kart_info[karts_list[n+1]].m_lives > m_kart_info[karts_list[n]].m_lives) swap = true;

            if(swap)
            {
                int tmp = karts_list[n+1];
                karts_list[n+1] = karts_list[n];
                karts_list[n] = tmp;
                sorted = false;
                break;
            }
        }
    } while(!sorted);
    
    for( unsigned int n = 0; n < NUM_KARTS; ++n )
    {
        m_karts[ karts_list[n] ]->setPosition( n+1 );
    }
    delete [] karts_list;
}   // updateKartRank

//-----------------------------------------------------------------------------
/** The battle is over if only one kart is left, or no player kart.
 */
bool ThreeStrikesBattle::isRaceOver()
{
    return getCurrentNumKarts()==1 || getCurrentNumPlayers()==0;
}   // isRaceOver

//-----------------------------------------------------------------------------
/** Called when the race finishes, i.e. after playing (if necessary) an
 *  end of race animation. It updates the time for all karts still racing,
 *  and then updates the ranks.
 */
void ThreeStrikesBattle::terminateRace()
{
    updateKartRanks();   
    World::terminateRace();
}   // terminateRace

//-----------------------------------------------------------------------------
void ThreeStrikesBattle::restartRace()
{
    World::restartRace();
    
    const unsigned int kart_amount = m_karts.size();
    
    for(unsigned int n=0; n<kart_amount; n++)
    {
        m_kart_info[n].m_lives         = 3;
        
        // no positions in this mode
        m_karts[n]->setPosition(-1);
    }// next kart
    
    m_battle_events.clear();
}   // restartRace

//-----------------------------------------------------------------------------
RaceGUI::KartIconDisplayInfo* ThreeStrikesBattle::getKartsDisplayInfo()
{
    const unsigned int kart_amount = getNumKarts();
    for(unsigned int i = 0; i < kart_amount ; i++)
    {
        RaceGUI::KartIconDisplayInfo& rank_info = m_kart_display_info[i];
        
        // reset color
        rank_info.lap = -1;
        
        switch(m_kart_info[i].m_lives)
        {
            case 3:
                rank_info.r = 0.0;
                rank_info.g = 1.0;
                rank_info.b = 0.0;
                break;
            case 2:
                rank_info.r = 1.0;
                rank_info.g = 0.9f;
                rank_info.b = 0.0;
                break;
            case 1:
                rank_info.r = 1.0;
                rank_info.g = 0.0;
                rank_info.b = 0.0;
                break;
            case 0:
                rank_info.r = 0.5;
                rank_info.g = 0.5;
                rank_info.b = 0.5;
                break;
        }
        
        char lives[4];
        sprintf(lives, "%i", m_kart_info[i].m_lives);
        
        rank_info.m_text = lives;
    }
    
    return m_kart_display_info;
}   // getKartDisplayInfo

//-----------------------------------------------------------------------------
void ThreeStrikesBattle::moveKartAfterRescue(Kart* kart, btRigidBody* body)
{
    // find closest point to drop kart on
    World *world = World::getWorld();
    const int start_spots_amount = world->getTrack()->getNumberOfStartPositions();
    assert(start_spots_amount > 0);
    
    float smallest_distance_found = -1;
    int  closest_id_found = -1;
    
    const float kart_x = kart->getXYZ().getX();
    const float kart_z = kart->getXYZ().getZ();
    
    for(int n=0; n<start_spots_amount; n++)
    {
        // no need for the overhead to compute exact distance with sqrt(), so using the
        // 'manhattan' heuristic which will do fine enough.
        const Vec3 &v=world->getTrack()->getStartPosition(n);
        const float dist_n= fabs(kart_x - v.getX()) +
                            fabs(kart_z - v.getZ());
        if(dist_n < smallest_distance_found || closest_id_found == -1)
        {
            closest_id_found = n;
            smallest_distance_found = dist_n;
        }
    }
    
    assert(closest_id_found != -1);
    const Vec3 &v=world->getTrack()->getStartPosition(closest_id_found);
    kart->setXYZ( Vec3(v) );
    
    // FIXME - implement correct heading
    btQuaternion heading(btVector3(0.0f, 1.0f, 0.0f), 
                         world->getTrack()->getStartHeading(closest_id_found));
    kart->setRotation(heading);

    //position kart from same height as in World::resetAllKarts
    btTransform pos;
    pos.setOrigin(kart->getXYZ()+btVector3(0, 0.5f*kart->getKartHeight(), 0.0f));
    pos.setRotation( btQuaternion(btVector3(0.0f, 1.0f, 0.0f), 0 /* angle */) );

    body->setCenterOfMassTransform(pos);

    //project kart to surface of track
    bool kart_over_ground = m_physics->projectKartDownwards(kart);

    if (kart_over_ground)
    {
        //add vertical offset so that the kart starts off above the track
        float vertical_offset = kart->getKartProperties()->getVertRescueOffset() *
                                kart->getKartHeight();
        body->translate(btVector3(0, vertical_offset, 0));
    }
    else
    {
        fprintf(stderr, "WARNING: invalid position after rescue for kart %s on track %s.\n",
                (kart->getIdent().c_str()), m_track->getIdent().c_str());
    }
    
    //add hit to kart
    kartHit(kart->getWorldKartId());
}   // moveKartAfterRescue

//-----------------------------------------------------------------------------
void ThreeStrikesBattle::raceResultOrder( int* order )
{
    updateKartRanks();
    
    const unsigned int num_karts = getNumKarts();
    for( unsigned int kart_id    = 0; kart_id < num_karts; ++kart_id )
    {
        const int pos = m_karts[kart_id]->getPosition() - 1;
        assert(pos >= 0);
        assert(pos < (int)num_karts);
        order[pos] = kart_id;
    }
}   // raceResultOrder

//-----------------------------------------------------------------------------
bool ThreeStrikesBattle::acceptPowerup(const PowerupType type) const
{
    // these powerups don't make much sense in battle mode
    if (type == POWERUP_PARACHUTE || type == POWERUP_ANVIL ||
        type == POWERUP_BUBBLEGUM || type == POWERUP_ZIPPER ||
        type == POWERUP_SWITCH    || type == POWERUP_PLUNGER)
    {
       return false;
    }
    
    return true;
}   // acceptPowerup

