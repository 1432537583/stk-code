//  $Id$
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006 
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

#include "challenges/unlock_manager.hpp"
#include "config/player.hpp"
#include "config/user_config.hpp"
#include "kart_selection.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/screen.hpp"
#include "guiengine/widgets.hpp"
#include "input/input.hpp"
#include "input/input_manager.hpp"
#include "input/device_manager.hpp"
#include "input/input_device.hpp"
#include "items/item_manager.hpp"
#include "io/file_manager.hpp"
#include "karts/kart.hpp"
#include "karts/kart_properties_manager.hpp"
#include "states_screens/race_setup_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/translation.hpp"
#include "utils/random_generator.hpp"
#include "utils/string_utils.hpp"

#include <string>

InputDevice* player_1_device = NULL;

using namespace GUIEngine;
using irr::core::stringw;

const char* RANDOM_KART_ID = "randomkart";
const char* ALL_KART_GROUPS_ID  = "all";
const char* ID_DONT_USE = "x";

DEFINE_SCREEN_SINGLETON( KartSelectionScreen );

class PlayerKartWidget;
    
/** Currently, navigation for multiple players at the same time is implemented in
    a somewhat clunky way. An invisible "dispatcher" widget is added above kart
    icons. When a player moves up, he focuses the dispatcher, which in turn moves
    the selection to the appropriate spinner. "tabbing roots" are used to make
    navigation back down possible. (FIXME: maybe find a cleaner way?) */
int g_root_id;

class FocusDispatcher : public Widget
{
    KartSelectionScreen* m_parent;
    int m_reserved_id;
public:
    FocusDispatcher(KartSelectionScreen* parent) : Widget(WTYPE_BUTTON)
    {
        m_parent = parent;
        m_reserved_id = -1;
        m_supports_multiplayer = true;
        
        x = 0;
        y = 0;
        w = 1;
        h = 1;
    }
    
    void setRootID(const int reservedID)
    {
        m_reserved_id = reservedID;
    }
    
    virtual void add()
    {
        assert(m_reserved_id != -1);
        core::rect<s32> widget_size = core::rect<s32>(x, y, x + w, y + h);
        
        //gui::IGUIStaticText* irrwidget = GUIEngine::getGUIEnv()->addStaticText(L" ", widget_size, false, false, NULL, m_reserved_id);
        m_element = GUIEngine::getGUIEnv()->addButton(widget_size, NULL, m_reserved_id, L"Dispatcher", L"");
        
        id = m_element->getID();
        m_element->setTabStop(true);
        m_element->setTabGroup(false);
        m_element->setTabOrder(id);
        m_element->setVisible(false);
    }
    
    virtual EventPropagation focused(const int playerID);
};
    
FocusDispatcher* g_dispatcher = NULL;

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/** A small extension to the spinner widget to add features like player ID management or badging */
class PlayerNameSpinner : public SpinnerWidget
{
    int m_playerID;
    bool m_incorrect;
    irr::gui::IGUIImage* m_red_mark_widget;
    KartSelectionScreen* m_parent;
    
    //virtual EventPropagation focused(const int m_playerID) ;
    
public:
    PlayerNameSpinner(KartSelectionScreen* parent, const int playerID)
    {
        m_playerID        = playerID;
        m_incorrect       = false;
        m_red_mark_widget = NULL;
        m_parent          = parent;
    }
    void setID(const int m_playerID)
    {
        PlayerNameSpinner::m_playerID = m_playerID;
    }
    
    /** Add a red mark on the spinner to mean "invalid choice" */
    void markAsIncorrect()
    {
        if (m_incorrect) return; // already flagged as incorrect
        
        m_incorrect = true;
        
        irr::video::ITexture* texture = irr_driver->getTexture( file_manager->getTextureFile("red_mark.png").c_str() ) ;
        const int mark_size = h;
        const int mark_x = w - mark_size*2;
        const int mark_y = 0;
        core::rect< s32 > red_mark_area(mark_x, mark_y, mark_x + mark_size, mark_y + mark_size);
        m_red_mark_widget = GUIEngine::getGUIEnv()->addImage( red_mark_area, /* parent */ m_element );
        m_red_mark_widget->setImage(texture);
        m_red_mark_widget->setScaleImage(true);
        m_red_mark_widget->setTabStop(false);
        m_red_mark_widget->setUseAlphaChannel(true);
    }
    
    /** Remove any red mark set with 'markAsIncorrect' */
    void markAsCorrect()
    {
        if (m_incorrect)
        {
            m_red_mark_widget->remove();
            m_red_mark_widget = NULL;
            m_incorrect = false;
        }
    }
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

/** A widget representing the kart selection for a player (i.e. the player's number, name, the kart view, the kart's name) */
class PlayerKartWidget : public Widget
{        
    /** Whether this player confirmed their selection */
    bool m_ready;
    
    /** widget coordinates */
    int player_id_x, player_id_y, player_id_w, player_id_h;
    int player_name_x, player_name_y, player_name_w, player_name_h;
    int model_x, model_y, model_w, model_h;
    int kart_name_x, kart_name_y, kart_name_w, kart_name_h;
    
    /** A reserved ID for this widget if any, -1 otherwise.  (If no ID is reserved, widget will not be
        in the regular tabbing order */
    int m_irrlicht_widget_ID;
    
    /** For animation purposes (see method 'move') */
    int target_x, target_y, target_w, target_h;
    float x_speed, y_speed, w_speed, h_speed;

    /** Object representing this player */
    StateManager::ActivePlayer* m_associatedPlayer;
    int m_playerID;

    /** Internal name of the spinner; useful to interpret spinner events, which contain the name of the activated object */
    std::string spinnerID;
    
public:
    /** Sub-widgets created by this widget */
    LabelWidget* playerIDLabel;
    PlayerNameSpinner* playerName;
    ModelViewWidget* modelView;
    LabelWidget* kartName;
    
    

    LabelWidget *getPlayerIDLabel() {return playerIDLabel;}
    std::string deviceName;
    std::string m_kartInternalName;
    
