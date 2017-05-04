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

#include "OgreStableHeaders.h"
#include "OgreGL3PlusStateCacheManager.h"
#include "OgreGL3PlusRenderSystem.h"
#include "OgreLogManager.h"
#include "OgreRoot.h"

namespace Ogre {
    
    GL3PlusStateCacheManager::GL3PlusStateCacheManager(void)
    {
        clearCache();
    }
    
    void GL3PlusStateCacheManager::initializeCache()
    {
        glBlendEquation(GL_FUNC_ADD);

        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        
        glBlendFunc(GL_ONE, GL_ZERO);
        
        glCullFace(mCullFace);

        glDepthFunc(mDepthFunc);

        glDepthMask(mDepthMask);

        glStencilMask(mStencilMask);

        glClearDepth(mClearDepth);

        glBindTexture(GL_TEXTURE_2D, 0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glActiveTexture(GL_TEXTURE0);

        glClearColor(mClearColour[0], mClearColour[1], mClearColour[2], mClearColour[3]);

        glColorMask(mColourMask[0], mColourMask[1], mColourMask[2], mColourMask[3]);

        glPolygonMode(GL_FRONT_AND_BACK, mPolygonMode);
    }

    void GL3PlusStateCacheManager::clearCache()
    {
        mDepthMask = GL_TRUE;
        mBlendEquation = GL_FUNC_ADD;
        mBlendEquationRGB = GL_FUNC_ADD;
        mBlendEquationAlpha = GL_FUNC_ADD;
        mCullFace = GL_BACK;
        mDepthFunc = GL_LESS;
        mStencilMask = 0xFFFFFFFF;
        mActiveTextureUnit = 0;
        mDiscardBuffers = 0;
        mClearDepth = 1.0f;
        mLastBoundTexID = 0;

        mPolygonMode = GL_FILL;

        // Initialize our cache variables and also the GL so that the
        // stored values match the GL state
        mBlendFuncSource = GL_ONE;
        mBlendFuncDest = GL_ZERO;
        
        mClearColour.resize(4);
        mClearColour[0] = mClearColour[1] = mClearColour[2] = mClearColour[3] = 0.0f;
        
        mColourMask.resize(4);
        mColourMask[0] = mColourMask[1] = mColourMask[2] = mColourMask[3] = GL_TRUE;

        mBoolStateMap.clear();
        mActiveBufferMap.clear();
        mTexUnitsMap.clear();
        mTextureCoordGen.clear();

        mViewport[0] = 0.0f;
        mViewport[1] = 0.0f;
        mViewport[2] = 0.0f;
        mViewport[3] = 0.0f;

        mPointSize = 1.0f;
        mPointSizeMin = 1.0f;
        mPointSizeMax = 1.0f;
        mPointAttenuation[0] = 1.0f;
        mPointAttenuation[1] = 0.0f;
        mPointAttenuation[2] = 0.0f;
    }
    
    GL3PlusStateCacheManager::~GL3PlusStateCacheManager(void)
    {
        mColourMask.clear();
        mClearColour.clear();
        mActiveBufferMap.clear();
        mBoolStateMap.clear();
        mTexUnitsMap.clear();
        mTextureCoordGen.clear();
    }
    
    void GL3PlusStateCacheManager::bindGLBuffer(GLenum target, GLuint buffer, bool force)
    {
        bool update = false;
       
        BindBufferMap::iterator i = mActiveBufferMap.find(target);
        if (i == mActiveBufferMap.end())
        {
            // Haven't cached this state yet.  Insert it into the map
            mActiveBufferMap.insert(BindBufferMap::value_type(target, buffer));
            update = true;
        }
        else if((*i).second != buffer || force) // Update the cached value if needed
        {
            (*i).second = buffer;
            update = true;
        }

        // Update GL
        if(update)
        {
            if(target == GL_FRAMEBUFFER)
            {
                OGRE_CHECK_GL_ERROR(glBindFramebuffer(target, buffer));
            }
            else if(target == GL_RENDERBUFFER)
            {
                OGRE_CHECK_GL_ERROR(glBindRenderbuffer(target, buffer));
            }
            else
            {
                OGRE_CHECK_GL_ERROR(glBindBuffer(target, buffer));
            }
        }

    }

    void GL3PlusStateCacheManager::deleteGLBuffer(GLenum target, GLuint buffer, bool force)
    {
        // Buffer name 0 is reserved and we should never try to delete it
        if(buffer == 0)
            return;
        
        BindBufferMap::iterator i = mActiveBufferMap.find(target);
        
        if (i != mActiveBufferMap.end() && ((*i).second == buffer || force))
        {
            if(target == GL_FRAMEBUFFER)
            {
                OGRE_CHECK_GL_ERROR(glDeleteFramebuffers(1, &buffer));
            }
            else if(target == GL_RENDERBUFFER)
            {
                OGRE_CHECK_GL_ERROR(glDeleteRenderbuffers(1, &buffer));
            }
            else
            {
                OGRE_CHECK_GL_ERROR(glDeleteBuffers(1, &buffer));
            }

            // Currently bound buffer is being deleted, update the cached value to 0,
            // which it likely the buffer that will be bound by the driver.
            // An update will be forced next time we try to bind on this target.
            (*i).second = 0;
        }
    }

    void GL3PlusStateCacheManager::invalidateStateForTexture(GLuint texture)
    {
        mTexUnitsMap.erase(texture);
    }

    // TODO: Store as high/low bits of a GLuint, use vector instead of map for TexParameteriMap
    void GL3PlusStateCacheManager::setTexParameteri(GLenum target, GLenum pname, GLint param)
    {
        // Check if we have a map entry for this texture id. If not, create a blank one and insert it.
        TexUnitsMap::iterator it = mTexUnitsMap.find(mLastBoundTexID);
        if (it == mTexUnitsMap.end())
        {
            TextureUnitParams unit;
            mTexUnitsMap[mLastBoundTexID] = unit;
            
            // Update the iterator
            it = mTexUnitsMap.find(mLastBoundTexID);
        }
        
        // Get a local copy of the parameter map and search for this parameter
        TexParameteriMap &myMap = (*it).second.mTexParameteriMap;
        TexParameteriMap::iterator i = myMap.find(pname);
        
        if (i == myMap.end())
        {
            // Haven't cached this state yet.  Insert it into the map
            myMap.insert(TexParameteriMap::value_type(pname, param));
            
            // Update GL
            OGRE_CHECK_GL_ERROR(glTexParameteri(target, pname, param));
        }
        else
        {
            // Update the cached value if needed
            if((*i).second != param)
            {
                (*i).second = param;
                
                // Update GL
                OGRE_CHECK_GL_ERROR(glTexParameteri(target, pname, param));
            }
        }
    }
    
    void GL3PlusStateCacheManager::bindGLTexture(GLenum target, GLuint texture)
    {
        mLastBoundTexID = texture;
        
        // Update GL
        OGRE_CHECK_GL_ERROR(glBindTexture(target, texture));
    }
    
    bool GL3PlusStateCacheManager::activateGLTextureUnit(size_t unit)
    {
        if (mActiveTextureUnit != unit)
        {
            if (unit < static_cast<GL3PlusRenderSystem*>(Root::getSingleton().getRenderSystem())->getCapabilities()->getNumTextureUnits())
            {
                OGRE_CHECK_GL_ERROR(glActiveTexture(GL_TEXTURE0 + unit));
                mActiveTextureUnit = unit;
                return true;
            }
            else if (!unit)
            {
                // always ok to use the first unit
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return true;
        }
    }
    
    // TODO: Store as high/low bits of a GLuint
    void GL3PlusStateCacheManager::setBlendFunc(GLenum source, GLenum dest)
    {
        if(mBlendFuncSource != source || mBlendFuncDest != dest)
        {
            mBlendFuncSource = source;
            mBlendFuncDest = dest;
            
            glBlendFunc(source, dest);
        }
    }

    void GL3PlusStateCacheManager::setDepthMask(GLboolean mask)
    {
        if(mDepthMask != mask)
        {
            mDepthMask = mask;
            
            OGRE_CHECK_GL_ERROR(glDepthMask(mask));
        }
    }
    
    void GL3PlusStateCacheManager::setDepthFunc(GLenum func)
    {
        if(mDepthFunc != func)
        {
            mDepthFunc = func;
            
            OGRE_CHECK_GL_ERROR(glDepthFunc(func));
        }
    }
    
    void GL3PlusStateCacheManager::setClearDepth(GLclampf depth)
    {
        if(mClearDepth != depth)
        {
            mClearDepth = depth;
            
            OGRE_CHECK_GL_ERROR(glClearDepth(depth));
        }
    }
    
    void GL3PlusStateCacheManager::setClearColour(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
    {
        if((mClearColour[0] != red) ||
           (mClearColour[1] != green) ||
           (mClearColour[2] != blue) ||
           (mClearColour[3] != alpha))
        {
            mClearColour[0] = red;
            mClearColour[1] = green;
            mClearColour[2] = blue;
            mClearColour[3] = alpha;
            
            OGRE_CHECK_GL_ERROR(glClearColor(mClearColour[0], mClearColour[1], mClearColour[2], mClearColour[3]));
        }
    }
    
    void GL3PlusStateCacheManager::setColourMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
    {
        if((mColourMask[0] != red) ||
           (mColourMask[1] != green) ||
           (mColourMask[2] != blue) ||
           (mColourMask[3] != alpha))
        {
            mColourMask[0] = red;
            mColourMask[1] = green;
            mColourMask[2] = blue;
            mColourMask[3] = alpha;
            
            OGRE_CHECK_GL_ERROR(glColorMask(mColourMask[0], mColourMask[1], mColourMask[2], mColourMask[3]));
        }
    }
    
    void GL3PlusStateCacheManager::setStencilMask(GLuint mask)
    {
        if(mStencilMask != mask)
        {
            mStencilMask = mask;
            
            OGRE_CHECK_GL_ERROR(glStencilMask(mask));
        }
    }
    
    void GL3PlusStateCacheManager::setEnabled(GLenum flag, bool enabled)
    {
        if (mBoolStateMap[flag] == enabled)
        {
            OGRE_CHECK_GL_ERROR(glDisable(flag));
        }
        else
        {
            OGRE_CHECK_GL_ERROR(glEnable(flag));
        }
    }

    void GL3PlusStateCacheManager::setViewport(GLint x, GLint y, GLsizei width, GLsizei height)
    {
        if((mViewport[0] != x) ||
           (mViewport[1] != y) ||
           (mViewport[2] != width) ||
           (mViewport[3] != height))
        {
            mViewport[0] = x;
            mViewport[1] = y;
            mViewport[2] = width;
            mViewport[3] = height;
            OGRE_CHECK_GL_ERROR(glViewport(x, y, width, height));
        }
    }

    void GL3PlusStateCacheManager::getViewport(int *array)
    {
        for (int i = 0; i < 4; ++i)
            array[i] = mViewport[i];
    }

    void GL3PlusStateCacheManager::setCullFace(GLenum face)
    {
        if(mCullFace != face)
        {
            mCullFace = face;
            
            glCullFace(face);
        }
    }

    void GL3PlusStateCacheManager::setBlendEquation(GLenum eq)
    {
        if(mBlendEquationRGB != eq || mBlendEquationAlpha != eq)
        {
            mBlendEquationRGB = eq;
            mBlendEquationAlpha = eq;

            OGRE_CHECK_GL_ERROR(glBlendEquation(eq));
        }
    }

    void GL3PlusStateCacheManager::setBlendEquation(GLenum eqRGB, GLenum eqAlpha)
    {
        if(mBlendEquationRGB != eqRGB || mBlendEquationAlpha != eqAlpha)
        {
            mBlendEquationRGB = eqRGB;
            mBlendEquationAlpha = eqAlpha;

            OGRE_CHECK_GL_ERROR(glBlendEquationSeparate(eqRGB, eqAlpha));
        }
    }

    void GL3PlusStateCacheManager::setPolygonMode(GLenum mode)
    {
        if (mPolygonMode != mode)
        {
            mPolygonMode = mode;
            OGRE_CHECK_GL_ERROR(glPolygonMode(GL_FRONT_AND_BACK, mPolygonMode));
        }
    }


    void GL3PlusStateCacheManager::setPointSize(GLfloat size)
    {
        if (mPointSize != size)
        {
            mPointSize = size;
            OGRE_CHECK_GL_ERROR(glPointSize(mPointSize));
        }
    }
    
    void GL3PlusStateCacheManager::enableTextureCoordGen(GLenum type)
    {
        OGRE_HashMap<GLenum, TexGenParams>::iterator it = mTextureCoordGen.find(mActiveTextureUnit);
        if (it == mTextureCoordGen.end())
        {
            OGRE_CHECK_GL_ERROR(glEnable(type));
            mTextureCoordGen[mActiveTextureUnit].mEnabled.insert(type);
        }
        else
        {
            if (it->second.mEnabled.find(type) == it->second.mEnabled.end())
            {
                OGRE_CHECK_GL_ERROR(glEnable(type));
                it->second.mEnabled.insert(type);
            }
        }
    }

    void GL3PlusStateCacheManager::disableTextureCoordGen(GLenum type)
    {
        OGRE_HashMap<GLenum, TexGenParams>::iterator it = mTextureCoordGen.find(mActiveTextureUnit);
        if (it != mTextureCoordGen.end())
        {
            std::set<GLenum>::iterator found = it->second.mEnabled.find(type);
            if (found != it->second.mEnabled.end())
            {
                OGRE_CHECK_GL_ERROR(glDisable(type));
                it->second.mEnabled.erase(found);
            }
        }
    }
}
