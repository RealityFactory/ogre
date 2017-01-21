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

#include "OgreGLSLESProgramCommon.h"
#include "OgreGLSLESProgram.h"
#include "OgreGpuProgramManager.h"
#include "OgreGLUtil.h"
#include "OgreGLES2RenderSystem.h"
#include "OgreRoot.h"
#include "OgreGLES2Support.h"

namespace Ogre {
    
    //-----------------------------------------------------------------------
    GLSLESProgramCommon::GLSLESProgramCommon(GLSLESProgram* vertexProgram, GLSLESProgram* fragmentProgram)
    : GLSLProgramCommon(vertexProgram)
    , mFragmentProgram(fragmentProgram)
    {
        // Initialise uniform cache
        mUniformCache = new GLES2UniformCache();
    }
    
    //-----------------------------------------------------------------------
    GLSLESProgramCommon::~GLSLESProgramCommon(void)
    {
        OGRE_CHECK_GL_ERROR(glDeleteProgram(mGLProgramHandle));

        delete mUniformCache;
        mUniformCache = 0;
    }
    
    //-----------------------------------------------------------------------
    Ogre::String GLSLESProgramCommon::getCombinedName()
    {
        String name;
        if (getVertexProgram())
        {
            name += "Vertex Program:" ;
            name += getVertexProgram()->getName();
        }
        if (mFragmentProgram)
        {
            name += " Fragment Program:" ;
            name += mFragmentProgram->getName();
        }
        name += "\n";

        return name;
    }
    
    //-----------------------------------------------------------------------
    GLint GLSLESProgramCommon::getAttributeIndex(VertexElementSemantic semantic, uint index)
    {
        GLint res = mCustomAttributesIndexes[semantic-1][index];
        if (res == NULL_CUSTOM_ATTRIBUTES_INDEX)
        {
            const char * attString = getAttributeSemanticString(semantic);
            GLint attrib;
            OGRE_CHECK_GL_ERROR(attrib = glGetAttribLocation(mGLProgramHandle, attString));

            // sadly position is a special case 
            if (attrib == NOT_FOUND_CUSTOM_ATTRIBUTES_INDEX && semantic == VES_POSITION)
            {
                OGRE_CHECK_GL_ERROR(attrib = glGetAttribLocation(mGLProgramHandle, "position"));
            }

            // for uv and other case the index is a part of the name
            if (attrib == NOT_FOUND_CUSTOM_ATTRIBUTES_INDEX)
            {
                String attStringWithSemantic = String(attString) + StringConverter::toString(index);
                OGRE_CHECK_GL_ERROR(attrib = glGetAttribLocation(mGLProgramHandle, attStringWithSemantic.c_str()));
            }

            // update mCustomAttributesIndexes with the index we found (or didn't find) 
            mCustomAttributesIndexes[semantic-1][index] = attrib;
            res = attrib;
        }
        return res;
    }
    //-----------------------------------------------------------------------
    bool GLSLESProgramCommon::getMicrocodeFromCache(const String& name, GLuint programHandle)
    {
        if (!GpuProgramManager::getSingleton().canGetCompiledShaderBuffer())
            return false;

        if (!GpuProgramManager::getSingleton().isMicrocodeAvailableInCache(name))
            return false;

        GpuProgramManager::Microcode cacheMicrocode = GpuProgramManager::getSingleton().getMicrocodeFromCache(name);

        // turns out we need this param when loading
        GLenum binaryFormat = 0;

        cacheMicrocode->seek(0);

        // get size of binary
        cacheMicrocode->read(&binaryFormat, sizeof(GLenum));

        if(!Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_CAN_GET_COMPILED_SHADER_BUFFER))
            return false;

        GLint binaryLength = static_cast<GLint>(cacheMicrocode->size() - sizeof(GLenum));

        // load binary
        OGRE_CHECK_GL_ERROR(glProgramBinaryOES(programHandle,
                           binaryFormat,
                           cacheMicrocode->getPtr(),
                           binaryLength));

        GLint success = 0;
        OGRE_CHECK_GL_ERROR(glGetProgramiv(programHandle, GL_LINK_STATUS, &success));

        return success;
    }
    void GLSLESProgramCommon::_writeToCache(const String& name, GLuint programHandle)
    {
        if(!Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_CAN_GET_COMPILED_SHADER_BUFFER))
            return;

        if(!GpuProgramManager::getSingleton().getSaveMicrocodesToCache())
            return;

        // Add to the microcode to the cache
        // Get buffer size
        GLint binaryLength = 0;
        OGRE_CHECK_GL_ERROR(glGetProgramiv(programHandle, GL_PROGRAM_BINARY_LENGTH_OES, &binaryLength));

        // Create microcode
        GpuProgramManager::Microcode newMicrocode =
            GpuProgramManager::getSingleton().createMicrocode(static_cast<uint32>(binaryLength + sizeof(GLenum)));

        // Get binary
        OGRE_CHECK_GL_ERROR(glGetProgramBinaryOES(programHandle, binaryLength, NULL, (GLenum *)newMicrocode->getPtr(),
                                                  newMicrocode->getPtr() + sizeof(GLenum)));

        // Add to the microcode to the cache
        GpuProgramManager::getSingleton().addMicrocodeToCache(name, newMicrocode);
    }
} // namespace Ogre
