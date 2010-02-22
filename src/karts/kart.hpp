//  $Id$
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2005 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006 SuperTuxKart-Team, Joerg Henrichs, Steve Baker
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

#ifndef HEADER_KART_HPP
#define HEADER_KART_HPP

#include "btBulletDynamicsCommon.h"

#include "graphics/camera.hpp"
#include "items/attachment.hpp"
#include "items/powerup.hpp"
#include "karts/moveable.hpp"
#include "karts/kart_properties.hpp"
#include "karts/controller/controller.hpp"
#include "karts/controller/kart_control.hpp"
#include "karts/kart_model.hpp"
#include "tracks/terrain_info.hpp"

class SkidMarks;
class Shadow;
class Item;
class Smoke;
class WaterSplash;
class Nitro;
class SlipStream;
class SFXBase;
class btUprightConstraint;
class btKart;
class btVehicleTuning;
class Quad;
class Stars;

/** The main kart class. All type of karts are of this object, but with 
 *  different controllers. The controllers are what turn a kart into a 
 *  player kart (i.e. the controller handle input), or an AI kart (the
 *  controller runs the AI code to set steering etc).
 *  Kart has two base classes: the most important one is moveable (which
 *  is an object that is moved on the track, and has position and rotations)
 *  and TerrainInfo, which manages the terrain the kart is on.
 */
class Kart : public TerrainInfo, public Moveable
{
private:
    /** Reset position. */
    btTransform  m_reset_transform;
    /** Index of kart in world. */
    unsigned int m_world_kart_id;
    /** Accumulated skidding factor. */
    float        m_skidding;

    /** The main controller of this object, used for driving. This 
     *  controller is used to run the kart. It will be replaced
     *  with an end kart controller when the kart finishes the race. */
    Controller  *m_controller;
    /** This saves the original controller when the end controller is
     *  used. This is an easy solution for restarting the race, since
     *  the controller do not need to be reinitialised. */
    Controller  *m_saved_controller;

    /** Initial rank of the kart. */
    int          m_initial_position;

    /** Current race position (1-num_karts). */
    int          m_race_position;

    /** The camera for each kart. Not all karts have cameras (e.g. AI karts
     *  usually don't), but there are exceptions: e.g. after the end of a
     *  race an AI kart is replacing the kart for a player.
     */

protected:       // Used by the AI atm
    KartControl  m_controls;           // The kart controls (e.g. steering, fire, ...)
    Powerup      m_powerup;
    float        m_zipper_time_left;   /**<Zipper time left. */
    Attachment   m_attachment;
    /** Easier access for player_kart. */
    Camera       *m_camera;
private:
    float        m_max_speed;          // maximum speed of the kart, computed from
    /** Depending on terrain a certain reduction to the maximum speed applies.
     *  This reduction is accumulated in m_max_speed_reduction. */
    float        m_max_speed_reduction;
    float        m_power_reduction;
    float        m_max_gear_rpm;       /**<Maximum engine rpm's for the current gear*/
    float        m_max_speed_reverse_ratio;
    float        m_bounce_back_time;   /**<A short time after a collision acceleration
                                        *  is disabled to allow the karts to bounce back*/

    // Bullet physics parameters
    // -------------------------
    btRaycastVehicle::btVehicleTuning 
                            *m_tuning;
    btCompoundShape          m_kart_chassis;
    btVehicleRaycaster      *m_vehicle_raycaster;
    btKart                  *m_vehicle;
    btUprightConstraint     *m_uprightConstraint;

     /** The amount of energy collected by hitting coins. Note that it
      *  must be float, since dt is subtraced in each timestep. */
    float         m_collected_energy;

    // Graphical effects
    // -----------------
    /** The shadow of a kart. */
    Shadow       *m_shadow;
    
    /** If a kart is flying, the shadow is disabled (since it is
     *  stuck to the kart, i.e. the shadow would be flying, too). */
    bool          m_shadow_enabled;
    
    /** Smoke from skidding. */
    Smoke        *m_smoke_system;
    
    /** Water splash when driving in water. */
    WaterSplash  *m_water_splash_system;

    /** For stars rotating around head effect */
    Stars        *m_stars_effect;
    
    /** Graphical effect when using a nitro. */
    Nitro        *m_nitro;

    /** Graphical effect when slipstreaming. */
    SlipStream   *m_slip_stream;

    float         m_wheel_rotation;
    /** For each wheel it stores the suspension length after the karts are at 
     *  the start position, i.e. the suspension will be somewhat compressed.
     *  The bullet suspensionRestLength is the value when the suspension is not
     *  at all compressed. */
    float         m_default_suspension_length[4];

    /** The skidmarks object for this kart. */
    SkidMarks    *m_skidmarks;

