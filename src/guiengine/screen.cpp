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


#include <iostream>
#include <sstream>
#include <assert.h>

#include "irrlicht.h"

#include "input/input.hpp"
#include "io/file_manager.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/screen.hpp"
#include "guiengine/widget.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/translation.hpp"

using namespace irr;

using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;
using namespace GUIEngine;

Screen::Screen(const char* file)
{
    m_magic_number = 0xCAFEC001;

    m_throttle_FPS = true;
    
    this->m_filename = file;
    m_loaded = false;
    loadFromFile();
    m_render_3d = false;
}

Screen::Screen()
{
    m_magic_number = 0xCAFEC001;

    m_loaded = false;
    m_render_3d = false;
}

Screen::~Screen()
{
    assert(m_magic_number == 0xCAFEC001);
    m_magic_number = 0xDEADBEEF;
}

void Screen::forgetWhatWasLoaded()
{
    assert(m_magic_number == 0xCAFEC001);
    for (int n=0; n<m_widgets.size(); n++)
    {
        assert(m_widgets[n].m_magic_number == 0xCAFEC001);
    }
    
    m_loaded = false;
    m_widgets.clearAndDeleteAll();
}

#if 0
#pragma mark -
#pragma mark Load/Init
#endif

// -----------------------------------------------------------------------------
void Screen::loadFromFile()
{
    assert(m_magic_number == 0xCAFEC001);
    IrrXMLReader* xml = irr::io::createIrrXMLReader( (file_manager->getGUIDir() + "/" + m_filename).c_str() );
    parseScreenFileDiv(xml, m_widgets);
    m_loaded = true;
    calculateLayout();
}
// -----------------------------------------------------------------------------
/* small shortcut so this method can be called without arguments */
void Screen::calculateLayout()
{
    assert(m_magic_number == 0xCAFEC001);
    // build layout
    calculateLayout( m_widgets );
}
// -----------------------------------------------------------------------------
/*
 * Recursive call that lays out children widget within parent (or screen if none)
 * Manages 'horizontal-row' and 'vertical-row' layouts, along with the proportions
 * of the remaining children, as well as absolute sizes and locations.
 */
