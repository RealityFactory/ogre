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

#ifndef __GLES2HardwareIndexBuffer_H__
#define __GLES2HardwareIndexBuffer_H__

#include "OgreGLES2Prerequisites.h"
#include "OgreHardwareIndexBuffer.h"
#include "OgreGLES2ManagedResource.h"
#include "OgreGLES2HardwareBuffer.h"

namespace Ogre {
    class _OgreGLES2Export GLES2HardwareIndexBuffer : public HardwareIndexBuffer MANAGED_RESOURCE
    {
        private:
            GLES2HardwareBuffer mBuffer;

        protected:
            void* lockImpl(size_t offset, size_t length, LockOptions options) {
                return mBuffer.lockImpl(offset, length, options);
            }
            void unlockImpl() {
                mBuffer.unlockImpl(mLockSize);
            }

#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID || OGRE_PLATFORM == OGRE_PLATFORM_EMSCRIPTEN
            /** See AndroidResource. */
            virtual void notifyOnContextLost();
        
            /** See AndroidResource. */
            virtual void notifyOnContextReset();
#endif
        
        public:
            GLES2HardwareIndexBuffer(HardwareBufferManagerBase* mgr, IndexType idxType, size_t numIndexes,
                                  HardwareBuffer::Usage usage,
                                  bool useShadowBuffer);

            /** See HardwareBuffer. */
            void readData(size_t offset, size_t length, void* pDest);
            /** See HardwareBuffer. */
            void writeData(size_t offset, size_t length, 
                const void* pSource, bool discardWholeBuffer = false);
#if OGRE_NO_GLES3_SUPPORT == 0
            /** See HardwareBuffer. */
            void copyData(HardwareBuffer& srcBuffer, size_t srcOffset,
                      size_t dstOffset, size_t length, bool discardWholeBuffer = false);
#endif
            /** See HardwareBuffer. */
            void _updateFromShadow(void);

            inline GLuint getGLBufferId(void) const { return mBuffer.getGLBufferId(); }
    };
}

#endif
