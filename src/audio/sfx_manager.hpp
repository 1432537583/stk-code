//  $Id$
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2008 Joerg Henrichs
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

#ifndef HEADER_SFX_MANAGER_HPP
#define HEADER_SFX_MANAGER_HPP

#include <vector>
#include <map>
#ifdef __APPLE__
#  include <OpenAL/al.h>
#else
#  include <AL/al.h>
#endif

#include "lisp/lisp.hpp"
#include "utils/vec3.hpp"

class SFXBase;
class XMLNode;

/** Manager of all sound effects. The manager reads all sound effects and
 *  maintains the corresponding buffers. Each sound effect objects uses
 *  on of the (shared) buffers from the sound manager.
 */
class SFXManager
{
public:

    /**
      *  Entries for custom SFX sounds.  These are unique for each kart.
      * eg. kart->playCustomSFX(SFX_MANAGER::CUSTOM_HORN)
      */
    enum CustomSFX
    {
        CUSTOM_HORN,    // Replaces default horn
        CUSTOM_CRASH,   // Played when colliding with another kart
        CUSTOM_WIN,     // Played when racer wins
        CUSTOM_EXPLODE, // Played when struck by bowling ball or dynamite
        CUSTOM_GOO,     // Played when driving through goo
        CUSTOM_PASS,    // Played when passing another kart
        CUSTOM_ZIPPER,  // Played when kart hits zipper
        CUSTOM_NAME,    // Introduction "I'm Tux!"
        CUSTOM_ATTACH,  // Played when something is attached to kart (Uh-Oh)
        CUSTOM_SHOOT,   // Played when weapon is used
        NUM_CUSTOMS
    };

    /** Status of a sound effect. */
    enum SFXStatus
    {
        SFX_UNKNOWN = -1, SFX_STOPED = 0, SFX_PAUSED = 1, SFX_PLAYING = 2,
        SFX_INITIAL = 3
    };

private:        
    
    class SFXBufferInfo
    {
    private:
    public:
        ALuint   m_sfx_buffer;
        bool     m_sfx_positional;
        float    m_sfx_rolloff;
        float    m_sfx_gain;
        
        SFXBufferInfo()
        {
            m_sfx_buffer = 0;
            m_sfx_gain = 1.0f;
            m_sfx_rolloff = 0.1f;
            m_sfx_positional = false;
        }

        
        /** Cannot appear in destructor because copy-constructors may be used,
          * and the OpenAL source must not be deleted on a copy */
        void freeBuffer()
        {
            alDeleteBuffers(1, &m_sfx_buffer);
            m_sfx_buffer = 0;
        }
        ~SFXBufferInfo()
        {
        }
    };
    
    /** The buffers and info for all sound effects. These are shared among all
     *  instances of SFXOpenal. */
    std::map<std::string, SFXBufferInfo> m_all_sfx_types;
    
    /** The actual instances (sound sources) */
    std::vector<SFXBase*> m_all_sfx;
    
    /** To play non-positional sounds without having to create a new object for each */
    static std::map<std::string, SFXBase*> m_quick_sounds;

    bool                      m_initialized;
    float                     m_masterGain;

    void                      loadSfx();

    void                      loadSingleSfx(const XMLNode* node);

public:
                             SFXManager();
    virtual                 ~SFXManager();
    bool                     sfxAllowed();
    bool                     addSingleSfx(  const char* sfx_name,
                                            std::string    filename,
                                            bool           positional,
                                            float          rolloff,
                                            float          gain);

    SFXBase*                 createSoundSource(const SFXBufferInfo& info, const bool addToSFXList=true);
    SFXBase*                 createSoundSource(const char* name, const bool addToSFXList=true);
    
    void                     deleteSFX(SFXBase *sfx);
    void                     pauseAll();
    void                     resumeAll();
    
    void                     setMasterSFXVolume(float gain);
    float                    getMasterSFXVolume() const { return m_masterGain; }
    
    static bool              checkError(const std::string &context);
    static const std::string getErrorString(int err);
    
    /** Positional sound is cool, but creating a new object just to play a simple
        menu sound is not. This function allows for 'quick sounds' in a single call.*/
    static void              quickSound(const char* soundName);

};

extern SFXManager* sfx_manager;

#endif // HEADER_SFX_MANAGER_HPP

