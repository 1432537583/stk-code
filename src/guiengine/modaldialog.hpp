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

#ifndef HEADER_MODAL_DIALOG_HPP
#define HEADER_MODAL_DIALOG_HPP

#include "irrlicht.h"
#include "utils/ptr_vector.hpp"
#include "guiengine/event_handler.hpp"
#include "guiengine/skin.hpp"

class PlayerProfile;

namespace GUIEngine
{
    class Widget;
    class TextBoxWidget;
    class ButtonWidget;
    
/**
  * Base class, derive your own.
  * Only once instance at a time (if you create a 2nd the first will be destroyed).
  * You can call static function 'dismiss' to simply close current dialog (so you don't
  * need to keep track of instances yourself)
  */
class ModalDialog : public SkinWidgetContainer
{
protected:
    irr::gui::IGUIWindow* m_irrlicht_window;
    irr::core::rect< irr::s32 > m_area;
    
    /**
     * Creates a modal dialog with given percentage of screen width and height
     */
    ModalDialog(const float percentWidth, const float percentHeight);
    
    virtual void onEnterPressedInternal();
    void clearWindow();
    
public:
    ptr_vector<Widget> m_children;
    
    virtual ~ModalDialog();
    
    /** Returns whether to block event propagation (usually, you will want to block events you processed) */
    virtual EventPropagation processEvent(std::string& eventSource){ return EVENT_LET; }
    
    static void dismiss();
    static void onEnterPressed();
    static ModalDialog* getCurrent();
    static bool isADialogActive();
};  
    
}
#endif