    // Variables for slipstreaming
    // ---------------------------
    /** The quad inside which another kart is considered to be slipstreaming.
     *  This value is current area, i.e. takes the kart position into account. */
    Quad         *m_slipstream_quad;

    /** This is slipstream area if the kart is at 0,0,0 without rotation. From 
     *  this value m_slipstream_area is computed by applying the kart transform. */
    Quad         *m_slipstream_original_quad;

    /** The time a kart was in slipstream. */
    float         m_slipstream_time;

    /** Slipstream mode: either nothing happening, or the kart is collecting
     *  'slipstream credits', or the kart is using accumulated credits. */
    enum         {SS_NONE, SS_COLLECT, SS_USE} m_slipstream_mode;

    float         m_finish_time;
    bool          m_finished_race;

    /** When a kart has its view blocked by the plunger, this variable will be 
     *  > 0 the number it contains is the time left before removing plunger. */
    float         m_view_blocked_by_plunger;
    float         m_speed;
    float         m_current_gear_ratio;
    /** Different kart modes: normal racing, being rescued, showing end
     *  animation, explosions, kart eliminated. */
    enum          {KM_RACE, KM_RESCUE, KM_END_ANIM, KM_EXPLOSION, 
                   KM_ELIMINATED}
                  m_kart_mode;

    std::vector<SFXBase*> m_custom_sounds;
    SFXBase      *m_beep_sound;
    SFXBase      *m_engine_sound;
    SFXBase      *m_crash_sound;
    SFXBase      *m_skid_sound;
    SFXBase      *m_goo_sound;
    float         m_time_last_crash;

    float         handleSlipstream(float dt);
    void          updatePhysics(float dt);

protected:
    float                 m_rescue_pitch, m_rescue_roll;
    const KartProperties *m_kart_properties;

    
public:
                   Kart(const std::string& ident, int position, 
                        const btTransform& init_transform);
    virtual       ~Kart();
    unsigned int   getWorldKartId() const            { return m_world_kart_id;   }
    void           setWorldKartId(unsigned int n)    { m_world_kart_id=n;        }
    void           loadData();
    virtual void   updateGraphics(const Vec3& off_xyz,  const Vec3& off_hpr);
    const KartProperties* 
                   getKartProperties() const      { return m_kart_properties; }
    // ------------------------------------------------------------------------
    /** Sets the kart properties. */
    void setKartProperties(const KartProperties *kp)
    {
        m_kart_properties=kp;                
    }
    // ------------------------------------------------------------------------
    /** Sets the attachment and time it stays attached. */
    void attach(attachmentType attachment_, float time)
    { 
        m_attachment.set(attachment_, time); 
    }
    // ------------------------------------------------------------------------
    /** Sets a new powerup. */
    void setPowerup (PowerupType t, int n)
    { 
        m_powerup.set(t, n);
    }   // setPowerup

    // ------------------------------------------------------------------------
    /** Sets the position in race this kart has (1<=p<=n). */
    virtual void setPosition(int p)    
    {
        m_race_position = p;
        m_controller->setPosition(p);
    }   // setPosition

    // ------------------------------------------------------------------------
    Attachment *getAttachment() 
    {
        return &m_attachment;          
    }   // getAttachment

    // ------------------------------------------------------------------------
    void setAttachmentType(attachmentType t, float time_left=0.0f, Kart*k=NULL)
    {
        m_attachment.set(t, time_left, k);   
    }
    // ------------------------------------------------------------------------
    /** Returns the camera of this kart (or NULL if no camera is attached
     *  to this kart). */
    Camera*        getCamera         ()       {return m_camera;}
    /** Returns the camera of this kart (or NULL if no camera is attached
     *  to this kart) - const version. */
    const Camera*  getCamera         () const {return m_camera;}
    /** Sets the camera for this kart. */
    void           setCamera(Camera *camera) {m_camera=camera; }

    const Powerup *getPowerup          () const { return &m_powerup;         }
    Powerup       *getPowerup          ()       { return &m_powerup;         }
    /** Returns the number of powerups. */
    int            getNumPowerup       () const { return m_powerup.getNum(); }
    /** Returns the time left for a zipper. */
    float          getZipperTimeLeft   () const { return m_zipper_time_left; }
    /** Returns the remaining collected energy. */
    float          getEnergy           () const { return m_collected_energy; }
    /** Returns the current position of this kart in the race. */
    int            getPosition         () const { return m_race_position;    }
    /** Returns the initial position of this kart. */
    int            getInitialPosition  () const { return m_initial_position; }
    /** Returns the finished time for a kart. */
    float          getFinishTime       () const { return m_finish_time;      }
    /** Returns true if this kart has finished the race. */
    bool           hasFinishedRace     () const { return m_finished_race;    }
    void           endRescue           ();
    void           getClosestKart      (float *cdist, int *closest);

