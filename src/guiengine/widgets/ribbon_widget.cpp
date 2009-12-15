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

#include "guiengine/widgets/ribbon_widget.hpp"

#include "guiengine/engine.hpp"
#include "input/input_manager.hpp"
#include "io/file_manager.hpp"
using namespace GUIEngine;
using namespace irr::core;
using namespace irr::gui;

#ifndef round
#  define round(x)  (floor(x+0.5f))
#endif

// -----------------------------------------------------------------------------
RibbonWidget::RibbonWidget(const RibbonType type)
{
    for (int n=0; n<MAX_PLAYER_COUNT; n++)
    {
        m_selection[n] = -1;
    }
    m_selection[0] = 0; // only player 0 has a selection by default
    
    m_ribbon_type = type;
    m_mouse_focus = NULL;
    updateSelection();
    m_type = WTYPE_RIBBON;
}
// -----------------------------------------------------------------------------
void RibbonWidget::add()
{
    m_labels.clearWithoutDeleting();
    
    
    rect<s32> widget_size = rect<s32>(x, y, x + w, y + h);
    
    int id = (m_reserved_id == -1 ? getNewID() : m_reserved_id);
    
    IGUIButton * btn = GUIEngine::getGUIEnv()->addButton(widget_size, m_parent, id, L"");
    m_element = btn;
    
    const int subbuttons_amount = m_children.size();
    
    // ---- check how much space each child button will take and fit them within available space
    int total_needed_space = 0;
    for (int i=0; i<subbuttons_amount; i++)
    {
        m_children[i].readCoords(this);
        
        if (m_children[i].m_type != WTYPE_ICON_BUTTON && m_children[i].m_type != WTYPE_BUTTON)
        {
            std::cerr << "/!\\ Warning /!\\ : ribbon widgets can only have (icon)button widgets as children " << std::endl;
            continue;
        }
        
        // ribbon children must not be keyboard navigatable, the parent ribbon takes care of that
        if (m_children[i].m_type == WTYPE_ICON_BUTTON)
        {
            ((IconButtonWidget*)m_children.get(i))->m_tab_stop = false;
        }
        
        total_needed_space += m_children[i].w;
    }
    
    int free_h_space = w - total_needed_space;
    
    //int biggest_y = 0;
    const int button_y = 10;
    float global_zoom = 1;
    
    const int min_free_space = 50;
    global_zoom = (float)w / (float)( w - free_h_space + min_free_space );
    free_h_space = (int)(w - total_needed_space*global_zoom);
    
    const int one_button_space = (int)round((float)w / (float)subbuttons_amount);
    
    // ---- add children
    for (int i=0; i<subbuttons_amount; i++)
    {
        
        const int widget_x = one_button_space*(i+1) - one_button_space/2;
        
        if (getRibbonType() == RIBBON_TABS)
        {
            IGUIButton * subbtn;
            rect<s32> subsize = rect<s32>(widget_x - one_button_space/2+2,  0,
                                          widget_x + one_button_space/2-2,  h);
            
            stringw& message = m_children[i].m_text;
            
            if (m_children[i].m_type == WTYPE_BUTTON)
            {
                subbtn = GUIEngine::getGUIEnv()->addButton(subsize, btn, getNewNoFocusID(), message.c_str(), L"");
                subbtn->setTabStop(false);
                subbtn->setTabGroup(false);
            }
            else if (m_children[i].m_type == WTYPE_ICON_BUTTON)
            {
                rect<s32> icon_part = rect<s32>(15,
                                                0,
                                                subsize.getHeight()+15,
                                                subsize.getHeight());
                rect<s32> label_part = rect<s32>(subsize.getHeight()+15,
                                                 0,
                                                 subsize.getWidth()-15,
                                                 subsize.getHeight());
                
                // use the same ID for all subcomponents; since event handling is done per-ID, no matter
                // which one your hover, this widget will get it
                int same_id = getNewNoFocusID();
                subbtn = GUIEngine::getGUIEnv()->addButton(subsize, btn, same_id, L"", L"");
                
                //MyGUIButton* icon = new MyGUIButton(GUIEngine::getGUIEnv(), subbtn, same_id, icon_part, true);
                IGUIButton* icon = GUIEngine::getGUIEnv()->addButton(icon_part, subbtn, same_id, L"");
                icon->setScaleImage(true);
                icon->setImage( GUIEngine::getDriver()->getTexture((file_manager->getDataDir() + "/" + m_children[i].m_properties[PROP_ICON]).c_str()) );
                icon->setUseAlphaChannel(true);
                icon->setDrawBorder(false);
                icon->setTabStop(false);
                
                IGUIStaticText* label = GUIEngine::getGUIEnv()->addStaticText(message.c_str(), label_part,
                                                                              false /* border */,
                                                                              true /* word wrap */,
                                                                              subbtn, same_id);
                label->setTextAlignment(EGUIA_CENTER, EGUIA_CENTER);
                label->setTabStop(false);
                label->setNotClipped(true);
                m_labels.push_back(label);
                
                subbtn->setTabStop(false);
                subbtn->setTabGroup(false);
                
            }
            
            m_children[i].m_element = subbtn;
        }
        // ---- non-tabs ribbons
        else if (m_children[i].m_type == WTYPE_ICON_BUTTON)
        {
            // how much space to keep for the label under the button
            const bool has_label = m_children[i].m_text.size() > 0;
            const int needed_space_under_button = has_label ? 30 : 10; // quite arbitrary for now
            
            // For now, the image stretches to keep the aspect ratio of the widget (FIXME, doesn't work)
            //float imageRatio = (float)m_children[i].w/(float)m_children[i].h;
            
            // size of the image
            video::ITexture* image = GUIEngine::getDriver()->getTexture((file_manager->getDataDir() + "/" + m_children[i].m_properties[PROP_ICON]).c_str());
            float image_h = image->getSize().Height;
            float image_w = image->getSize().Width;
            //float image_w = image_h*imageRatio;

            // if button too high to fit, scale down
            float zoom = global_zoom;
            while (button_y + image_h*zoom + needed_space_under_button > h) zoom -= 0.01f;
            
            // ---- add bitmap button part
            //rect<s32> subsize = rect<s32>(widget_x - (int)(image_w/2.0f), button_y,
            //                              widget_x + (int)(image_w/2.0f), button_y + (int)(m_children[i].h*zoom));
            
            m_children[i].x = widget_x - (int)(image_w*zoom/2.0f);
            m_children[i].y = button_y;
            m_children[i].w = image_w*zoom;
            m_children[i].h = image_h*zoom;

            //std::wcout << L"Widget has text '" << m_children[i].m_text.c_str() << "'\n";
            
            
            m_children.get(i)->m_parent = btn;
            m_children.get(i)->add();
            //subbtn->setUseAlphaChannel(true);
            //subbtn->setImage( GUIEngine::getDriver()->getTexture((file_manager->getDataDir() + "/" + m_children[i].m_properties[PROP_ICON]).c_str()) );
            
            // ---- label part
            /*
            if (has_label)
            {
                subsize = rect<s32>(widget_x - one_button_space/2,
                                    (int)((button_y + m_children[i].h)*zoom) + 5, // leave 5 pixels between button and label
                                    widget_x + (int)(one_button_space/2.0f), h);
                
                stringw& message = m_children[i].m_text;
                IGUIStaticText* label = GUIEngine::getGUIEnv()->addStaticText(message.c_str(), subsize, false, true, btn);
                label->setTextAlignment(EGUIA_CENTER, EGUIA_CENTER);
                label->setTabStop(false);
                label->setNotClipped(true);
                
                m_labels.push_back(label);
                
                const int final_y = subsize.getHeight() + label->getTextHeight();
                if(final_y > biggest_y) biggest_y = final_y;
            }
            */
            //subbtn->setTabStop(false);
            //subbtn->setTabGroup(false);
        }
        else
        {
            std::cerr << "/!\\ Warning /!\\ : Invalid contents type in ribbon" << std::endl;
        }
        
        
        //m_children[i].id = subbtn->getID();
        m_children[i].m_event_handler = this;
    }// next sub-button
    
    id = m_element->getID();
    m_element->setTabOrder(id);
    m_element->setTabGroup(false);
    updateSelection();
}