    PlayerKartWidget(KartSelectionScreen* parent,StateManager:: ActivePlayer* associatedPlayer,
                     Widget* area, const int m_playerID, const int irrlichtWidgetID=-1) : Widget(WTYPE_DIV)
    {
        m_associatedPlayer = associatedPlayer;
        x_speed = 1.0f;
        y_speed = 1.0f;
        w_speed = 1.0f;
        h_speed = 1.0f;
        m_ready = false;
        
        m_irrlicht_widget_ID = irrlichtWidgetID;

        this->m_playerID = m_playerID;
        this->m_properties[PROP_ID] = StringUtils::insertValues("@p%i", m_playerID);
        
        setSize(area->x, area->y, area->w, area->h);
        target_x = x;
        target_y = y;
        target_w = w;
        target_h = h;
        
        // ---- Player ID label
        if (associatedPlayer->getDevice()->getType() == DT_KEYBOARD)
        {
            deviceName += "keyboard";
        }
        else if (associatedPlayer->getDevice()->getType() == DT_GAMEPAD)
        {
            deviceName += "gamepad";
        }
        
        playerIDLabel = new LabelWidget();
        
        playerIDLabel->m_text = 
            //I18N: In kart selection screen (Will read like 'Player 1 (foobartech gamepad)')
            StringUtils::insertValues(_("Player %i (%s)"), m_playerID + 1, deviceName.c_str()); 
        
        playerIDLabel->m_properties[PROP_TEXT_ALIGN] = "center";
        playerIDLabel->m_properties[PROP_ID] = StringUtils::insertValues("@p%i_label", m_playerID);
        playerIDLabel->x = player_id_x;
        playerIDLabel->y = player_id_y;
        playerIDLabel->w = player_id_w;
        playerIDLabel->h = player_id_h;
        
        //playerID->setParent(this);
        m_children.push_back(playerIDLabel);
        
        // ---- Player identity spinner
        playerName = new PlayerNameSpinner(parent, m_playerID);
        playerName->x = player_name_x;
        playerName->y = player_name_y;
        playerName->w = player_name_w;
        playerName->h = player_name_h;
        //playerName->m_event_handler = this;
        
        if (irrlichtWidgetID == -1)
        {
            playerName->m_tab_down_root = g_root_id;
        }
        
        spinnerID = StringUtils::insertValues("@p%i_spinner", m_playerID);
        
        const int playerAmount = UserConfigParams::m_all_players.size();
        playerName->m_properties[PROP_MIN_VALUE] = "0";
        playerName->m_properties[PROP_MAX_VALUE] = (playerAmount-1);
        playerName->m_properties[PROP_ID] = spinnerID;
        playerName->m_properties[PROP_WARP_AROUND] = "true";

        //playerName->m_event_handler = this;
        m_children.push_back(playerName);
        
        
        // ----- Kart model view
        modelView = new ModelViewWidget();
        
        modelView->x = model_x;
        modelView->y = model_y;
        modelView->w = model_w;
        modelView->h = model_h;
        modelView->m_properties[PROP_ID] = StringUtils::insertValues("@p%i_model", m_playerID);
        //modelView->setParent(this);
        m_children.push_back(modelView);
        
        // Init kart model
        std::string& default_kart = UserConfigParams::m_default_kart;
        const KartProperties* props = kart_properties_manager->getKart(default_kart);
        KartModel* kartModel = props->getKartModel();
        
        this->modelView->addModel( kartModel->getModel(), Vec3(0,0,0), kartModel->getBaseFrame() );
        this->modelView->addModel( kartModel->getWheelModel(0), kartModel->getWheelGraphicsPosition(0) );
        this->modelView->addModel( kartModel->getWheelModel(1), kartModel->getWheelGraphicsPosition(1) );
        this->modelView->addModel( kartModel->getWheelModel(2), kartModel->getWheelGraphicsPosition(2) );
        this->modelView->addModel( kartModel->getWheelModel(3), kartModel->getWheelGraphicsPosition(3) );
        this->modelView->setRotateContinuously( 35.0f );
        
        // ---- Kart name label
        kartName = new LabelWidget();
        kartName->m_text = props->getName();
        kartName->m_properties[PROP_TEXT_ALIGN] = "center";
        kartName->m_properties[PROP_ID] = StringUtils::insertValues("@p%i_kartname", m_playerID);
        kartName->x = kart_name_x;
        kartName->y = kart_name_y;
        kartName->w = kart_name_w;
        kartName->h = kart_name_h;
        //kartName->setParent(this);
        m_children.push_back(kartName);
    }
    
    ~PlayerKartWidget()
    {
        if (GUIEngine::getFocusForPlayer(m_playerID) == this)
        {
            GUIEngine::focusNothingForPlayer(m_playerID);
        }

        if (playerIDLabel->getIrrlichtElement() != NULL)
            playerIDLabel->getIrrlichtElement()->remove();
        
        if (playerName != NULL && playerName->getIrrlichtElement() != NULL)
            playerName->getIrrlichtElement()->remove();
        
        if (modelView->getIrrlichtElement() != NULL)
            modelView->getIrrlichtElement()->remove();

        if (kartName->getIrrlichtElement() != NULL)
            kartName->getIrrlichtElement()->remove();
        
        getCurrentScreen()->manualRemoveWidget(this);
    }
    
    /** Called when players are renumbered (changes the player ID) */
    void setPlayerID(const int newPlayerID)
    {
        if (StateManager::get()->getActivePlayer(newPlayerID) != m_associatedPlayer)
        {
            printf("Player: %p\nIndex: %d\nm_associatedPlayer: %p\n",
                   StateManager::get()->getActivePlayer(newPlayerID), newPlayerID, m_associatedPlayer);
            std::cerr << "Internal inconsistency, PlayerKartWidget has IDs and pointers that do not correspond to one player\n";
            assert(false);
        }
        
        // Remove current focus, but rembmer it
        Widget* focus = GUIEngine::getFocusForPlayer(m_playerID);
        GUIEngine::focusNothingForPlayer(m_playerID);

        // Change the player ID
        m_playerID = newPlayerID;
        
        // restore previous focus, but with new player ID
        if (focus != NULL) focus->setFocusForPlayer(m_playerID);

        //I18N: In kart selection screen (Will read like 'Player 1 (foobartech gamepad)')
        irr::core::stringw  newLabel = StringUtils::insertValues(_("Player %i (%s)"), m_playerID + 1, deviceName.c_str());
        playerIDLabel->setText( newLabel ); 
        playerIDLabel->m_properties[PROP_ID] = StringUtils::insertValues("@p%i_label", m_playerID);
        
        if (playerName != NULL) playerName->setID(m_playerID);
    }
    
