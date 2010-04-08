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

#include <irrlicht.h>

#include "challenges/unlock_manager.hpp"
#include "guiengine/widget.hpp"
#include "io/file_manager.hpp"
#include "race/race_manager.hpp"
#include "states_screens/arenas_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "states_screens/tracks_screen.hpp"
#include "utils/translation.hpp"

#include "states_screens/race_setup_screen.hpp"


using namespace GUIEngine;
DEFINE_SCREEN_SINGLETON( RaceSetupScreen );

class GameModeRibbonListener : public DynamicRibbonHoverListener
{
    RaceSetupScreen* m_parent;
public:

    GameModeRibbonListener(RaceSetupScreen* parent)
    {
        m_parent = parent;
    }

    virtual void onSelectionChanged(DynamicRibbonWidget* theWidget, const std::string& selectionID, 
                                    const irr::core::stringw& selectionText, const int playerID)
    {
        // game mode changed!!
        m_parent->onGameModeChanged();
    }
};

// -----------------------------------------------------------------------------

RaceSetupScreen::RaceSetupScreen() : Screen("racesetup.stkgui")
{
}

// -----------------------------------------------------------------------------

void RaceSetupScreen::eventCallback(Widget* widget, const std::string& name, const int playerID)
{
    if (name == "difficulty")
    {
        RibbonWidget* w = dynamic_cast<RibbonWidget*>(widget);
        assert(w != NULL);
        const std::string& selection = w->getSelectionIDString(GUI_PLAYER_ID);
        
        if (selection == "novice")
        {
            UserConfigParams::m_difficulty = RaceManager::RD_EASY;
            race_manager->setDifficulty(RaceManager::RD_EASY);
        }
        else if (selection == "intermediate")
        {
            UserConfigParams::m_difficulty = RaceManager::RD_MEDIUM;
            race_manager->setDifficulty(RaceManager::RD_MEDIUM);
        }
        else if (selection == "expert")
        {
            UserConfigParams::m_difficulty = RaceManager::RD_HARD;
            race_manager->setDifficulty(RaceManager::RD_HARD);
        }
    }
    else if (name == "gamemode")
    {
        DynamicRibbonWidget* w = dynamic_cast<DynamicRibbonWidget*>(widget);
        const std::string selectedMode = w->getSelectionIDString(GUI_PLAYER_ID);
        
        if (selectedMode == "normal")
        {
            race_manager->setMinorMode(RaceManager::MINOR_MODE_NORMAL_RACE);
            StateManager::get()->pushScreen( TracksScreen::getInstance() );
        }
        else if (selectedMode == "timetrial")
        {
            race_manager->setMinorMode(RaceManager::MINOR_MODE_TIME_TRIAL);
            StateManager::get()->pushScreen( TracksScreen::getInstance() );
        }
        else if (selectedMode == "ftl")
        {
            race_manager->setMinorMode(RaceManager::MINOR_MODE_FOLLOW_LEADER);
            StateManager::get()->pushScreen( TracksScreen::getInstance() );
        }
        else if (selectedMode == "3strikes")
        {
            race_manager->setMinorMode(RaceManager::MINOR_MODE_3_STRIKES);
            race_manager->setNumKarts( race_manager->getNumPlayers() ); // no AI karts;
            StateManager::get()->pushScreen( ArenasScreen::getInstance() );
        }
        else if (selectedMode == "locked")
        {
            unlock_manager->playLockSound();
        }
    }
    else if (name == "aikartamount")
    {
        SpinnerWidget* w = dynamic_cast<SpinnerWidget*>(widget);
        
        race_manager->setNumKarts( race_manager->getNumPlayers() + w->getValue() );
    }
}

// -----------------------------------------------------------------------------