// -----------------------------------------------------------------------------
void RibbonWidget::select(std::string item, const int mousePlayerID)
{
    const int subbuttons_amount = m_children.size();
    
    for (int i=0; i<subbuttons_amount; i++)
    {
        if (m_children[i].m_properties[PROP_ID] == item)
        {
            m_selection[mousePlayerID] = i;
            updateSelection();
            return;
        }
    }
    
}
// -----------------------------------------------------------------------------
EventPropagation RibbonWidget::rightPressed(const int playerID)
{
    m_selection[playerID]++;
    if (m_selection[playerID] >= m_children.size())
    {
        if (m_event_handler != NULL)
        {
            ((DynamicRibbonWidget*)m_event_handler)->scroll(1); // FIXME? - find cleaner way to propagate event to parent
            m_selection[playerID] = m_children.size()-1;
        }
        else m_selection[playerID] = 0;
    }
    updateSelection();
    
    if (m_ribbon_type == RIBBON_COMBO)
    {
        const int mousePlayerID = input_manager->getPlayerKeyboardID();
        const int MASTER_PLAYER = 0; // FIXME: unclean
        if (playerID == mousePlayerID || playerID == MASTER_PLAYER)
        {
            m_mouse_focus = m_children.get(m_selection[playerID]);
        }
    }
    
    if (m_ribbon_type != RIBBON_TOOLBAR)
    {
        //GUIEngine::transmitEvent( this, m_properties[PROP_ID], playerID );
        return EVENT_LET;
    }
    else
    {
        return EVENT_BLOCK;
    }
}
// -----------------------------------------------------------------------------
EventPropagation RibbonWidget::leftPressed(const int playerID)
{
    m_selection[playerID]--;
    if (m_selection[playerID] < 0)
    {
        if (m_event_handler != NULL)
        {
            ((DynamicRibbonWidget*)m_event_handler)->scroll(-1); // FIXME? - find cleaner way to propagate event to parent
            m_selection[playerID] = 0;
        }
        else m_selection[playerID] = m_children.size()-1;
    }
    updateSelection();
    
    if (m_ribbon_type == RIBBON_COMBO)
    {
        const int mousePlayerID = input_manager->getPlayerKeyboardID();
        const int MASTER_PLAYER = 0; // FIXME: unclean
        if (playerID == mousePlayerID || playerID == MASTER_PLAYER)
        {
            m_mouse_focus = m_children.get(m_selection[playerID]);
        }
    }
    
    if (m_ribbon_type != RIBBON_TOOLBAR)
    {
        //GUIEngine::transmitEvent( this, m_properties[PROP_ID], playerID );
        return EVENT_LET;
    }
    else
    {
        return EVENT_BLOCK;
    }
}
// -----------------------------------------------------------------------------
EventPropagation RibbonWidget::focused(const int playerID)
{    
    Widget::focused(playerID);
    
    if (m_children.size() < 1) return EVENT_LET; // empty ribbon
    
    if (m_ribbon_type == RIBBON_COMBO)
    {
        const int mousePlayerID = input_manager->getPlayerKeyboardID();
        const int MASTER_PLAYER = 0; // FIXME: unclean
        if (m_mouse_focus == NULL && m_selection[playerID] != -1  && (playerID == mousePlayerID || playerID == MASTER_PLAYER))
        {
            m_mouse_focus = m_children.get(m_selection[playerID]);
        }
    }
    
    if (m_event_handler != NULL)
    {
        //m_focus->setFocusForPlayer( playerID );
                
        // FIXME : unclean, children ribbons shouldn't need to know about their parent
        ((DynamicRibbonWidget*)m_event_handler)->onRowChange( this, playerID );
    }
    
    return EVENT_LET;
}
// -----------------------------------------------------------------------------
EventPropagation RibbonWidget::mouseHovered(Widget* child, const int mousePlayerID)
{
    //std::cout << "RibbonWidget::mouseHovered " << mousePlayerID << std::endl;
    const int subbuttons_amount = m_children.size();
    
    if (m_ribbon_type == RIBBON_COMBO)
    {
        //std::cout << "SETTING m_mouse_focus\n";
        m_mouse_focus = child;
    }
    
    // In toolbar ribbons, hovering selects
    if (m_ribbon_type == RIBBON_TOOLBAR)
    {
        for (int i=0; i<subbuttons_amount; i++)
        {
            if (m_children.get(i) == child)
            {
                if (m_selection[mousePlayerID] == i) return EVENT_BLOCK; // was already selected, don't send another event
                m_selection[mousePlayerID] = i; // don't change selection on hover for others
                break;
            }
        }
    }
    
    updateSelection();
    return EVENT_BLOCK;
}