    /** Returns the ID of this player */
    int getPlayerID() const
    {
        return m_playerID;
    }
    
    /** Add the widgets to the current screen */
    virtual void add()
    {
        playerIDLabel->add();
        
        // the first player will have an ID of its own to allow for keyboard navigation despite this widget being added last
        if (m_irrlicht_widget_ID != -1) playerName->m_reserved_id = m_irrlicht_widget_ID;
        else playerName->m_reserved_id = Widget::getNewNoFocusID();
        
        playerName->add();
        playerName->getIrrlichtElement()->setTabStop(false);
        
        modelView->add();
        kartName->add();
        
        modelView->update(0);
        
        playerName->clearLabels();
        const int playerAmount = UserConfigParams::m_all_players.size();
        for (int n=0; n<playerAmount; n++)
        {
            playerName->addLabel( UserConfigParams::m_all_players[n].getName() );
        }
        
        // select the right player profile in the spinner
        playerName->setValue(m_associatedPlayer->getProfile()->getName());
        
        assert(playerName->getStringValue() == m_associatedPlayer->getProfile()->getName());
    }
    
    /** Get the associated ActivePlayer object*/
    StateManager::ActivePlayer* getAssociatedPlayer()
    {
        return m_associatedPlayer;
    }
    
    /** Starts a 'move/resize' animation, by simply passing destination coords. The animation
        will then occur on each call to 'onUpdate'. */
    void move(const int x, const int y, const int w, const int h)
    {
        target_x = x;
        target_y = y;
        target_w = w;
        target_h = h;
        
        x_speed = abs( this->x - x ) / 300.0f;
        y_speed = abs( this->y - y ) / 300.0f;
        w_speed = abs( this->w - w ) / 300.0f;
        h_speed = abs( this->h - h ) / 300.0f;
    }
    
    /** Call when player confirmed his identity and kart */
    void markAsReady()
    {
        if (m_ready) return; // already ready
        
        m_ready = true;
        
        stringw playerNameString = playerName->getStringValue();
        playerIDLabel->setText( StringUtils::insertValues( _("%s is ready"), playerNameString ) );
        
        m_children.remove(playerName);
        playerName->getIrrlichtElement()->remove();
        playerName->elementRemoved();
        playerName = NULL;
        
        sfx_manager->quickSound( "wee" );
        
        modelView->setRotateTo(30.0f, 150.0f);
        
        player_id_w *= 2;
        player_name_w = 0;
        
        modelView->setBadge(OK_BADGE);
    }
    
    /** \return Whether this player confirmed his kart and indent selection */
    bool isReady()
    {
        return m_ready;
    }
    
    /** Updates the animation (moving/shrinking/etc.) */
    void onUpdate(float delta)
    {
        if (target_x == x && target_y == y && target_w == w && target_h == h) return;
        
        int move_step = (int)(delta*1000.0f);
        
        // move x towards target
        if (x < target_x)
        {
            x += (int)(move_step*x_speed);
            if (x > target_x) x = target_x; // don't move to the other side of the target
        }
        else if (x > target_x)
        {
            x -= (int)(move_step*x_speed);
            if (x < target_x) x = target_x; // don't move to the other side of the target
        }
        
        // move y towards target
        if (y < target_y)
        {
            y += (int)(move_step*y_speed);
            if (y > target_y) y = target_y; // don't move to the other side of the target
        }
        else if (y > target_y)
        {
            y -= (int)(move_step*y_speed);
            if (y < target_y) y = target_y; // don't move to the other side of the target
        }
        
        // move w towards target
        if (w < target_w)
        {
            w += (int)(move_step*w_speed);
            if (w > target_w) w = target_w; // don't move to the other side of the target
        }
        else if (w > target_w)
        {
            w -= (int)(move_step*w_speed);
            if (w < target_w) w = target_w; // don't move to the other side of the target
        }
        // move h towards target
        if (h < target_h)
        {
            h += (int)(move_step*h_speed);
            if (h > target_h) h = target_h; // don't move to the other side of the target
        }
        else if (h > target_h)
        {
            h -= (int)(move_step*h_speed);
            if (h < target_h) h = target_h; // don't move to the other side of the target
        }
        
        setSize(x, y, w, h);
        
        playerIDLabel->move(player_id_x,
                            player_id_y,
                            player_id_w,
                            player_id_h);
        
        if (playerName != NULL)
        {
            playerName->move(player_name_x,
                             player_name_y,
                             player_name_w,
                             player_name_h );
        }
        modelView->move(model_x,
                        model_y,
                        model_w,
                        model_h);
        kartName->move(kart_name_x,
                       kart_name_y,
                       kart_name_w,
                       kart_name_h);
        
    }

    /** Event callback */
    virtual GUIEngine::EventPropagation transmitEvent(Widget* w, const std::string& originator, const int m_playerID)
    {
        if (m_ready) return EVENT_LET; // if it's declared ready, there is really nothing to process
        
        //std::cout << "= kart selection :: transmitEvent " << originator << " =\n";

        std::string name = w->m_properties[PROP_ID];
        
        //std::cout << "    (checking if that's me: I am " << spinnerID << ")\n";
        
        // update player profile when spinner changed
        if (originator == spinnerID)
        {
            if(UserConfigParams::m_verbosity>=5)
            {
                std::cout << "Identity changed for player " << m_playerID
                          << " : " << irr::core::stringc(playerName->getStringValue().c_str()).c_str() 
                          << std::endl;
            }
            m_associatedPlayer->setPlayerProfile( UserConfigParams::m_all_players.get(playerName->getValue()) );
        }

        return EVENT_LET; // continue propagating the event
    }

