//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009 Marianne Gagnon
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


#include "states_screens/state_manager.hpp"

#include "audio/sfx_manager.hpp"
#include "audio/music_manager.hpp"
#include "config/stk_config.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/screen.hpp"
#include "input/input_device.hpp"
#include "input/input_manager.hpp"
#include "main_loop.hpp"
#include "modes/world.hpp"
#include "states_screens/dialogs/race_paused_dialog.hpp"
#include "utils/translation.hpp"

using namespace GUIEngine;

StateManager* state_manager_singleton = NULL;

StateManager* StateManager::get()
{
    if (state_manager_singleton == NULL) state_manager_singleton = new StateManager();
    return state_manager_singleton;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark Player Management
#endif

// ----------------------------------------------------------------------------

StateManager::ActivePlayer* StateManager::getActivePlayer(const int id)
{
    ActivePlayer *returnPlayer = NULL;
    if (id < m_active_players.size() && id >= 0)
    {
        returnPlayer = m_active_players.get(id);
    }
    else
    {
        fprintf(stderr, "getActivePlayer(): id out of bounds\n");
        assert(false);
        return NULL;
    }
    
    assert( returnPlayer->m_id == id );
    
    return returnPlayer;
}

// ----------------------------------------------------------------------------

const PlayerProfile* StateManager::getActivePlayerProfile(const int id)
{
    ActivePlayer* a = getActivePlayer(id);
    if (a == NULL) return NULL;
    return a->getProfile();
}

// ----------------------------------------------------------------------------

void StateManager::updateActivePlayerIDs()
{
    const int amount = m_active_players.size();
    for (int n=0; n<amount; n++)
    {
        m_active_players[n].m_id = n;
    }
}

// ----------------------------------------------------------------------------

int StateManager::createActivePlayer(PlayerProfile *profile, InputDevice *device)
{
    ActivePlayer *p;
    int i;
    p = new ActivePlayer(profile, device);
    i = m_active_players.size();
    m_active_players.push_back(p);
    
    updateActivePlayerIDs();
    
    return i;
}

// ----------------------------------------------------------------------------

void StateManager::removeActivePlayer(int id)
{
    m_active_players.erase(id);
    updateActivePlayerIDs();
}

// ----------------------------------------------------------------------------

int StateManager::activePlayerCount()
{
    return m_active_players.size();
}

// ----------------------------------------------------------------------------

void StateManager::resetActivePlayers()
{
    const int amount = m_active_players.size();
    for(int i=0; i<amount; i++)
    {
        m_active_players[i].setDevice(NULL);
    }
    m_active_players.clearAndDeleteAll();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark misc stuff
#endif

bool StateManager::throttleFPS()
{
    return m_game_mode != GUIEngine::GAME  &&  GUIEngine::getCurrentScreen()->throttleFPS();
}

// ----------------------------------------------------------------------------

void StateManager::escapePressed()
{
    // in input sensing mode
    if(input_manager->isInMode(InputManager::INPUT_SENSE_KEYBOARD) ||
       input_manager->isInMode(InputManager::INPUT_SENSE_GAMEPAD) )
    {
        ModalDialog::dismiss();
        input_manager->setMode(InputManager::MENU);
    }
    // when another modal dialog is visible
    else if(ModalDialog::isADialogActive())
    {
        ModalDialog::getCurrent()->escapePressed();
    }
    // In-game
    else if(m_game_mode == GAME)
    {
        new RacePausedDialog(0.8f, 0.6f);
        //resetAndGoToMenu("main.stkgui");
        //input_manager->setMode(InputManager::MENU);
    }
    // In menus
    else
    {
        if (getCurrentScreen()->onEscapePressed()) popMenu();
    }
}

// ----------------------------------------------------------------------------

void StateManager::onGameStateChange(GameState previousState, GameState newState)
{
    if (newState == GAME)
    {
        irr_driver->hidePointer();
        input_manager->setMode(InputManager::INGAME);

        if (previousState == INGAME_MENU)
        {
            // unpause the world
            if (World::getWorld() != NULL) World::getWorld()->unpause();
        }
    }
    else  // menu (including in-game menu)
    {
        irr_driver->showPointer();
        input_manager->setMode(InputManager::MENU);
        sfx_manager->positionListener( Vec3(0,0,0), Vec3(0,1,0) );
        
        if (newState == MENU)
        {
            music_manager->startMusic(stk_config->m_title_music);
        }
        else if (newState == INGAME_MENU)
        {
            // pause game when an in-game menu is shown
            if (World::getWorld() != NULL) World::getWorld()->pause();
        }
    }    
}

// ----------------------------------------------------------------------------

void StateManager::onStackEmptied()
{
    main_loop->abort();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark ActivePlayer
#endif

StateManager::ActivePlayer::ActivePlayer(PlayerProfile* player, InputDevice *device)
{
    m_player = player;
    m_device = NULL;
    m_kart = NULL;
    setDevice(device);
}  // ActivePlayer

// ----------------------------------------------------------------------------
StateManager::ActivePlayer::~ActivePlayer()
{
    setDevice(NULL);
}   // ~ActivePlayer

// ----------------------------------------------------------------------------

void StateManager::ActivePlayer::setPlayerProfile(PlayerProfile* player)
{
    m_player = player;
}   // setPlayerProfile

// ----------------------------------------------------------------------------

void StateManager::ActivePlayer::setDevice(InputDevice* device)
{
    // unset player from previous device he was assigned to, if any
    if (m_device != NULL) m_device->setPlayer(NULL);
    
    m_device = device;
    
    // inform the devce of its new owner
    if (device != NULL) device->setPlayer(this);
}   // setDevice


