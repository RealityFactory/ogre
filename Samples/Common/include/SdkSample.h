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
#ifndef __SdkSample_H__
#define __SdkSample_H__

#include "Sample.h"
#include "OgreTrays.h"
#include "OgreCameraMan.h"
#include "OgreAdvancedRenderControls.h"
#include "Ogre.h"

namespace OgreBites
{
    /*=============================================================================
    // Base SDK sample class. Includes default player camera and SDK trays.
    =============================================================================*/
    class SdkSample : public Sample, public TrayListener
    {
    public:
        SdkSample()
        {
            // so we don't have to worry about checking if these keys exist later
            mInfo["Title"] = "Untitled";
            mInfo["Description"] = "";
            mInfo["Category"] = "Unsorted";
            mInfo["Thumbnail"] = "";
            mInfo["Help"] = "";

            mTrayMgr = 0;
            mCameraMan = 0;
            mCamera = 0;
            mViewport = 0;
            mControls = 0;
            mCursorWasVisible = false;
            mDragLook = false;
        }

        /*-----------------------------------------------------------------------------
        | Manually update the cursor position after being unpaused.
        -----------------------------------------------------------------------------*/
        virtual void unpaused()
        {
            mTrayMgr->refreshCursor();
        }

        /*-----------------------------------------------------------------------------
        | Automatically saves position and orientation for free-look cameras.
        -----------------------------------------------------------------------------*/
        virtual void saveState(Ogre::NameValuePairList& state)
        {
            if (mCameraMan->getStyle() == CS_FREELOOK)
            {
                state["CameraPosition"] = Ogre::StringConverter::toString(mCamera->getPosition());
                state["CameraOrientation"] = Ogre::StringConverter::toString(mCamera->getOrientation());
            }
        }

        /*-----------------------------------------------------------------------------
        | Automatically restores position and orientation for free-look cameras.
        -----------------------------------------------------------------------------*/
        virtual void restoreState(Ogre::NameValuePairList& state)
        {
            if (state.find("CameraPosition") != state.end() && state.find("CameraOrientation") != state.end())
            {
                mCameraMan->setStyle(CS_FREELOOK);
                mCamera->setPosition(Ogre::StringConverter::parseVector3(state["CameraPosition"]));
                mCamera->setOrientation(Ogre::StringConverter::parseQuaternion(state["CameraOrientation"]));
            }
        }

        virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt)
        {
            mTrayMgr->frameRendered(evt);
            mControls->frameRendered(evt);

            if (!mTrayMgr->isDialogVisible())
            {
                mCameraMan->frameRendered(evt);   // if dialog isn't up, then update the camera
            }

            return true;
        }

        virtual void windowResized(Ogre::RenderWindow* rw)
        {
            mCamera->setAspectRatio((Ogre::Real)mViewport->getActualWidth() / (Ogre::Real)mViewport->getActualHeight());
        }

        virtual bool keyPressed(const KeyboardEvent& evt)
        {
        	int key = evt.keysym.sym;
        	
            if (key == 'h' || key == SDLK_F1)   // toggle visibility of help dialog
            {
                if (!mTrayMgr->isDialogVisible() && mInfo["Help"] != "") mTrayMgr->showOkDialog("Help", mInfo["Help"]);
                else mTrayMgr->closeDialog();
            }

            if (mTrayMgr->isDialogVisible()) return true;   // don't process any more keys if dialog is up

            if (key == SDLK_F6)   // take a screenshot
            {
                mWindow->writeContentsToTimestampedFile("screenshot", ".png");
            }
#if OGRE_PROFILING
            // Toggle visibility of profiler window
            else if (key == SDLK_P)
            {
                Ogre::Profiler* prof = Ogre::Profiler::getSingletonPtr();
                if (prof)
                    prof->setEnabled(!prof->getEnabled());
            }
#endif // OGRE_PROFILING
            else {
                mControls->keyPressed(evt);
            }

            mCameraMan->keyPressed(evt);
            return true;
        }

        virtual bool keyReleased(const KeyboardEvent& evt)
        {
            mCameraMan->keyReleased(evt);

            return true;
        }

        /* IMPORTANT: When overriding these following handlers, remember to allow the tray manager
        to filter out any interface-related mouse events before processing them in your scene.
        If the tray manager handler returns true, the event was meant for the trays, not you. */
        virtual bool mouseMoved(const MouseMotionEvent& evt)
        {
            if (mTrayMgr->mouseMoved(evt)) return true;

            mCameraMan->mouseMoved(evt);
            return true;
        }