    /** Sets the size of the widget as a whole, and placed children widgets inside itself */
    void setSize(const int x, const int y, const int w, const int h)
    {
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;
        
        // -- sizes
        player_id_w = w;
        player_id_h = 25; // FIXME : don't hardcode font size
        
        player_name_h = 40;
        player_name_w = std::min(400, w);
        
        kart_name_w = w;
        kart_name_h = 25;
        
        // for shrinking effect
        if (h < 175)
        {
            const float factor = h / 175.0f;
            kart_name_h   = (int)(kart_name_h*factor);
            player_name_h = (int)(player_name_h*factor);
            player_id_h   = (int)(player_id_h*factor);
        }
        
        // --- layout
        player_id_x = x;
        player_id_y = y;
        
        player_name_x = x + w/2 - player_name_w/2;
        player_name_y = y + player_id_h;

        const int modelMaxHeight =  h - kart_name_h - player_name_h - player_id_h;
        const int modelMaxWidth =  w;
        const int bestSize = std::min(modelMaxWidth, modelMaxHeight);
        const int modelY = y + player_name_h + player_id_h;
        model_x = x + w/2 - (int)(bestSize/2);
        model_y = modelY + modelMaxHeight/2 - bestSize/2;
        model_w = (int)(bestSize);
        model_h = bestSize;
        
        kart_name_x = x;
        kart_name_y = y + h - kart_name_h;
    }
    
    /** Sets which kart was selected for this player */
    void setKartInternalName(const std::string& whichKart)
    {
        m_kartInternalName = whichKart;
    }
    const std::string& getKartInternalName() const
    {
        return m_kartInternalName;
    }
};

/** Small utility function that returns whether the two given players chose the same kart.
 * The advantage of this function is that it can handle "random kart" selection. */
bool sameKart(const PlayerKartWidget& player1, const PlayerKartWidget& player2)
{
    return player1.getKartInternalName() == player2.getKartInternalName() &&
    player1.getKartInternalName() != RANDOM_KART_ID;
}

#if 0
#pragma mark -
#pragma mark KartHoverListener
#endif
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
  
class KartHoverListener : public DynamicRibbonHoverListener
{
    KartSelectionScreen* m_parent;
public:
    unsigned int m_magic_number;

    KartHoverListener(KartSelectionScreen* parent)
    {
        m_magic_number = 0xCAFEC001;
        m_parent = parent;
    }
    
    virtual ~KartHoverListener()
    {
        assert(m_magic_number == 0xCAFEC001);
        m_magic_number = 0xDEADBEEF;
    }
    