// -----------------------------------------------------------------------------
const std::string& RibbonWidget::getSelectionIDString(const int playerID)
{
    static std::string empty;
    if (m_selection[playerID] == -1) return empty;
    return m_children[m_selection[playerID]].m_properties[PROP_ID];
}

// -----------------------------------------------------------------------------
void RibbonWidget::updateSelection()
{
    const int subbuttons_amount = m_children.size();
    
    // FIXME: m_selection, m_selected, m_mouse_focus... what a mess...
    
    //std::cout << "----\n";
    // Update selection flags for mouse player
    for (int p=0; p<MAX_PLAYER_COUNT; p++)
    {
        for (int i=0; i<subbuttons_amount; i++)
        {
            m_children[i].m_selected[p] = (i == m_selection[p]);
            
            /*
            if (m_children[i].m_selected[p])
                std::cout << "m_children[" << i << "].m_selected[" << p << "] = " << m_children[i].m_selected[p] << std::endl;
            if (p == 0 && m_children.get(i) == m_mouse_focus)
                std::cout << "m_children[" << i << "] is mouse focus" << std::endl;
             */
        }
    }
    
    // Update the 'mouse focus' if necessary
    /*
    if (subbuttons_amount > 0 && m_ribbon_type == RIBBON_COMBO)
    {
        const int mousePlayerID = input_manager->getPlayerKeyboardID();
        if (mousePlayerID != -1 && m_selection[mousePlayerID] != -1)
        {
            m_mouse_focus = m_children.get(m_selection[mousePlayerID]);
            std::cout << "RESET mouse focus\n";
        }
    }
     */
}
// -----------------------------------------------------------------------------
EventPropagation RibbonWidget::transmitEvent(Widget* w, std::string& originator, const int playerID)
{
    const int subbuttons_amount = m_children.size();
    
    for (int i=0; i<subbuttons_amount; i++)
    {
        if (m_children[i].m_properties[PROP_ID] == originator)
        {
            m_selection[playerID] = i;
            break;
        }
    }
    
    updateSelection();
    
    // bring focus back to enclosing ribbon widget
    this->setFocusForPlayer( playerID );
    
    return EVENT_LET;
}
// -----------------------------------------------------------------------------
void RibbonWidget::setLabel(const int id, irr::core::stringw new_name)
{
    if (m_labels.size() == 0) return; // ignore this call for ribbons without labels
    
    assert(id >= 0);
    assert(id < m_labels.size());
    m_labels[id].setText( new_name.c_str() );
    m_text = new_name;
}
// -----------------------------------------------------------------------------
int RibbonWidget::findItemNamed(const char* internalName)
{    
    const int size = m_children.size();
    
    //printf("Ribbon : Looking for %s among %i items\n", internalName, size);
    
    for (int n=0; n<size; n++)
    {
        //printf("     Ribbon : Looking for %s in item %i : %s\n", internalName, n, m_children[n].m_properties[PROP_ID].c_str());
        if (m_children[n].m_properties[PROP_ID] == internalName) return n;
    }
    return -1;
}