void Screen::calculateLayout(ptr_vector<Widget>& widgets, Widget* parent)
{
    assert(m_magic_number == 0xCAFEC001);
    const unsigned short widgets_amount = widgets.size();
    
    // ----- read x/y/size parameters
    for(unsigned short n=0; n<widgets_amount; n++)
    {
        widgets[n].readCoords(parent);
    }//next widget        
    
    // ----- manage 'layout's if relevant
    do // i'm using 'while false' here just to be able to 'break' ...
    {
        if(parent == NULL) break;
        
        std::string layout_name = parent->m_properties[PROP_LAYOUT];
        if(layout_name.size() < 1) break;
        
        bool horizontal = false;
        
        if (!strcmp("horizontal-row", layout_name.c_str()))
            horizontal = true;
        else if(!strcmp("vertical-row", layout_name.c_str()))
            horizontal = false;
        else
        {
            std::cerr << "Unknown layout name : " << layout_name.c_str() << std::endl;
            break;
        }
        
        const int w = parent->w, h = parent->h;
        
        // find space left after placing all absolutely-sized widgets in a row
        // (the space left will be divided between remaining widgets later)
        int left_space = (horizontal ? w : h);
        unsigned short total_proportion = 0;
        for(int n=0; n<widgets_amount; n++)
        {
            // relatively-sized widget
            std::string prop = widgets[n].m_properties[ PROP_PROPORTION ];
            if(prop.size() != 0)
            {
                total_proportion += atoi( prop.c_str() );
                continue;
            }
            
            // absolutely-sized widgets
            left_space -= (horizontal ? widgets[n].w : widgets[n].h);
        } // next widget
        
        // ---- lay widgets in row
        int x = parent->x, y = parent->y;
        for (int n=0; n<widgets_amount; n++)
        {
            std::string prop = widgets[n].m_properties[ PROP_PROPORTION ];
            if(prop.size() != 0)
            {
                // proportional size
                int proportion = 1;
                std::istringstream myStream(prop);
                if (!(myStream >> proportion))
                    std::cerr << "/!\\ Warning /!\\ : proportion  '" << prop.c_str() << "' is not a number in widget " << widgets[n].m_properties[PROP_ID].c_str() << std::endl;
                
                const float fraction = (float)proportion/(float)total_proportion;
                
                if (horizontal)
                {
   
                    widgets[n].x = x;
                    
                    
                    std::string align = widgets[n].m_properties[ PROP_ALIGN ];
                    if (align.size() < 1)
                    {        
                        if (widgets[n].m_properties[ PROP_Y ].size() > 0)
                            widgets[n].y = atoi(widgets[n].m_properties[ PROP_Y ].c_str());
                        else
                            widgets[n].y = y;
                    }
                    else if (align == "top")     widgets[n].y = y;
                    else if (align == "center")  widgets[n].y = y + h/2 - widgets[n].h/2;
                    else if (align == "bottom")  widgets[n].y = y + h - widgets[n].h;
                    else std::cerr  << "/!\\ Warning /!\\ : alignment  '" << align.c_str()
                                    <<  "' is unknown (widget '" << widgets[n].m_properties[PROP_ID].c_str()
                                    << "', in a horiozntal-row layout)\n";
                    
                    widgets[n].w = (int)(left_space*fraction);
                    if (widgets[n].m_properties[PROP_MAX_WIDTH].size() > 0)
                    {
                        const int max_width = atoi( widgets[n].m_properties[PROP_MAX_WIDTH].c_str() );
                        if (widgets[n].w > max_width) widgets[n].w = max_width;
                    }
                    
                    x += widgets[n].w;
                }
                else
                {    
                    widgets[n].h = (int)(left_space*fraction);
                    
                    if (widgets[n].m_properties[PROP_MAX_HEIGHT].size() > 0)
                    {
                        const int max_height = atoi( widgets[n].m_properties[PROP_MAX_HEIGHT].c_str() );
                        if(widgets[n].h > max_height) widgets[n].h = max_height;
                    }
                    
                    std::string align = widgets[n].m_properties[ PROP_ALIGN ];
                    if (align.size() < 1)
                    {                        
                        if (widgets[n].m_properties[ PROP_X ].size() > 0)
                            widgets[n].x = atoi(widgets[n].m_properties[ PROP_X ].c_str());
                        else
                            widgets[n].x = x;
                    }
                    else if (align == "left")   widgets[n].x = x;
                    else if (align == "center") widgets[n].x = x + w/2 - widgets[n].w/2;
                    else if (align == "right")  widgets[n].x = x + w - widgets[n].w;
                    else std::cerr << "/!\\ Warning /!\\ : alignment  '" << align.c_str()
                                   <<  "' is unknown (widget '" << widgets[n].m_properties[PROP_ID].c_str()
                                   << "', in a vertical-row layout)\n";
                    widgets[n].y = y;
                    
                    y += widgets[n].h;
                }
            }
            else
            {
                // absolute size
                
                if (horizontal)
                {
                    widgets[n].x = x;
                    
                    std::string align = widgets[n].m_properties[ PROP_ALIGN ];

                    if (align.size() < 1)
                    {
                        if (widgets[n].m_properties[ PROP_Y ].size() > 0)
                            widgets[n].y = atoi(widgets[n].m_properties[ PROP_Y ].c_str());
                        else
                            widgets[n].y = y;
                    }
                    else if (align == "top")    widgets[n].y = y;
                    else if (align == "center") widgets[n].y = y + h/2 - widgets[n].h/2;
                    else if (align == "bottom") widgets[n].y = y + h - widgets[n].h;
                    else std::cerr << "/!\\ Warning /!\\ : alignment  '" << align.c_str() << "' is unknown in widget " << widgets[n].m_properties[PROP_ID].c_str() << std::endl;
                    
                    x += widgets[n].w;
                }
                else
                {
                    //widgets[n].h = abs_var;
                    
                    std::string align = widgets[n].m_properties[ PROP_ALIGN ];
                    
                    if (align.size() < 1)
                    {
                        if (widgets[n].m_properties[ PROP_X ].size() > 0)
                            widgets[n].x = atoi(widgets[n].m_properties[ PROP_X ].c_str());
                        else
                            widgets[n].x = x;
                    }
                    else if (align == "left")   widgets[n].x = x;
                    else if (align == "center") widgets[n].x = x + w/2 - widgets[n].w/2;
                    else if (align == "right")  widgets[n].x = x + w - widgets[n].w;
                    else std::cerr << "/!\\ Warning /!\\ : alignment  '" << align.c_str() << "' is unknown in widget " << widgets[n].m_properties[PROP_ID].c_str() << std::endl;
                    widgets[n].y = y;
                    
                    y += widgets[n].h;
                }
            } // end if property or absolute size
            
        } // next widget
        
    } while(false);
    
    // ----- also deal with containers' children
    for(int n=0; n<widgets_amount; n++)
    {
        if(widgets[n].m_type == WTYPE_DIV) calculateLayout(widgets[n].m_children, &widgets[n]);
    }
}