    bool           hasViewBlockedByPlunger() const
                                                { return m_view_blocked_by_plunger > 0; }
    void           blockViewWithPlunger()       { m_view_blocked_by_plunger = 10; }
    
   /** Returns a bullet transform object located at the kart's position
       and oriented in the direction the kart is going. Can be useful
       e.g. to calculate the starting point and direction of projectiles
    */
   btTransform     getKartHeading      (const float customPitch=-1);

    
    // Functions to access the current kart properties (which might get changed,
    // e.g. mass increase or air_friction increase depending on attachment etc.)
    // -------------------------------------------------------------------------
    const video::SColor &getColor   () const {return m_kart_properties->getColor();}
    float          getMass          () const
    {
        return m_kart_properties->getMass()
               + m_attachment.weightAdjust();
    }
    float          getMaxPower      () const {return m_kart_properties->getMaxPower();}
    float          getTimeFullSteer () const {return m_kart_properties->getTimeFullSteer();}
    float          getBrakeFactor   () const {return m_kart_properties->getBrakeFactor();}
    float          getFrictionSlip  () const {return m_kart_properties->getFrictionSlip();}
    float          getSkidding      () const {return m_skidding;}
    float          getMaxSteerAngle () const
                       {return m_kart_properties->getMaxSteerAngle(getSpeed());}
    const Vec3&    getGravityCenterShift   () const
        {return m_kart_properties->getGravityCenterShift();                    }
    float          getSteerPercent  () const {return m_controls.m_steer;       }
    KartControl&
                   getControls      ()       {return m_controls;               }
    const KartControl&
                   getControls      () const {return m_controls;               }
    /** Sets the kart controls. Used e.g. by replaying history. */
    void           setControls(const KartControl &c) { m_controls = c;         }
    /** Returns the maximum speed of the kart independent of the 
     *  terrain it is on. */
    float          getMaxSpeed      () const {return m_max_speed;              }    
    /** Returns the maximum speed of the kart but includes the effect of 
     *  the terrain it is on. */    
    float          getMaxSpeedOnTerrain() const {return m_max_speed-
                                                     m_max_speed_reduction;    }
    /** Returns the length of the kart. */
    float          getKartLength    () const
                   {return m_kart_properties->getKartModel()->getLength();     }
    /** Returns the height of the kart. */
    float          getKartHeight    () const 
                   {return m_kart_properties->getKartModel()->getHeight();     }
    /** Returns the width of the kart. */
    float          getKartWidth    () const 
                   {return m_kart_properties->getKartModel()->getWidth();      }
    btKart        *getVehicle       () const {return m_vehicle;                }
    btUprightConstraint *getUprightConstraint() const {return m_uprightConstraint;}
    void           createPhysics    ();
    bool           isInRest         () const;
    //have to use this instead of moveable getVelocity to get velocity for bullet rigid body
    float          getSpeed         () const {return m_speed;                 }
    /** This is used on the client side only to set the speed of the kart
     *  from the server information.                                       */
    void           setSpeed         (float s) {m_speed = s;                   }
    void           setSuspensionLength();
    float          handleNitro      (float dt);
    float          getActualWheelForce();
    /** True if the wheels are touching the ground. */
    bool           isOnGround       () const;
    /** Returns true if the kart is close to the ground, used to dis/enable
     *  the upright constraint to allow for more realistic explosions. */
    bool           isNearGround     () const;
    /** Returns true if the kart is eliminated. */
    bool           isEliminated     () const {return m_kart_mode==KM_ELIMINATED;}
    /** Returns true if the kart is being rescued. */
    bool           isRescue         () const {return m_kart_mode==KM_RESCUE;}
    void           eliminate        ();
    void           resetBrakes      ();
    void           adjustSpeed      (float f);
    void           updatedWeight    ();
    void           forceRescue      ();
    void           handleExplosion  (const Vec3& pos, bool direct_hit);
    /** Returns a name to be displayed for this kart. */
    virtual const irr::core::stringw& getName() const {return m_kart_properties->getName();}
    const std::string& getIdent     () const {return m_kart_properties->getIdent();}
    // addMessages gets called by world to add messages to the gui
    virtual void   addMessages      () {};
    virtual void   collectedItem    (const Item &item, int random_attachment);
    virtual void   reset            ();
    virtual void   handleZipper     ();
    virtual void   crashed          (Kart *k);
    
    virtual void   update           (float dt);
    virtual void   finishedRace     (float time);
    void           beep             ();
    bool           playCustomSFX    (unsigned int type);
    /** Returns the start transform, i.e. position and rotation. */
    const btTransform getResetTransform() const {return m_reset_transform;}
    /** Returns the controller of this kart. */
    Controller*    getController() { return m_controller; }
    /** Returns the controller of this kart (const version). */
    const Controller* getController() const { return m_controller; }
    void           setController(Controller *controller);
};


#endif

/* EOF */