void RaceSetupScreen::onGameModeChanged()
{
    DynamicRibbonWidget* w2 = getWidget<DynamicRibbonWidget>("gamemode");
    assert( w2 != NULL );
    
    //FIXME: don't hardcode player 0?
    std::string gamemode = w2->getSelectionIDString(0);
    
    // deactivate the AI karts count widget for modes for which we have no AI
    //FIXME? Don't hardcode here which modes have an AI and which don't
    SpinnerWidget* kartamount = getWidget<SpinnerWidget>("aikartamount");
    kartamount->m_deactivated = (gamemode == "3strikes");
    
    // use this dirty trick to hide the number inside the spinner (FIXME)
    if (kartamount->m_deactivated)
    {
        kartamount->m_text = L"-";
        kartamount->setValue( kartamount->getValue() );
    }
    else if (kartamount->m_text.size() > 0)
    {
        kartamount->m_text = L"";
        kartamount->setValue( kartamount->getValue() );
    }
}

// -----------------------------------------------------------------------------

void RaceSetupScreen::init()
{
    RibbonWidget* w = getWidget<RibbonWidget>("difficulty");
    assert( w != NULL );
    w->setSelection( race_manager->getDifficulty(), GUI_PLAYER_ID );
    
    SpinnerWidget* kartamount = getWidget<SpinnerWidget>("aikartamount");
    kartamount->setValue( race_manager->getNumberOfKarts() - race_manager->getNumPlayers() );
    
    DynamicRibbonWidget* w2 = getWidget<DynamicRibbonWidget>("gamemode");
    assert( w2 != NULL );
    w2->clearItems();

    // ---- Add game modes
    irr::core::stringw name1 = irr::core::stringw(
        RaceManager::getNameOf(RaceManager::MINOR_MODE_NORMAL_RACE)) + L"\n";
    //FIXME: avoid duplicating descriptions from the help menu!
    name1 +=  _("All blows allowed, so catch weapons and make clever use of them!");
    
    w2->addItem( name1, "normal", RaceManager::getIconOf(RaceManager::MINOR_MODE_NORMAL_RACE));
    
    irr::core::stringw name2 = irr::core::stringw(
        RaceManager::getNameOf(RaceManager::MINOR_MODE_TIME_TRIAL)) + L"\n";
    //FIXME: avoid duplicating descriptions from the help menu!
    name2 += _("Contains no powerups, so only your driving skills matter!");
    w2->addItem( name2, "timetrial", RaceManager::getIconOf(RaceManager::MINOR_MODE_TIME_TRIAL));
    
    if (unlock_manager->isLocked("followtheleader"))
    {
        w2->addItem( _("Locked : solve active challenges to gain access to more!"),
                    "locked", RaceManager::getIconOf(RaceManager::MINOR_MODE_FOLLOW_LEADER), true);
    }
    else
    {
        irr::core::stringw name3 = irr::core::stringw(
            RaceManager::getNameOf(RaceManager::MINOR_MODE_FOLLOW_LEADER)) + L"\n";
        //FIXME: avoid duplicating descriptions from the help menu!
        name3 += _("Run for second place, as the last kart will be disqualified every time the counter hits zero. Beware : going in front of the leader will get you eliminated too!");
        w2->addItem(name3, "ftl", RaceManager::getIconOf(RaceManager::MINOR_MODE_FOLLOW_LEADER), false);
    }
    
    if (race_manager->getNumPlayers() > 1)
    {
        irr::core::stringw name4 = irr::core::stringw(
            RaceManager::getNameOf(RaceManager::MINOR_MODE_3_STRIKES)) + L"\n";
        //FIXME: avoid duplicating descriptions from the help menu!
        name4 += _("Hit others with weapons until they lose all their lives. (Only in multiplayer games)");
        w2->addItem( name4, "3strikes", RaceManager::getIconOf(RaceManager::MINOR_MODE_3_STRIKES));
    }
    

    w2->updateItemDisplay();    
    
    //FIXME: it's unclear to me whether I must add a listener everytime init is called or not
    m_mode_listener = new GameModeRibbonListener(this);
    w2->registerHoverListener(m_mode_listener);
}

// -----------------------------------------------------------------------------

void RaceSetupScreen::tearDown()
{
    //delete m_mode_listener;
    //m_mode_listener = NULL;
    
    //DynamicRibbonWidget* w2 = getWidget<DynamicRibbonWidget>("gamemode");
    //assert( w2 != NULL );
    // w2->setListener(NULL);
}

// -----------------------------------------------------------------------------