// -----------------------------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Adding/Removing widgets
#endif


void Screen::addWidgets()
{
    assert(m_magic_number == 0xCAFEC001);
    if (!m_loaded) loadFromFile();
    
    addWidgetsRecursively( m_widgets );

    //std::cout << "*****ScreenAddWidgets " << m_filename.c_str() << " : focusing the first widget*****\n";
    
    // select the first widget (for first players only; if other players need some focus the Screen must provide it).
    Widget* w = getFirstWidget();
    //std::cout << "First widget is " << (w == NULL ? "null" : w->m_properties[PROP_ID].c_str()) << std::endl;
    if (w != NULL) w->setFocusForPlayer( PLAYER_ID_GAME_MASTER );
    else std::cerr << "Couldn't select first widget, NULL was returned\n";
}
// -----------------------------------------------------------------------------
void Screen::addWidgetsRecursively(ptr_vector<Widget>& widgets, Widget* parent)
{
    const unsigned short widgets_amount = widgets.size();
    
    // ------- add widgets
    for (int n=0; n<widgets_amount; n++)
    {
        if (widgets[n].m_type == WTYPE_DIV)
        {
            widgets[n].add(); // Will do nothing, but will maybe reserve an ID
            addWidgetsRecursively(widgets[n].m_children, &widgets[n]);
        }
        else
        {
            // warn if widget has no dimensions (except for ribbons and icons, where it is normal since it adjusts to its contents)
            if((widgets[n].w < 1 || widgets[n].h < 1) && widgets[n].m_type != WTYPE_RIBBON  && widgets[n].m_type != WTYPE_ICON_BUTTON)
                std::cerr << "/!\\ Warning /!\\ : widget " << widgets[n].m_properties[PROP_ID].c_str() << " has no dimensions" << std::endl;
            
            if(widgets[n].x == -1 || widgets[n].y == -1)
                std::cerr << "/!\\ Warning /!\\ : widget " << widgets[n].m_properties[PROP_ID].c_str() << " has no position" << std::endl;
            
            widgets[n].add();
        }
        
    } // next widget
    
}

// -----------------------------------------------------------------------------
/**
 * Called when screen is removed. This means all irrlicht widgets this object has pointers
 * to are now gone. Set all references to NULL to avoid problems.
 */