        // convert and redirect
        virtual bool touchMoved(const TouchFingerEvent& evt) {
            MouseMotionEvent e;
            e.xrel = evt.dx * mWindow->getWidth();
            e.yrel = evt.dy * mWindow->getHeight();
            return mouseMoved(e);
        }

        virtual bool mousePressed(const MouseButtonEvent& evt)
        {
            if (mTrayMgr->mousePressed(evt)) return true;

            if (mDragLook && evt.button == BUTTON_LEFT)
            {
                mCameraMan->setStyle(CS_FREELOOK);
                mTrayMgr->hideCursor();
            }

            mCameraMan->mousePressed(evt);
            return true;
        }

        // convert and redirect
        virtual bool touchPressed(const TouchFingerEvent& evt) {
            MouseButtonEvent e;
            e.button = BUTTON_LEFT;
            return mousePressed(e);
        }

        virtual bool mouseReleased(const MouseButtonEvent& evt)
        {
            if (mTrayMgr->mouseReleased(evt)) return true;

            if (mDragLook && evt.button == BUTTON_LEFT)
            {
                mCameraMan->setStyle(CS_MANUAL);
                mTrayMgr->showCursor();
            }

            mCameraMan->mouseReleased(evt);
            return true;
        }

        // convert and redirect
        virtual bool touchReleased(const TouchFingerEvent& evt) {
            MouseButtonEvent e;
            e.button = BUTTON_LEFT;
            return mouseReleased(e);
        }

        virtual bool mouseWheelRolled(const MouseWheelEvent& evt) {
            mCameraMan->mouseWheelRolled(evt);
            return true;
        }

        /*-----------------------------------------------------------------------------
        | Extended to setup a default tray interface and camera controller.
        -----------------------------------------------------------------------------*/
        virtual void _setup(Ogre::RenderWindow* window, Ogre::FileSystemLayer* fsLayer, Ogre::OverlaySystem* overlaySys)
        {
            // assign mRoot here in case Root was initialised after the Sample's constructor ran.
            mRoot = Ogre::Root::getSingletonPtr();
            mWindow = window;
            mFSLayer = fsLayer;
            mOverlaySystem = overlaySys;

            locateResources();
            createSceneManager();
            setupView();

            mTrayMgr = new TrayManager("SampleControls", window, this);  // create a tray interface

            loadResources();
            mResourcesLoaded = true;

            // show stats and logo and hide the cursor
            mTrayMgr->showFrameStats(TL_BOTTOMLEFT);
            mTrayMgr->showLogo(TL_BOTTOMRIGHT);
            mTrayMgr->hideCursor();

            mControls = new AdvancedRenderControls(mTrayMgr, mCamera);
            setupContent();
            mContentSetup = true;

            mDone = false;
        }

        virtual void _shutdown()
        {
            Sample::_shutdown();

            delete mTrayMgr;
            delete mCameraMan;
            delete mControls;

            // restore settings we may have changed, so as not to affect other samples
            Ogre::MaterialManager::getSingleton().setDefaultTextureFiltering(Ogre::TFO_BILINEAR);
            Ogre::MaterialManager::getSingleton().setDefaultAnisotropy(1);
        }

    protected:

        virtual void setupView()
        {
            // setup default viewport layout and camera
            mCamera = mSceneMgr->createCamera("MainCamera");
            mViewport = mWindow->addViewport(mCamera);
            mCamera->setAspectRatio((Ogre::Real)mViewport->getActualWidth() / (Ogre::Real)mViewport->getActualHeight());
            mCamera->setAutoAspectRatio(true);
            mCamera->setNearClipDistance(5);

            mCameraMan = new CameraMan(mCamera);   // create a default camera controller
        }

        virtual void setDragLook(bool enabled)
        {
            if (enabled)
            {
                mCameraMan->setStyle(CS_MANUAL);
                mTrayMgr->showCursor();
                mDragLook = true;
            }
            else
            {
                mCameraMan->setStyle(CS_FREELOOK);
                mTrayMgr->hideCursor();
                mDragLook = false;
            }
        }

        Ogre::Viewport* mViewport;          // main viewport
        Ogre::Camera* mCamera;              // main camera
        TrayManager* mTrayMgr;           // tray interface manager
        CameraMan* mCameraMan;           // basic camera controller
        AdvancedRenderControls* mControls; // sample details panel
        bool mCursorWasVisible;             // was cursor visible before dialog appeared
        bool mDragLook;                     // click and drag to free-look
    };
}

#endif
