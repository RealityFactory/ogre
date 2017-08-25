/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2008 Renato Araujo Oliveira Filho <renatox@gmail.com>
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

#ifndef __EGLSupport_H__
#define __EGLSupport_H__


#include "OgreGLNativeSupport.h"
#include <EGL/egl.h>
#include "OgreEGLWindow.h"

#define ENABLE_EGL_CHECK 0

#if ENABLE_EGL_CHECK
    #define EGL_CHECK_ERROR \
    { \
        int e = eglGetError(); \
        if ((e != 0) && (e != EGL_SUCCESS))\
        { \
            char msgBuf[4096]; \
            sprintf(msgBuf, "EGL error 0x%04X in %s at line %i\n", e, __PRETTY_FUNCTION__, __LINE__); \
            LogManager::getSingleton().logMessage(msgBuf); \
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR, msgBuf, __PRETTY_FUNCTION__); \
        } \
    }
#else
    #define EGL_CHECK_ERROR {}
#endif

namespace Ogre {
    template<class C> void removeDuplicates(C& c)
    {
        std::sort(c.begin(), c.end());
        typename C::iterator p = std::unique(c.begin(), c.end());
        c.erase(p, c.end());
    }

    class _OgrePrivate EGLSupport : public GLNativeSupport
    {
        protected:
            void refreshConfig(void);

            EGLDisplay mGLDisplay;
            NativeDisplayType mNativeDisplay;

            bool mIsExternalDisplay;
            bool mRandr;
            typedef std::pair<uint, uint> ScreenSize;
            typedef short Rate;
            typedef std::pair<ScreenSize, Rate> VideoMode;
            typedef std::vector<VideoMode> VideoModes;
            VideoModes mVideoModes;
            VideoMode mOriginalMode;
            VideoMode mCurrentMode;
            StringVector mSampleLevels;

            EGLint mEGLMajor, mEGLMinor;

            //virtual EGLWindow* createEGLWindow( EGLSupport * support) = 0;

            void initialiseExtensions();
        public:
            EGLSupport(int profile);

            void start(void);
            void stop(void);
            void addConfig(void);
            void setConfigOption(const String &name, const String &value);
            virtual String getDisplayName (void);
            EGLDisplay getGLDisplay(void);
            void setGLDisplay(EGLDisplay val);
            EGLConfig* chooseGLConfig(const EGLint *attribList, EGLint *nElements);
            EGLConfig* getConfigs(EGLint *nElements);
            EGLBoolean getGLConfigAttrib(EGLConfig fbConfig, EGLint attribute, EGLint *value);
            void* getProcAddress(const char* name);
            ::EGLContext createNewContext(EGLDisplay eglDisplay, ::EGLConfig glconfig, ::EGLContext shareList) const;

            NameValuePairList parseOptions(uint& w, uint& h, bool& fullscreen);

            RenderWindow* newWindow(const String& name,
                                    unsigned int width, unsigned int height,
                                    bool fullScreen,
                                    const NameValuePairList *miscParams = 0) {
                return NULL;
            }

            ::EGLConfig getGLConfigFromContext(::EGLContext context);
            ::EGLConfig getGLConfigFromDrawable(::EGLSurface drawable,
                                                unsigned int *w, unsigned int *h);
            ::EGLConfig selectGLConfig (const EGLint* minAttribs, const EGLint *maxAttribs);
            void switchMode(void);
            virtual void switchMode(uint& width, uint& height, short& frequency) = 0;
           // virtual GLPBuffer* createPBuffer(PixelComponentType format,
            //                           size_t width, size_t height) = 0;
//          NativeDisplayType getNativeDisplay();
    };
}

#endif
