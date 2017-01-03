/*
 -----------------------------------------------------------------------------
 This source file is part of OGRE
 (Object-oriented Graphics Rendering Engine)
 For the latest info, see http://www.ogre3d.org/

 Copyright (c) 2000-2014 Torus Knot Software Ltd

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 -----------------------------------------------------------------------------
 */
#ifndef __SdkCameraMan_H__
#define __SdkCameraMan_H__

#include "OgreBitesPrerequisites.h"
#include "OgreCamera.h"
#include "OgreSceneNode.h"
#include "OgreFrameListener.h"

#include "OgreInput.h"

/** \addtogroup Optional Components
*  @{
*/
/** \addtogroup Bites
*  @{
*/
namespace OgreBites
{
    enum CameraStyle   /// enumerator values for different styles of camera movement
    {
        CS_FREELOOK,
        CS_ORBIT,
        CS_MANUAL
    };

    /**
    Utility class for controlling the camera in samples.
    */
    class _OgreBitesExport CameraMan
    {
    public:
        CameraMan(Ogre::Camera* cam);

        virtual ~CameraMan() {}

        /**
        Swaps the camera on our camera man for another camera.
        */
        virtual void setCamera(Ogre::Camera* cam);

        virtual Ogre::Camera* getCamera()
        {
            return mCamera;
        }

        /**
        Sets the target we will revolve around. Only applies for orbit style.
        */
        virtual void setTarget(Ogre::SceneNode* target);

        virtual Ogre::SceneNode* getTarget()
        {
            return mTarget;
        }

        /**
        Sets the spatial offset from the target. Only applies for orbit style.
        */
        virtual void setYawPitchDist(Ogre::Radian yaw, Ogre::Radian pitch, Ogre::Real dist);

        /**
        Sets the camera's top speed. Only applies for free-look style.
        */
        virtual void setTopSpeed(Ogre::Real topSpeed)
        {
            mTopSpeed = topSpeed;
        }

        virtual Ogre::Real getTopSpeed()
        {
            return mTopSpeed;
        }

        /**
        Sets the movement style of our camera man.
        */
        virtual void setStyle(CameraStyle style);

        virtual CameraStyle getStyle()
        {
            return mStyle;
        }

        /**
        Manually stops the camera when in free-look mode.
        */
        virtual void manualStop();

        virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);

        /**
        Processes key presses for free-look style movement.
        */
        virtual void injectKeyDown(const KeyboardEvent& evt);

        /**
        Processes key releases for free-look style movement.
        */
        virtual void injectKeyUp(const KeyboardEvent& evt);

        /**
        Processes mouse movement differently for each style.
        */
        virtual void injectMouseMove(const MouseMotionEvent& evt);

        virtual void injectMouseWheel(const MouseWheelEvent& evt);

        /**
        Processes mouse presses. Only applies for orbit style.
        Left button is for orbiting, and right button is for zooming.
        */
        virtual void injectMouseDown(const MouseButtonEvent& evt);

        /**
        Processes mouse releases. Only applies for orbit style.
        Left button is for orbiting, and right button is for zooming.
        */
        virtual void injectMouseUp(const MouseButtonEvent& evt);

    protected:

        Ogre::Camera* mCamera;
        CameraStyle mStyle;
        Ogre::SceneNode* mTarget;
        bool mOrbiting;
        bool mZooming;
        Ogre::Real mTopSpeed;
        Ogre::Vector3 mVelocity;
        bool mGoingForward;
        bool mGoingBack;
        bool mGoingLeft;
        bool mGoingRight;
        bool mGoingUp;
        bool mGoingDown;
        bool mFastMove;
    };
}

#endif