void Screen::elementsWereDeleted(ptr_vector<Widget>* within_vector)
{
    assert(m_magic_number == 0xCAFEC001);
    if (within_vector == NULL) within_vector = &m_widgets;
    const unsigned short widgets_amount = within_vector->size();
    
    for (int n=0; n<widgets_amount; n++)
    {
        Widget& widget = (*within_vector)[n];
        
        widget.elementRemoved();
        
        if (widget.m_children.size() > 0)
        {
            elementsWereDeleted( &(widget.m_children) );
        }
    }
}
// -----------------------------------------------------------------------------
void Screen::manualAddWidget(Widget* w)
{
    assert(m_magic_number == 0xCAFEC001);
    m_widgets.push_back(w);
}
// -----------------------------------------------------------------------------
void Screen::manualRemoveWidget(Widget* w)
{
    assert(m_magic_number == 0xCAFEC001);
    m_widgets.remove(w);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark Getting widgets
#endif

Widget* Screen::getWidget(const char* name)
{
    return getWidget(name, &m_widgets);
}

// -----------------------------------------------------------------------------

Widget* Screen::getWidget(const int id)
{
    return getWidget(id, &m_widgets);
}

// -----------------------------------------------------------------------------

Widget* Screen::getWidget(const char* name, ptr_vector<Widget>* within_vector)
{
    const unsigned short widgets_amount = within_vector->size();
    
    for(int n=0; n<widgets_amount; n++)
    {
        Widget& widget = (*within_vector)[n];
        
        if (widget.m_properties[PROP_ID] == name) return &widget;
        
        if (widget.searchInsideMe() && widget.m_children.size() > 0)
        {
            Widget* el = getWidget(name, &(widget.m_children));
            if(el != NULL) return el;
        }
    } // next
    
    return NULL;
}
// -----------------------------------------------------------------------------
Widget* Screen::getWidget(const int id, ptr_vector<Widget>* within_vector)
{
    const unsigned short widgets_amount = within_vector->size();
    
    for(int n=0; n<widgets_amount; n++)
    {
        Widget& widget = (*within_vector)[n];
        
        if (widget.m_element != NULL && widget.m_element->getID() == id) return &widget;
        
        if (widget.searchInsideMe() && widget.m_children.size() > 0)
        {
            // std::cout << "widget = <" << widget.m_properties[PROP_ID].c_str() << ">  widget.m_children.size()=" << widget.m_children.size() << std::endl;
            Widget* el = getWidget(id, &(widget.m_children));
            if(el != NULL) return el;
        }
    } // next
    
    return NULL;
}
// -----------------------------------------------------------------------------
Widget* Screen::getFirstWidget(ptr_vector<Widget>* within_vector)
{
    if (within_vector == NULL) within_vector = &m_widgets;
    
    for (int i = 0; i < within_vector->size(); i++)
    {
        if (!within_vector->get(i)->m_focusable) continue;
        
        // if container, also checks children (FIXME: don't hardcode which types to avoid descending into)
        if (within_vector->get(i)->m_children.size() > 0 &&
            within_vector->get(i)->m_type != WTYPE_RIBBON &&
            within_vector->get(i)->m_type != WTYPE_SPINNER)
        {
            Widget* w = getFirstWidget(&within_vector->get(i)->m_children);
            if (w != NULL) return w;
        }
        
        Widget* item = within_vector->get(i);
        if (item->m_element == NULL ||
            item->m_element->getTabOrder() == -1 ||
            item->m_element->getTabOrder() >= 1000 /* non-tabbing items are given such IDs */ ||
            !item->m_focusable)
        {
            continue;
        }
        
        return item;
    }
    return NULL;
}
// -----------------------------------------------------------------------------
Widget* Screen::getLastWidget(ptr_vector<Widget>* within_vector)
{
    if (within_vector == NULL) within_vector = &m_widgets;
    
    for (int i = within_vector->size()-1; i >= 0; i--)
    {
        if (!within_vector->get(i)->m_focusable) continue;
        
        // if container, also checks children
        if (within_vector->get(i)->m_children.size() > 0 &&
            within_vector->get(i)->m_type != WTYPE_RIBBON &&
            within_vector->get(i)->m_type != WTYPE_SPINNER)
        {
            Widget* w = getLastWidget(&within_vector->get(i)->m_children);
            if (w != NULL) return w;
        }
        
        Widget* item = within_vector->get(i);
        if (item->m_element == NULL ||
            item->m_element->getTabOrder() == -1 ||
            item->m_element->getTabOrder() >= 1000 /* non-tabbing items are given such IDs */ ||
            !item->m_focusable)
        {
            continue;
        }
        
        return item;
    }
    return NULL;
}