    void onSelectionChanged(DynamicRibbonWidget* theWidget, const std::string& selectionID,
                            const irr::core::stringw& selectionText, const int playerID)
    {
        assert(m_magic_number == 0xCAFEC001);

        // Don't allow changing the selection after confirming it
        if (m_parent->m_kart_widgets[playerID].isReady())
        {
            // discard events sent when putting back to the right kart
            if (selectionID == m_parent->m_kart_widgets[playerID].m_kartInternalName) return;
            
            DynamicRibbonWidget* w = m_parent->getWidget<DynamicRibbonWidget>("karts");
            assert(w != NULL);
            
            w->setSelection(m_parent->m_kart_widgets[playerID].m_kartInternalName, playerID, true);
            return;
        }
        
        // Update the displayed model
        ModelViewWidget* w3 = m_parent->m_kart_widgets[playerID].modelView;
        assert( w3 != NULL );
        
        if (selectionID == RANDOM_KART_ID)
        {
            // Random kart
            scene::IMesh* model = item_manager->getItemModel(Item::ITEM_BONUS_BOX);
            w3->clearModels();
            w3->addModel( model, Vec3(0.0f, -12.0f, 0.0f) );
            w3->update(0);
            m_parent->m_kart_widgets[playerID].kartName->setText( _("Random Kart") );
        }
        else
        {
            //printf("%s\n", selectionID.c_str());
            const KartProperties* kart = kart_properties_manager->getKart(selectionID);
            if (kart == NULL) return;
            KartModel* kartModel = kart->getKartModel();
            
            w3->clearModels();
            w3->addModel( kartModel->getModel(), Vec3(0,0,0), kartModel->getBaseFrame() );
            w3->addModel( kartModel->getWheelModel(0), kartModel->getWheelGraphicsPosition(0) );
            w3->addModel( kartModel->getWheelModel(1), kartModel->getWheelGraphicsPosition(1) );
            w3->addModel( kartModel->getWheelModel(2), kartModel->getWheelGraphicsPosition(2) );
            w3->addModel( kartModel->getWheelModel(3), kartModel->getWheelGraphicsPosition(3) );
            w3->update(0);

            m_parent->m_kart_widgets[playerID].kartName->setText( selectionText.c_str() );
        }

        m_parent->m_kart_widgets[playerID].setKartInternalName(selectionID);
        m_parent->validateKartChoices();
    }
};
KartHoverListener* karthoverListener = NULL;

#if 0
#pragma mark -
#pragma mark KartSelectionScreen
#endif
    
// -----------------------------------------------------------------------------
KartSelectionScreen::KartSelectionScreen() : Screen("karts.stkgui")
{
    g_dispatcher = new FocusDispatcher(this);
    m_player_confirmed = false;
    
    // Dynamically add tabs
    RibbonWidget* tabs = this->getWidget<RibbonWidget>("kartgroups");
    assert( tabs != NULL );
    
    tabs->clearAllChildren();
    
    const std::vector<std::string>& groups = kart_properties_manager->getAllGroups();
    
    const int group_amount = groups.size();
    for (int n=0; n<group_amount; n++)
    {
        //FIXME: group name not translated
        tabs->addTextChild( stringw(groups[n].c_str()).c_str() , groups[n]);
    }
    
    if (group_amount > 1)
    {
        //I18N: name of the tab that will show tracks from all groups
        tabs->addTextChild( _("All") , ALL_KART_GROUPS_ID);
    }
}

// ----------------------------------------------------------------------------- 

void KartSelectionScreen::init()
{
    m_player_confirmed = false;
    
    RibbonWidget* tabs = this->getWidget<RibbonWidget>("kartgroups");
    assert( tabs != NULL );
    tabs->m_deactivated = false;
    
    // FIXME: Reload previous kart selection screen state
    m_kart_widgets.clearAndDeleteAll();
    StateManager::get()->resetActivePlayers();
    input_manager->getDeviceList()->setAssignMode(DETECT_NEW);
    
    DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
    assert( w != NULL );
    
    if (karthoverListener == NULL)
    {
        karthoverListener = new KartHoverListener(this);
        w->registerHoverListener(karthoverListener);
    }
    
    // Build kart list (it is built everytime, to account for .g. locking)
    setKartsFromCurrentGroup();
    
    /*
     
     TODO: Ultimately, it'd be nice to *not* delete g_player_karts so that
     when players return to the kart selection screen, it will appear as
     it did when they left (at least when returning from the track menu).
     Rebuilding the screen is a little tricky.
     
     */
    
    if (m_kart_widgets.size() > 0)
    {
        // FIXME: trying to rebuild the screen
        for (int n = 0; n < m_kart_widgets.size(); n++)
        {
            PlayerKartWidget *pkw;
            pkw = m_kart_widgets.get(n);
            this->manualAddWidget(pkw);
            pkw->add();
        }
        
    }
    else // For now this is what will happen
    {
        playerJoin( input_manager->getDeviceList()->getLatestUsedDevice(), true );
        w->updateItemDisplay();
    }
    
    // Player 0 select default kart
    w->setSelection(UserConfigParams::m_default_kart, 0, true);
}

// -----------------------------------------------------------------------------

void KartSelectionScreen::tearDown()
{
    //g_player_karts.clearWithoutDeleting();
    m_kart_widgets.clearAndDeleteAll();
}

// -----------------------------------------------------------------------------

void KartSelectionScreen::forgetWhatWasLoaded()
{    
    Screen::forgetWhatWasLoaded();
    
    // these pointers are no more valid (have been deleted along other widgets)
    g_dispatcher = NULL;
    karthoverListener = NULL;
}

// -----------------------------------------------------------------------------
// Return true if event was handled successfully
bool KartSelectionScreen::playerJoin(InputDevice* device, bool firstPlayer)
{
    if (UserConfigParams::m_verbosity>=5) std::cout << "playerJoin() ==========\n";

    if (g_dispatcher == NULL)
    {
        g_dispatcher = new FocusDispatcher(this);
    }
    
    DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
    if (w == NULL)
    {
        std::cerr << "playerJoin(): Called outside of kart selection screen.\n";
        return false;
    }
    else if (device == NULL)
    {
        std::cerr << "playerJoin(): Received null device pointer\n";
        return false;
    }

    if (StateManager::get()->activePlayerCount() >= MAX_PLAYER_COUNT)
    {
        std::cerr << "Maximum number of players reached\n";
        sfx_manager->quickSound( "use_anvil" );
        return false;
    }
    
    // ---- Get available area for karts
    // make a copy of the area, ands move it to be outside the screen
    Widget kartsArea = *this->getWidget("playerskarts"); // copy
    kartsArea.x = irr_driver->getFrameSize().Width; // start at the rightmost of the screen
    
    // ---- Create new active player
    PlayerProfile* profileToUse = UserConfigParams::m_all_players.get(0);
    
    if (!firstPlayer)
    {
        const int playerProfileCount = UserConfigParams::m_all_players.size();
        for (int n=0; n<playerProfileCount; n++)
        {
            if (UserConfigParams::m_all_players[n].isGuestAccount())
            {
                profileToUse = UserConfigParams::m_all_players.get(n);
                break;
            }
        }
    }
    
    const int new_player_id = StateManager::get()->createActivePlayer( profileToUse, device );
    StateManager::ActivePlayer* aplayer = StateManager::get()->getActivePlayer(new_player_id);
    
    // ---- Create focus dispatcher
    if (firstPlayer)
    {
        g_dispatcher->setRootID(kartsArea.m_reserved_id);
        g_dispatcher->add();
        if (!m_widgets.contains(g_dispatcher)) m_widgets.push_back(g_dispatcher);

        // We keep the ID of the "root" widget, see comment at top
        g_root_id = kartsArea.m_reserved_id;
        //std::cout << "++++ root ID : " << g_root_id << std::endl;
    }
    
    // ---- Create player/kart widget
    PlayerKartWidget* newPlayerWidget = new PlayerKartWidget(this, aplayer, &kartsArea, m_kart_widgets.size());

    this->manualAddWidget(newPlayerWidget);
    newPlayerWidget->add();
    
    m_kart_widgets.push_back(newPlayerWidget);
    
    // ---- Divide screen space among all karts
    const int amount = m_kart_widgets.size();
    Widget* fullarea = this->getWidget("playerskarts");
    const int splitWidth = fullarea->w / amount;
    
    for (int n=0; n<amount; n++)
    {
        m_kart_widgets[n].move( fullarea->x + splitWidth*n, fullarea->y, splitWidth, fullarea->h );
    }
    
    
    if (!firstPlayer)
    {
        // select something (anything) in the ribbon so that the selection array does not contain
        // garbage (FIXME: the ribbon should handle this internally!)
        w->setSelection(new_player_id, new_player_id, true);
            
        newPlayerWidget->playerName->setFocusForPlayer(new_player_id);
    }
    
    return true;
}

PlayerKartWidget* removedWidget = NULL;

// -----------------------------------------------------------------------------
// Returns true if event was handled succesfully
bool KartSelectionScreen::playerQuit(StateManager::ActivePlayer* player)
{    
    int playerID = -1;
    
    DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
    if (w == NULL)
    {
        std::cout << "playerQuit() called outside of kart selection screen.\n";
        return false;
    }

    // If last player quits, return to main menu
    if (m_kart_widgets.size() <= 1) return false;

    // Find the player ID associated to this player
    for (int n=0; n<m_kart_widgets.size(); n++)
    {
        if (m_kart_widgets[n].getAssociatedPlayer() == player)
        {
            // Check that this player has not already confirmed, then they can't back out
            if (m_kart_widgets[n].isReady())
            {
                sfx_manager->quickSound( "use_anvil" );
                return true;
            }
            
            playerID = n;
            break;
        }
    }
    if (playerID == -1)
    {
        std::cout << "void playerQuit(ActivePlayer* player) : cannot find passed player\n";
        return false;
    }
    if(UserConfigParams::m_verbosity>=5)
        std::cout << "playerQuit( " << playerID << " )\n";

    // Just a cheap way to check if there is any discrepancy 
    // between g_player_karts and the active player array
    assert( m_kart_widgets.size() == StateManager::get()->activePlayerCount() );

    // unset selection of this player
    GUIEngine::focusNothingForPlayer(playerID);
    
    // keep the removed kart a while, for the 'disappear' animation to take place
    removedWidget = m_kart_widgets.remove(playerID);
    
    // Tell the StateManager to remove this player
    StateManager::get()->removeActivePlayer(playerID);
    
    // Karts count changed, maybe order too, so renumber them. Also takes
    // care of updating selections.
    renumberKarts();
    
    // Tell the removed widget to perform the shrinking animation (which will be updated in onUpdate,
    // and will stop when the widget has disappeared)
    Widget* fullarea = this->getWidget("playerskarts");
    removedWidget->move( removedWidget->x + removedWidget->w/2, fullarea->y + fullarea->h, 0, 0);
    
    // check if all players are ready
    bool allPlayersReady = true;
    const int amount = m_kart_widgets.size();
    for (int n=0; n<amount; n++)
    {            
        if (!m_kart_widgets[n].isReady())
        {
            allPlayersReady = false;
            break;
        }
    }
    if (allPlayersReady) allPlayersDone();
    
    return true;
}
    
// -----------------------------------------------------------------------------
    
void KartSelectionScreen::onUpdate(float delta, irr::video::IVideoDriver*) 
{
    // Dispatch the onUpdate event to each kart, so they can perform their animation if any
    const int amount = m_kart_widgets.size();
    for (int n=0; n<amount; n++)
    {
        m_kart_widgets[n].onUpdate(delta);
    }
    
    // When a kart widget is removed, it's a kept a while, for the disappear animation to take place
    if (removedWidget != NULL)
    {
        removedWidget->onUpdate(delta);
       
        if (removedWidget->w == 0 || removedWidget->h == 0)
        {
            // destruct when too small (for "disappear" effects)
            this->manualRemoveWidget(removedWidget);
            delete removedWidget;
            removedWidget = NULL;
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * Callback handling events from the kart selection menu
 */
void KartSelectionScreen::eventCallback(Widget* widget, const std::string& name, const int playerID)
{
    if (name == "kartgroups" && !m_player_confirmed) // don't allow changing group after someone confirmed
    {
        RibbonWidget* tabs = this->getWidget<RibbonWidget>("kartgroups");
        assert(tabs != NULL);
        DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
        assert(w != NULL);
        
        setKartsFromCurrentGroup();

        const std::string selected_kart_group = tabs->getSelectionIDString(PLAYER_ID_GAME_MASTER);
                       
        // update players selections (FIXME: don't hardcode player 0 below)
        const int num_players = m_kart_widgets.size();
        for (int n=0; n<num_players; n++)
        {
            // player 0 is the one that can change the groups, leave his focus on the tabs
            // for others, remove focus from kart that might no more exist in this tab.
            if (n > 0) GUIEngine::focusNothingForPlayer(n);
            
            // try to preserve the same kart for each player (except for player 0, since it's the one 
            // that can change the groups, so focus for player 0 must remain on the tabs)
            const std::string& selected_kart_group = m_kart_widgets[n].getKartInternalName();
            if (!w->setSelection( selected_kart_group, n, n>0 ))
            {
                // if we get here, it means one player "lost" his kart in the tab switch
                std::cout << "Player " << n << " lost their selection when switching tabs!!!\n";
                
                // Select a random kart in this case
                RandomGenerator random;
                const int count = w->getItems().size();
                if (count > 0)
                {
                    const int randomID = random.get( count );
                    
                    // select kart for players > 0 (player 0 is the one that can change the groups,
                    // so focus for player 0 must remain on the tabs)
                    w->setSelection( randomID, n, n>0 );
                }
                else
                {
                    std::cerr << "WARNING : kart selection screen has 0 items in the ribbon\n";
                }
            }
        } // end for
    }
    else if (name == "karts")
    {
        DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
        assert(w != NULL);
        const std::string selection = w->getSelectionIDString(playerID);
        if (selection == "locked")
        {
            unlock_manager->playLockSound();
            return;
        }
        
        // make sure no other player selected the same identity or kart
        const int amount = m_kart_widgets.size();
        for (int n=0; n<amount; n++)
        {
            if (n == playerID) continue; // don't check a kart against itself
            
            const bool player_ready   = m_kart_widgets[n].isReady();
            const bool ident_conflict = !m_kart_widgets[n].getAssociatedPlayer()->getProfile()->isGuestAccount() &&
                                        m_kart_widgets[n].getAssociatedPlayer()->getProfile() ==
                                        m_kart_widgets[playerID].getAssociatedPlayer()->getProfile();
            const bool kart_conflict  = sameKart(m_kart_widgets[n], m_kart_widgets[playerID]);
            
            if (player_ready && (ident_conflict || kart_conflict))
            {
                printf("\n***\n*** You can't select this identity or kart, someone already took it!! ***\n***\n\n");
                
                sfx_manager->quickSound( "use_anvil" );
                return;
            }
            
            // If two PlayerKart entries are associated to the same ActivePlayer, something went wrong
            assert(m_kart_widgets[n].getAssociatedPlayer() != m_kart_widgets[playerID].getAssociatedPlayer());
        }
        
        // Mark this player as ready to start
        m_kart_widgets[playerID].markAsReady();
        m_player_confirmed = true;
        
        RibbonWidget* tabs = this->getWidget<RibbonWidget>("kartgroups");
        assert( tabs != NULL );
        tabs->m_deactivated = true;
        
        // validate choices to notify player of duplicates
        const bool names_ok = validateIdentChoices();
        const bool karts_ok = validateKartChoices();
        
        if (!names_ok || !karts_ok) return;
        
        // check if all players are ready
        bool allPlayersReady = true;
        for (int n=0; n<amount; n++)
        {            
            if (!m_kart_widgets[n].isReady())
            {
                allPlayersReady = false;
                break;
            }
        }
        
        if (allPlayersReady) allPlayersDone();
    }
    else
    {
        // Transmit to all subwindows, maybe *they* care about this event
        //FIXME: this may now be done automatically by the caller, so maybe I nee to remove this
        const int amount = m_kart_widgets.size();
        for (int n=0; n<amount; n++)
        {
            m_kart_widgets[n].transmitEvent(widget, name, playerID);
        }
        
        // those events may mean that a player selection changed, so validate again
        validateIdentChoices();
        validateKartChoices();
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark KartSelectionScreen (private)
#endif

void KartSelectionScreen::allPlayersDone()
{        
    input_manager->setMasterPlayerOnly(true);
    
    DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
    assert( w != NULL );
    
    const ptr_vector< StateManager::ActivePlayer, HOLD >& players = StateManager::get()->getActivePlayers();
    
    // ---- Print selection (for debugging purposes)
    if(UserConfigParams::m_verbosity>=4)
    {
        std::cout << "==========\n" << players.size() << " players :\n";
        for (int n=0; n<players.size(); n++)
        {
            std::cout << "     Player " << n << " is " << players[n].getConstProfile()->getName()
                << " on " << players[n].getDevice()->m_name << std::endl;
        }
        std::cout << "==========\n";
    }
    
    for (int n=0; n<players.size(); n++)
    {
        StateManager::get()->getActivePlayer(n)->getProfile()->m_use_frequency++;
    }
    // ---- Give player info to race manager
    race_manager->setNumPlayers( players.size() );
    race_manager->setNumLocalPlayers( players.size() );
    
    // ---- Manage 'random kart' selection(s)
    RandomGenerator random;
    
    //g_player_karts.clearAndDeleteAll();      
    //race_manager->setLocalKartInfo(0, w->getSelectionIDString());
    
    std::vector<ItemDescription> items = w->getItems();
    
    // remove the 'random' item itself
    const int item_count = items.size();
    for (int n=0; n<item_count; n++)
    {
        if (items[n].m_code_name == RANDOM_KART_ID)
        {
            items[n].m_code_name = ID_DONT_USE;
            break;
        }
    }
    
    // pick random karts
    const int kart_count = m_kart_widgets.size();
    for (int n = 0; n < kart_count; n++)
    {
        std::string selected_kart_group = m_kart_widgets[n].m_kartInternalName;
        
        if (selected_kart_group == RANDOM_KART_ID)
        {
            // don't select an already selected kart
            int randomID;
            bool done = false;
            do
            {
                randomID = random.get(item_count);
                if (items[randomID].m_code_name != ID_DONT_USE)
                {
                    selected_kart_group = items[randomID].m_code_name;
                    done = true;
                }
                items[randomID].m_code_name = ID_DONT_USE;
            } while (!done);
        }
        else
        {
            // mark the item as taken
            for (int i=0; i<item_count; i++)
            {
                if (items[i].m_code_name == items[n].m_code_name)
                {
                    items[i].m_code_name = ID_DONT_USE;
                    break;
                }
            }
        }
        // std::cout << "selection=" << selection.c_str() << std::endl;
        
        race_manager->setLocalKartInfo(n, selected_kart_group);
    }
    
    // ---- Switch to assign mode
    input_manager->getDeviceList()->setAssignMode(ASSIGN);
    
    StateManager::get()->pushScreen( RaceSetupScreen::getInstance() );
}

// -----------------------------------------------------------------------------
bool KartSelectionScreen::validateIdentChoices()
{
    bool ok = true;
    
    const int amount = m_kart_widgets.size();
    
    // reset all marks, we'll re-add them next if errors are still there
    for (int n=0; n<amount; n++)
    {
        // first check if the player name widget is still there, it won't be for those that confirmed
        if (m_kart_widgets[n].playerName != NULL)
        {
            m_kart_widgets[n].playerName->markAsCorrect();
            
            // verify internal consistency in debug mode
            assert( m_kart_widgets[n].getAssociatedPlayer()->getProfile() ==
                   UserConfigParams::m_all_players.get(m_kart_widgets[n].playerName->getValue()) );
        }
    }
    
    // perform actual checking
    for (int n=0; n<amount; n++)
    {        
        // skip players that took a guest account, they can be many on the same identity in this case
        if (m_kart_widgets[n].getAssociatedPlayer()->getProfile()->isGuestAccount())
        {
            continue;
        }

        // check if another kart took the same identity as the current one
        for (int m=n+1; m<amount; m++)
        {            
                
            // check if 2 players took the same name
            if (m_kart_widgets[n].getAssociatedPlayer()->getProfile() ==
                m_kart_widgets[m].getAssociatedPlayer()->getProfile())
            {
                printf("\n***\n*** Identity conflict!! ***\n***\n\n");
                std::cout << " Player " << n << " chose " << m_kart_widgets[n].getAssociatedPlayer()->getProfile()->getName() << std::endl;
                std::cout << " Player " << m << " chose " << m_kart_widgets[m].getAssociatedPlayer()->getProfile()->getName() << std::endl;
                
                // two players took the same name. check if one is ready
                if (!m_kart_widgets[n].isReady() && m_kart_widgets[m].isReady())
                {
                    // player m is ready, so player n should not choose this name
                    m_kart_widgets[n].playerName->markAsIncorrect();
                }
                else if (m_kart_widgets[n].isReady() && !m_kart_widgets[m].isReady())
                {
                    // player n is ready, so player m should not choose this name
                    m_kart_widgets[m].playerName->markAsIncorrect();
                }
                else if (m_kart_widgets[n].isReady() && m_kart_widgets[m].isReady())
                {
                    // it should be impossible for two players to confirm they're ready with the same name
                    assert(false);
                }
                
                ok = false;
            }
        } // end for
    }
    
    return ok;
}

// -----------------------------------------------------------------------------

bool KartSelectionScreen::validateKartChoices()
{
    bool ok = true;
    
    const int amount = m_kart_widgets.size();
    
    // reset all marks, we'll re-add them next if errors are still there
    for (int n=0; n<amount; n++)
    {
        m_kart_widgets[n].modelView->unsetBadge(BAD_BADGE);
    }
    
    for (int n=0; n<amount; n++)
    {        
        for (int m=n+1; m<amount; m++)
        {
            // check if 2 players took the same name
            if (sameKart(m_kart_widgets[n], m_kart_widgets[m]))
            {
                printf("\n***\n*** Kart conflict!! ***\n***\n\n");
                std::cout << " Player " << n << " chose " << m_kart_widgets[n].getKartInternalName() << std::endl;
                std::cout << " Player " << m << " chose " << m_kart_widgets[m].getKartInternalName() << std::endl;
                
                // two players took the same kart. check if one is ready
                if (!m_kart_widgets[n].isReady() && m_kart_widgets[m].isReady())
                {
                    std::cout << "--> Setting red badge on player " << n << std::endl;
                    // player m is ready, so player n should not choose this name
                    m_kart_widgets[n].modelView->setBadge(BAD_BADGE);
                }
                else if (m_kart_widgets[n].isReady() && !m_kart_widgets[m].isReady())
                {
                    std::cout << "--> Setting red badge on player " << m << std::endl;
                    // player n is ready, so player m should not choose this name
                    m_kart_widgets[m].modelView->setBadge(BAD_BADGE);
                }
                else if (m_kart_widgets[n].isReady() && m_kart_widgets[m].isReady())
                {
                    // it should be impossible for two players to confirm they're ready with the same kart
                    assert(false);
                }
                
                // we know it's not ok (but don't stop right now, all bad ones need red badges)
                ok = false;
            }
        } // end for
    }
    
    return ok;
    
}

// -----------------------------------------------------------------------------
    
void KartSelectionScreen::renumberKarts()
{
    DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
    assert( w != NULL );
    Widget* fullarea = this->getWidget("playerskarts");
    const int splitWidth = fullarea->w / m_kart_widgets.size();

    printf("Renumbering karts...");
    for (int n=0; n < m_kart_widgets.size(); n++)
    {
        m_kart_widgets[n].setPlayerID(n);
        m_kart_widgets[n].move( fullarea->x + splitWidth*n, fullarea->y, splitWidth, fullarea->h );
    }

    w->updateItemDisplay();
    printf("OK\n");

}

// -----------------------------------------------------------------------------

void KartSelectionScreen::setKartsFromCurrentGroup()
{
    RibbonWidget* tabs = this->getWidget<RibbonWidget>("kartgroups");
    assert(tabs != NULL);
    
    const std::string selected_kart_group = tabs->getSelectionIDString(PLAYER_ID_GAME_MASTER);
    
    DynamicRibbonWidget* w = this->getWidget<DynamicRibbonWidget>("karts");
    w->clearItems();
    
    // FIXME: merge this code with the code that adds karts initially, copy-and-paste is ugly
    
    if (selected_kart_group == ALL_KART_GROUPS_ID)
    {
        const int kart_amount = kart_properties_manager->getNumberOfKarts();
        
        for (int n=0; n<kart_amount; n++)
        {
            const KartProperties* prop = kart_properties_manager->getKartById(n);
            std::string icon_path = "/karts/" + prop->getIdent() + "/" + prop->getIconFile();

            if (unlock_manager->isLocked(prop->getIdent()))
            {
                w->addItem( _("Locked : solve active challenges to gain access to more!"),
                           "locked", icon_path, LOCKED_BADGE);
            }
            else
            {
                w->addItem(prop->getName().c_str(), prop->getIdent().c_str(), icon_path.c_str());
            }
        }
    }
    //FIXME: what does this do there???
    else if (selected_kart_group == "locked")
    {
        unlock_manager->playLockSound();
    }
    else if (selected_kart_group != DynamicRibbonWidget::NO_ITEM_ID)
    {        
        std::vector<int> group = kart_properties_manager->getKartsInGroup(selected_kart_group);
        const int kart_amount = group.size();
        
        
        for (int n=0; n<kart_amount; n++)
        {
            const KartProperties* prop = kart_properties_manager->getKartById(group[n]);
            std::string icon_path = "/karts/" + prop->getIdent() + "/" + prop->getIconFile();

            if (unlock_manager->isLocked(prop->getIdent()))
            {
                w->addItem( _("Locked : solve active challenges to gain access to more!"),
                            "locked", icon_path, LOCKED_BADGE);
            }
            else
            {
                w->addItem(prop->getName().c_str(), prop->getIdent().c_str(), icon_path.c_str());
            }
        }
    }
    // add random
    w->addItem(_("Random Kart"), RANDOM_KART_ID, "/gui/random_kart.png");
    
    w->updateItemDisplay();    
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#if 0
#pragma mark -
#endif

// FIXME : clean this mess, this file should not contain so many classes spread all over the place
EventPropagation FocusDispatcher::focused(const int playerID)
{ 
    if(UserConfigParams::m_verbosity>=5)
        std::cout << "FocusDispatcher focused by player " << playerID << std::endl;
    
    // since this screen is multiplayer, redirect focus to the right widget
    const int amount = m_parent->m_kart_widgets.size();
    for (int n=0; n<amount; n++)
    {
        if (m_parent->m_kart_widgets[n].getPlayerID() == playerID)
        {
            // If player is done, don't do anything with focus
            if (m_parent->m_kart_widgets[n].isReady()) return GUIEngine::EVENT_BLOCK;
            
            //std::cout << "--> Redirecting focus for player " << playerID << " from FocusDispatcher "  <<
            //             " (ID " << m_element->getID() <<
            //             ") to spinner " << n << " (ID " <<
            //             m_parent->m_kart_widgets[n].playerName->getIrrlichtElement()->getID() << 
            //             ")" << std::endl;
            
            m_parent->m_kart_widgets[n].playerName->setFocusForPlayer(playerID);
            

            return GUIEngine::EVENT_BLOCK;
        }
    }
    
    assert(false);
    return GUIEngine::EVENT_LET;        
}

