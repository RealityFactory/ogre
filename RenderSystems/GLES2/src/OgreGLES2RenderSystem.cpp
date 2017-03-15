/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

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
#define NOMINMAX
#include "OgreGLES2RenderSystem.h"
#include "OgreGLES2TextureManager.h"
#include "OgreGLES2DepthBuffer.h"
#include "OgreGLES2HardwarePixelBuffer.h"
#include "OgreGLES2HardwareBufferManager.h"
#include "OgreGLES2HardwareIndexBuffer.h"
#include "OgreGLES2HardwareVertexBuffer.h"
#include "OgreGLES2GpuProgramManager.h"
#include "OgreGLUtil.h"
#include "OgreGLES2FBORenderTexture.h"
#include "OgreGLES2HardwareOcclusionQuery.h"
#include "OgreGLES2VertexDeclaration.h"
#include "OgreGLSLESProgramFactory.h"
#include "OgreRoot.h"
#include "OgreViewport.h"
#include "OgreFrustum.h"
#if !OGRE_NO_GLES2_CG_SUPPORT
#include "OgreGLSLESCgProgramFactory.h"
#endif
#include "OgreGLSLESLinkProgram.h"
#include "OgreGLSLESLinkProgramManager.h"
#include "OgreGLSLESProgramPipelineManager.h"
#include "OgreGLSLESProgramPipeline.h"
#include "OgreGLES2StateCacheManager.h"
#include "OgreRenderWindow.h"
#include "OgreGLES2PixelFormat.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
#include "OgreEAGLES2Context.h"
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID || OGRE_PLATFORM == OGRE_PLATFORM_EMSCRIPTEN
#   include "OgreGLES2ManagedResourceManager.h"
Ogre::GLES2ManagedResourceManager* Ogre::GLES2RenderSystem::mResourceManager = NULL;
#endif

// Convenience macro from ARB_vertex_buffer_object spec
#define VBO_BUFFER_OFFSET(i) ((char *)NULL + (i))

#ifndef GL_PACK_ROW_LENGTH_NV
#define GL_PACK_ROW_LENGTH_NV             0x0D02
#endif

using namespace std;

static void gl2ext_to_gl3core() {
    glUnmapBufferOES = glUnmapBuffer;
    glRenderbufferStorageMultisampleAPPLE = glRenderbufferStorageMultisample;

    glGenQueriesEXT = glGenQueries;
    glDeleteQueriesEXT = glDeleteQueries;
    glBeginQueryEXT = glBeginQuery;
    glEndQueryEXT = glEndQuery;
    glGetQueryObjectuivEXT = glGetQueryObjectuiv;

    glMapBufferRangeEXT = glMapBufferRange;
    glFlushMappedBufferRangeEXT = glFlushMappedBufferRange;

    glTexImage3DOES = (PFNGLTEXIMAGE3DOESPROC)glTexImage3D;
    glCompressedTexImage3DOES = glCompressedTexImage3D;
    glTexSubImage3DOES = glTexSubImage3D;
    glCompressedTexSubImage3DOES = glCompressedTexSubImage3D;

    glFenceSyncAPPLE = glFenceSync;
    glClientWaitSyncAPPLE = glClientWaitSync;
    glDeleteSyncAPPLE = glDeleteSync;

    glProgramBinaryOES = glProgramBinary;
    glGetProgramBinaryOES = glGetProgramBinary;

    glDrawElementsInstancedEXT = glDrawElementsInstanced;
    glDrawArraysInstancedEXT = glDrawArraysInstanced;
    glVertexAttribDivisorEXT = glVertexAttribDivisor;
    glBindVertexArrayOES = glBindVertexArray;
    glGenVertexArraysOES = glGenVertexArrays;
    glDeleteVertexArraysOES = glDeleteVertexArrays;
}

namespace Ogre {

#if OGRE_PLATFORM != OGRE_PLATFORM_APPLE_IOS
    static GLES2Support* glsupport;
    static GLESWglProc get_proc(const char* proc) {
        return (GLESWglProc)glsupport->getProcAddress(proc);
    }
#endif

    GLES2RenderSystem::GLES2RenderSystem()
        : mGpuProgramManager(0),
          mGLSLESProgramFactory(0),
          mHardwareBufferManager(0),
          mRTTManager(0),
          mCurTexMipCount(0)
    {
        size_t i;

        LogManager::getSingleton().logMessage(getName() + " created.");

        mRenderAttribsBound.reserve(100);
        mRenderInstanceAttribsBound.reserve(100);

        mEnableFixedPipeline = false;

#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID || OGRE_PLATFORM == OGRE_PLATFORM_EMSCRIPTEN
        mResourceManager = OGRE_NEW GLES2ManagedResourceManager();
#endif
        
        mStateCacheManager = OGRE_NEW GLES2StateCacheManager();
        mGLSupport = new GLES2Support(getGLSupport(GLNativeSupport::CONTEXT_ES));
        mGLSupport->setStateCacheManager(mStateCacheManager);
        
#if OGRE_PLATFORM != OGRE_PLATFORM_APPLE_IOS
        glsupport = mGLSupport;
#endif

        mGLSupport->addConfig();

        for (i = 0; i < OGRE_MAX_TEXTURE_LAYERS; i++)
        {
            // Dummy value
            mTextureTypes[i] = 0;
        }

        mActiveRenderTarget = 0;
        mCurrentContext = 0;
        mMainContext = 0;
        mGLInitialised = false;
        mMinFilter = FO_LINEAR;
        mMipFilter = FO_POINT;
        mCurrentVertexProgram = 0;
        mCurrentFragmentProgram = 0;
    }

    GLES2RenderSystem::~GLES2RenderSystem()
    {
        shutdown();

        // Destroy render windows
        RenderTargetMap::iterator i;
        for (i = mRenderTargets.begin(); i != mRenderTargets.end(); ++i)
        {
            OGRE_DELETE i->second;
        }

        mRenderTargets.clear();
        OGRE_DELETE mGLSupport;
        mGLSupport = 0;

        OGRE_DELETE mStateCacheManager;
        mStateCacheManager = 0;

#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID || OGRE_PLATFORM == OGRE_PLATFORM_EMSCRIPTEN
        if (mResourceManager != NULL)
        {
            OGRE_DELETE mResourceManager;
            mResourceManager = NULL;
        }
#endif
    }

    const String& GLES2RenderSystem::getName(void) const
    {
        static String strName("OpenGL ES 2.x Rendering Subsystem");
        return strName;
    }

    ConfigOptionMap& GLES2RenderSystem::getConfigOptions(void)
    {
        return mGLSupport->getConfigOptions();
    }

    void GLES2RenderSystem::setConfigOption(const String &name, const String &value)
    {
        mGLSupport->setConfigOption(name, value);
    }

    String GLES2RenderSystem::validateConfigOptions(void)
    {
        // XXX Return an error string if something is invalid
        return mGLSupport->validateConfig();
    }

    RenderWindow* GLES2RenderSystem::_initialise(bool autoCreateWindow,
                                                 const String& windowTitle)
    {
        mGLSupport->start();

        // Create the texture manager
        mTextureManager = OGRE_NEW GLES2TextureManager(*mGLSupport); 

        RenderWindow *autoWindow = mGLSupport->createWindow(autoCreateWindow,
                                                            this, windowTitle);
        RenderSystem::_initialise(autoCreateWindow, windowTitle);
        return autoWindow;
    }

    RenderSystemCapabilities* GLES2RenderSystem::createRenderSystemCapabilities() const
    {
        RenderSystemCapabilities* rsc = OGRE_NEW RenderSystemCapabilities();

        rsc->setCategoryRelevant(CAPS_CATEGORY_GL, true);
        rsc->setDriverVersion(mDriverVersion);

        const char* deviceName = (const char*)glGetString(GL_RENDERER);
        if (deviceName)
        {
            rsc->setDeviceName(deviceName);
        }

        rsc->setRenderSystemName(getName());
        rsc->parseVendorFromString(mGLSupport->getGLVendor());

        // Multitexturing support and set number of texture units
        GLint units;
        OGRE_CHECK_GL_ERROR(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units));
        rsc->setNumTextureUnits(std::min<ushort>(16, units));

        // Check for hardware stencil support and set bit depth
        GLint stencil;

        OGRE_CHECK_GL_ERROR(glGetIntegerv(GL_STENCIL_BITS, &stencil));

        if(stencil)
        {
            rsc->setCapability(RSC_HWSTENCIL);
            rsc->setCapability(RSC_TWO_SIDED_STENCIL);
            rsc->setStencilBufferBitDepth(stencil);
        }

        if(mHasGLES30 ||
                (mGLSupport->checkExtension("GL_EXT_sRGB")
                 && mGLSupport->checkExtension("GL_NV_sRGB_formats")))
            rsc->setCapability(RSC_HW_GAMMA);

        // Scissor test is standard
        rsc->setCapability(RSC_SCISSOR_TEST);

        // Vertex Buffer Objects are always supported by OpenGL ES
        rsc->setCapability(RSC_VBO);
        if(mGLSupport->checkExtension("GL_OES_element_index_uint") || mHasGLES30)
            rsc->setCapability(RSC_32BIT_INDEX);

        // Check for hardware occlusion support
        if(mGLSupport->checkExtension("GL_EXT_occlusion_query_boolean") || mHasGLES30)
        {
            rsc->setCapability(RSC_HWOCCLUSION);
        }

        // OpenGL ES - Check for these extensions too
        // For 2.0, http://www.khronos.org/registry/gles/api/2.0/gl2ext.h

        if (mGLSupport->checkExtension("GL_IMG_texture_compression_pvrtc") ||
            mGLSupport->checkExtension("GL_EXT_texture_compression_dxt1") ||
            mGLSupport->checkExtension("GL_EXT_texture_compression_s3tc") ||
            mGLSupport->checkExtension("GL_OES_compressed_ETC1_RGB8_texture") ||
            mGLSupport->checkExtension("GL_AMD_compressed_ATC_texture") ||
            mGLSupport->checkExtension("WEBGL_compressed_texture_s3tc") ||
            mGLSupport->checkExtension("WEBGL_compressed_texture_atc") ||
            mGLSupport->checkExtension("WEBGL_compressed_texture_pvrtc") ||
            mGLSupport->checkExtension("WEBGL_compressed_texture_etc1"))

        {
            rsc->setCapability(RSC_TEXTURE_COMPRESSION);

            if(mGLSupport->checkExtension("GL_IMG_texture_compression_pvrtc") ||
               mGLSupport->checkExtension("GL_IMG_texture_compression_pvrtc2") ||
               mGLSupport->checkExtension("WEBGL_compressed_texture_pvrtc"))
                rsc->setCapability(RSC_TEXTURE_COMPRESSION_PVRTC);
                
            if((mGLSupport->checkExtension("GL_EXT_texture_compression_dxt1") &&
               mGLSupport->checkExtension("GL_EXT_texture_compression_s3tc")) ||
               mGLSupport->checkExtension("WEBGL_compressed_texture_s3tc"))
                rsc->setCapability(RSC_TEXTURE_COMPRESSION_DXT);

            if(mGLSupport->checkExtension("GL_OES_compressed_ETC1_RGB8_texture") ||
               mGLSupport->checkExtension("WEBGL_compressed_texture_etc1"))
                rsc->setCapability(RSC_TEXTURE_COMPRESSION_ETC1);

            if(mHasGLES30)
                rsc->setCapability(RSC_TEXTURE_COMPRESSION_ETC2);

            if(mGLSupport->checkExtension("GL_AMD_compressed_ATC_texture") ||
               mGLSupport->checkExtension("WEBGL_compressed_texture_atc"))
                rsc->setCapability(RSC_TEXTURE_COMPRESSION_ATC);
        }

        if (mGLSupport->checkExtension("GL_EXT_texture_filter_anisotropic"))
            rsc->setCapability(RSC_ANISOTROPY);

        rsc->setCapability(RSC_FBO);
        rsc->setCapability(RSC_HWRENDER_TO_TEXTURE);
#if OGRE_NO_GLES3_SUPPORT == 0
        // Probe number of draw buffers
        // Only makes sense with FBO support, so probe here
        GLint buffers;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &buffers);
        rsc->setNumMultiRenderTargets(std::min<int>(buffers, (GLint)OGRE_MAX_MULTIPLE_RENDER_TARGETS));
        rsc->setCapability(RSC_MRT_DIFFERENT_BIT_DEPTHS);
#else
        rsc->setNumMultiRenderTargets(1);
#endif

        // Cube map
        rsc->setCapability(RSC_CUBEMAPPING);

        // Stencil wrapping
        rsc->setCapability(RSC_STENCIL_WRAP);

        // GL always shares vertex and fragment texture units (for now?)
        rsc->setVertexTextureUnitsShared(true);

        // Hardware support mipmapping
        rsc->setCapability(RSC_AUTOMIPMAP);

        // Blending support
        rsc->setCapability(RSC_BLENDING);
        rsc->setCapability(RSC_ADVANCED_BLEND_OPERATIONS);

        // DOT3 support is standard
        rsc->setCapability(RSC_DOT3);
        
        // Point size
        GLfloat psRange[2] = {0.0, 0.0};
        OGRE_CHECK_GL_ERROR(glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, psRange));
        rsc->setMaxPointSize(psRange[1]);

        if(mGLSupport->checkExtension("GL_EXT_texture_filter_anisotropic"))
        {
            // Max anisotropy
            GLfloat aniso = 0;
            OGRE_CHECK_GL_ERROR(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso));
            rsc->setMaxSupportedAnisotropy(aniso);
        }

        // Point sprites
        rsc->setCapability(RSC_POINT_SPRITES);
        rsc->setCapability(RSC_POINT_EXTENDED_PARAMETERS);
        
        // GLSL ES is always supported in GL ES 2
        rsc->addShaderProfile("glsles");
        if (getNativeShadingLanguageVersion() >= 320)
            rsc->addShaderProfile("glsl320es");
        if (getNativeShadingLanguageVersion() >= 310)
            rsc->addShaderProfile("glsl310es");
        if (getNativeShadingLanguageVersion() >= 300)
            rsc->addShaderProfile("glsl300es");

#if !OGRE_NO_GLES2_CG_SUPPORT
        rsc->addShaderProfile("cg");
        rsc->addShaderProfile("ps_2_0");
        rsc->addShaderProfile("vs_2_0");
#endif

        // UBYTE4 always supported
        rsc->setCapability(RSC_VERTEX_FORMAT_UBYTE4);

        // Infinite far plane always supported
        rsc->setCapability(RSC_INFINITE_FAR_PLANE);

        // Vertex/Fragment Programs
        rsc->setCapability(RSC_VERTEX_PROGRAM);
        rsc->setCapability(RSC_FRAGMENT_PROGRAM);

        // Separate shader objects
#if OGRE_PLATFORM != OGRE_PLATFORM_NACL
        OGRE_IF_IOS_VERSION_IS_GREATER_THAN(5.0)
        if(mGLSupport->checkExtension("GL_EXT_separate_shader_objects"))
        {
            rsc->setCapability(RSC_SEPARATE_SHADER_OBJECTS);
            rsc->setCapability(RSC_GLSL_SSO_REDECLARE);
        }

        // Mesa 11.2 does not behave according to spec and throws a "gl_Position redefined"
        if(rsc->getDeviceName().find("Mesa") != String::npos) {
            rsc->unsetCapability(RSC_GLSL_SSO_REDECLARE);
        }
#endif

        GLfloat floatConstantCount = 0;
#if OGRE_NO_GLES3_SUPPORT == 0
        glGetFloatv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &floatConstantCount);
#else
        glGetFloatv(GL_MAX_VERTEX_UNIFORM_VECTORS, &floatConstantCount);
#endif
        rsc->setVertexProgramConstantFloatCount((Ogre::ushort)floatConstantCount);
        rsc->setVertexProgramConstantBoolCount((Ogre::ushort)floatConstantCount);
        rsc->setVertexProgramConstantIntCount((Ogre::ushort)floatConstantCount);

        // Fragment Program Properties
        floatConstantCount = 0;
#if OGRE_NO_GLES3_SUPPORT == 0
        glGetFloatv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &floatConstantCount);
#else
        glGetFloatv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &floatConstantCount);
#endif
        rsc->setFragmentProgramConstantFloatCount((Ogre::ushort)floatConstantCount);
        rsc->setFragmentProgramConstantBoolCount((Ogre::ushort)floatConstantCount);
        rsc->setFragmentProgramConstantIntCount((Ogre::ushort)floatConstantCount);

        // Check for Float textures
        if(mGLSupport->checkExtension("GL_OES_texture_float") || mGLSupport->checkExtension("GL_OES_texture_half_float") || mHasGLES30)
            rsc->setCapability(RSC_TEXTURE_FLOAT);

        rsc->setCapability(RSC_TEXTURE_1D);

        if(mHasGLES30 || mGLSupport->checkExtension("GL_OES_texture_3D"))
            rsc->setCapability(RSC_TEXTURE_3D);

        // ES 3 always supports NPOT textures
        if(mGLSupport->checkExtension("GL_OES_texture_npot") || mGLSupport->checkExtension("GL_ARB_texture_non_power_of_two") || mHasGLES30)
        {
            rsc->setCapability(RSC_NON_POWER_OF_2_TEXTURES);
            rsc->setNonPOW2TexturesLimited(false);
        }
        else if(mGLSupport->checkExtension("GL_APPLE_texture_2D_limited_npot"))
        {
            rsc->setNonPOW2TexturesLimited(true);
        }

        // Alpha to coverage always 'supported' when MSAA is available
        // although card may ignore it if it doesn't specifically support A2C
        rsc->setCapability(RSC_ALPHA_TO_COVERAGE);
        
        // No point sprites, so no size
        rsc->setMaxPointSize(0.f);
        
        if(mHasGLES30 || mGLSupport->checkExtension("GL_OES_vertex_array_object"))
            rsc->setCapability(RSC_VAO);

        if (mHasGLES30 || mGLSupport->checkExtension("GL_OES_get_program_binary"))
        {
            // http://www.khronos.org/registry/gles/extensions/OES/OES_get_program_binary.txt
            GLint formats;
            OGRE_CHECK_GL_ERROR(glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS_OES, &formats));

            if(formats > 0)
                rsc->setCapability(RSC_CAN_GET_COMPILED_SHADER_BUFFER);
        }

        if (mGLSupport->checkExtension("GL_EXT_instanced_arrays") || mHasGLES30)
        {
            rsc->setCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA);
        }
        else if(mGLSupport->checkExtension("GL_ANGLE_instanced_arrays"))
        {
            rsc->setCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA);
            glDrawElementsInstancedEXT = glDrawElementsInstancedANGLE;
            glDrawArraysInstancedEXT = glDrawArraysInstancedANGLE;
            glVertexAttribDivisorEXT = glVertexAttribDivisorANGLE;
        }

        if (mGLSupport->checkExtension("GL_EXT_debug_marker") &&
            mGLSupport->checkExtension("GL_EXT_debug_label"))
        {
            OGRE_IF_IOS_VERSION_IS_GREATER_THAN(5.0)
            rsc->setCapability(RSC_DEBUG);
        }

#if OGRE_NO_GLES3_SUPPORT == 0
        // Check if render to vertex buffer (transform feedback in OpenGL)
        rsc->setCapability(RSC_HWRENDER_TO_VERTEX_BUFFER);
#endif
        return rsc;
    }

    void GLES2RenderSystem::initialiseFromRenderSystemCapabilities(RenderSystemCapabilities* caps, RenderTarget* primary)
    {
        if(caps->getRenderSystemName() != getName())
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "Trying to initialize GLES2RenderSystem from RenderSystemCapabilities that do not support OpenGL ES",
                        "GLES2RenderSystem::initialiseFromRenderSystemCapabilities");
        }

        mGpuProgramManager = OGRE_NEW GLES2GpuProgramManager();

        mGLSLESProgramFactory = OGRE_NEW GLSLESProgramFactory();
        HighLevelGpuProgramManager::getSingleton().addFactory(mGLSLESProgramFactory);

#if !OGRE_NO_GLES2_CG_SUPPORT
        mGLSLESCgProgramFactory = OGRE_NEW GLSLESCgProgramFactory();
        HighLevelGpuProgramManager::getSingleton().addFactory(mGLSLESCgProgramFactory);
#endif

        // Use VBO's by default
        mHardwareBufferManager = OGRE_NEW GLES2HardwareBufferManager();

        // Create FBO manager
        LogManager::getSingleton().logMessage("GL ES 2: Using FBOs for rendering to textures");
        mRTTManager = new GLES2FBOManager();
        caps->setCapability(RSC_RTT_SEPARATE_DEPTHBUFFER);

        Log* defaultLog = LogManager::getSingleton().getDefaultLog();
        if (defaultLog)
        {
            caps->log(defaultLog);
        }

        mGLInitialised = true;
    }

    void GLES2RenderSystem::shutdown(void)
    {

        // Deleting the GLSL program factory
        if (mGLSLESProgramFactory)
        {
            // Remove from manager safely
            if (HighLevelGpuProgramManager::getSingletonPtr())
                HighLevelGpuProgramManager::getSingleton().removeFactory(mGLSLESProgramFactory);
            OGRE_DELETE mGLSLESProgramFactory;
            mGLSLESProgramFactory = 0;
        }

#if !OGRE_NO_GLES2_CG_SUPPORT
        // Deleting the GLSL program factory
        if (mGLSLESCgProgramFactory)
        {
            // Remove from manager safely
            if (HighLevelGpuProgramManager::getSingletonPtr())
                HighLevelGpuProgramManager::getSingleton().removeFactory(mGLSLESCgProgramFactory);
            OGRE_DELETE mGLSLESCgProgramFactory;
            mGLSLESCgProgramFactory = 0;
        }
#endif
        // Deleting the GPU program manager and hardware buffer manager.  Has to be done before the mGLSupport->stop().
        OGRE_DELETE mGpuProgramManager;
        mGpuProgramManager = 0;

        OGRE_DELETE mHardwareBufferManager;
        mHardwareBufferManager = 0;

        delete mRTTManager;
        mRTTManager = 0;

        OGRE_DELETE mTextureManager;
        mTextureManager = 0;

        // Delete extra threads contexts
        for (GLES2ContextList::iterator i = mBackgroundContextList.begin();
             i != mBackgroundContextList.end(); ++i)
        {
            GLES2Context* pCurContext = *i;

            pCurContext->releaseContext();

            delete pCurContext;
        }

        mBackgroundContextList.clear();

        RenderSystem::shutdown();

        mGLSupport->stop();

        mGLInitialised = 0;
    }

    RenderWindow* GLES2RenderSystem::_createRenderWindow(const String &name, unsigned int width, unsigned int height,
                                                        bool fullScreen, const NameValuePairList *miscParams)
    {
        if (mRenderTargets.find(name) != mRenderTargets.end())
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "NativeWindowType with name '" + name + "' already exists",
                        "GLES2RenderSystem::_createRenderWindow");
        }

        // Log a message
        StringStream ss;
        ss << "GLES2RenderSystem::_createRenderWindow \"" << name << "\", " <<
            width << "x" << height << " ";
        if (fullScreen)
            ss << "fullscreen ";
        else
            ss << "windowed ";

        if (miscParams)
        {
            ss << " miscParams: ";
            NameValuePairList::const_iterator it;
            for (it = miscParams->begin(); it != miscParams->end(); ++it)
            {
                ss << it->first << "=" << it->second << " ";
            }

            LogManager::getSingleton().logMessage(ss.str());
        }

        // Create the window
        RenderWindow* win = mGLSupport->newWindow(name, width, height, fullScreen, miscParams);
        attachRenderTarget((Ogre::RenderTarget&) *win);

        if (!mGLInitialised)
        {
            initialiseContext(win);
            mDriverVersion = mGLSupport->getGLVersion();

            // Get the shader language version
            const char* shadingLangVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
            LogManager::getSingleton().logMessage("Shading language version: " + String(shadingLangVersion));
            StringVector tokens = StringUtil::split(shadingLangVersion, ". ");
            size_t i = 0;

            // iOS reports the GLSL version with a whole bunch of non-digit characters so we have to find where the version starts.
            for(; i < tokens.size(); i++)
            {
                if (isdigit(*tokens[i].c_str()))
                    break;
            }
            mNativeShadingLanguageVersion = (StringConverter::parseUnsignedInt(tokens[i]) * 100) + StringConverter::parseUnsignedInt(tokens[i+1]);
            if (mNativeShadingLanguageVersion < 100) // Emscripten + MS IE/Edge reports an experimental WebGL version (e.g. 0.96) which causes a compile error
                mNativeShadingLanguageVersion = 100;

            // Initialise GL after the first window has been created
            // TODO: fire this from emulation options, and don't duplicate Real and Current capabilities
            mRealCapabilities = createRenderSystemCapabilities();

            // use real capabilities if custom capabilities are not available
            if (!mUseCustomCapabilities)
                mCurrentCapabilities = mRealCapabilities;

            fireEvent("RenderSystemCapabilitiesCreated");

            initialiseFromRenderSystemCapabilities(mCurrentCapabilities, (RenderTarget *) win);

            // Initialise the main context
            _oneTimeContextInitialization();
            if (mCurrentContext)
                mCurrentContext->setInitialized();
        }

        if( win->getDepthBufferPool() != DepthBuffer::POOL_NO_DEPTH )
        {
            // Unlike D3D9, OGL doesn't allow sharing the main depth buffer, so keep them separate.
            // Only Copy does, but Copy means only one depth buffer...
            GLES2Context *windowContext = 0;
            win->getCustomAttribute( "GLCONTEXT", &windowContext );
            GLES2DepthBuffer *depthBuffer = OGRE_NEW GLES2DepthBuffer( DepthBuffer::POOL_DEFAULT, this,
                                                            windowContext, 0, 0,
                                                            win->getWidth(), win->getHeight(),
                                                            win->getFSAA(), 0, true );

            mDepthBufferPool[depthBuffer->getPoolId()].push_back( depthBuffer );

            win->attachDepthBuffer( depthBuffer );
        }

        return win;
    }

    //---------------------------------------------------------------------
    DepthBuffer* GLES2RenderSystem::_createDepthBufferFor( RenderTarget *renderTarget )
    {
        GLES2DepthBuffer *retVal = 0;

        // Only FBO & pbuffer support different depth buffers, so everything
        // else creates dummy (empty) containers
        // retVal = mRTTManager->_createDepthBufferFor( renderTarget );
        GLES2FrameBufferObject *fbo = 0;
        renderTarget->getCustomAttribute("FBO", &fbo);

        if( fbo )
        {
            // Presence of an FBO means the manager is an FBO Manager, that's why it's safe to downcast
            // Find best depth & stencil format suited for the RT's format
            GLuint depthFormat, stencilFormat;
            static_cast<GLES2FBOManager*>(mRTTManager)->getBestDepthStencil( fbo->getFormat(),
                                                                        &depthFormat, &stencilFormat );

            GLES2RenderBuffer *depthBuffer = OGRE_NEW GLES2RenderBuffer( depthFormat, fbo->getWidth(),
                                                                fbo->getHeight(), fbo->getFSAA() );

            GLES2RenderBuffer *stencilBuffer = depthBuffer;
            if( 
#if OGRE_NO_GLES3_SUPPORT == 0
               depthFormat != GL_DEPTH32F_STENCIL8 &&
#endif
               depthFormat != GL_DEPTH24_STENCIL8_OES &&
               stencilFormat )
            {
                stencilBuffer = OGRE_NEW GLES2RenderBuffer( stencilFormat, fbo->getWidth(),
                                                           fbo->getHeight(), fbo->getFSAA() );
            } else {
                stencilBuffer = NULL;
            }

            // No "custom-quality" multisample for now in GL
            retVal = OGRE_NEW GLES2DepthBuffer( 0, this, mCurrentContext, depthBuffer, stencilBuffer,
                                        fbo->getWidth(), fbo->getHeight(), fbo->getFSAA(), 0, false );
        }

        return retVal;
    }
    //---------------------------------------------------------------------
    void GLES2RenderSystem::_getDepthStencilFormatFor( PixelFormat internalColourFormat, GLenum *depthFormat,
                                                      GLenum *stencilFormat )
    {
        mRTTManager->getBestDepthStencil( internalColourFormat, depthFormat, stencilFormat );
    }

    MultiRenderTarget* GLES2RenderSystem::createMultiRenderTarget(const String & name)
    {
        MultiRenderTarget *retval = mRTTManager->createMultiRenderTarget(name);
        attachRenderTarget(*retval);
        return retval;
    }

    void GLES2RenderSystem::destroyRenderWindow(const String& name)
    {
        // Find it to remove from list.
        RenderTarget* pWin = detachRenderTarget(name);
        OgreAssert(pWin, "unknown RenderWindow name");

        _destroyDepthBuffer(pWin);
        OGRE_DELETE pWin;
    }

    void GLES2RenderSystem::_destroyDepthBuffer(RenderTarget* pWin)
    {
        GLES2Context *windowContext = 0;
        pWin->getCustomAttribute("GLCONTEXT", &windowContext);
        
        // 1 Window <-> 1 Context, should be always true
        assert( windowContext );
        
        bool bFound = false;
        // Find the depth buffer from this window and remove it.
        DepthBufferMap::iterator itMap = mDepthBufferPool.begin();
        DepthBufferMap::iterator enMap = mDepthBufferPool.end();
        
        while( itMap != enMap && !bFound )
        {
            DepthBufferVec::iterator itor = itMap->second.begin();
            DepthBufferVec::iterator end  = itMap->second.end();
            
            while( itor != end )
            {
                // A DepthBuffer with no depth & stencil pointers is a dummy one,
                // look for the one that matches the same GL context
                GLES2DepthBuffer *depthBuffer = static_cast<GLES2DepthBuffer*>(*itor);
                GLES2Context *glContext = depthBuffer->getGLContext();
                
                if( glContext == windowContext &&
                   (depthBuffer->getDepthBuffer() || depthBuffer->getStencilBuffer()) )
                {
                    bFound = true;
                    
                    delete *itor;
                    itMap->second.erase( itor );
                    break;
                }
                ++itor;
            }
            
            ++itMap;
        }
    }

    void GLES2RenderSystem::_setTexture(size_t stage, bool enabled, const TexturePtr &texPtr)
    {
        GLES2TexturePtr tex = static_pointer_cast<GLES2Texture>(texPtr);

        if (!mStateCacheManager->activateGLTextureUnit(stage))
            return;

        if (enabled)
        {
            mCurTexMipCount = 0;
            GLuint texID =  0;
            if (tex)
            {
                // Note used
                tex->touch();
                mTextureTypes[stage] = tex->getGLES2TextureTarget();
                texID = tex->getGLID();
                mCurTexMipCount = tex->getNumMipmaps();
            }
            else
            {
                // Assume 2D
                mTextureTypes[stage] = GL_TEXTURE_2D;
                texID = static_cast<GLES2TextureManager*>(mTextureManager)->getWarningTextureID();
            }

            mStateCacheManager->bindGLTexture(mTextureTypes[stage], texID);
        }
        else
        {
            // Bind zero texture
            mStateCacheManager->bindGLTexture(GL_TEXTURE_2D, 0);
        }

        mStateCacheManager->activateGLTextureUnit(0);
    }

    void GLES2RenderSystem::_setTextureCoordSet(size_t stage, size_t index)
    {
    }

    GLint GLES2RenderSystem::getTextureAddressingMode(TextureUnitState::TextureAddressingMode tam) const
    {
        switch (tam)
        {
            case TextureUnitState::TAM_CLAMP:
            case TextureUnitState::TAM_BORDER:
                return GL_CLAMP_TO_EDGE;
            case TextureUnitState::TAM_MIRROR:
                return GL_MIRRORED_REPEAT;
            case TextureUnitState::TAM_WRAP:
            default:
                return GL_REPEAT;
        }
    }

    void GLES2RenderSystem::_setTextureAddressingMode(size_t stage, const TextureUnitState::UVWAddressingMode& uvw)
    {
        if (!mStateCacheManager->activateGLTextureUnit(stage))
            return;

        mStateCacheManager->setTexParameteri(mTextureTypes[stage], GL_TEXTURE_WRAP_S, getTextureAddressingMode(uvw.u));
        mStateCacheManager->setTexParameteri(mTextureTypes[stage], GL_TEXTURE_WRAP_T, getTextureAddressingMode(uvw.v));

        if(getCapabilities()->hasCapability(RSC_TEXTURE_3D))
            mStateCacheManager->setTexParameteri(mTextureTypes[stage], GL_TEXTURE_WRAP_R_OES, getTextureAddressingMode(uvw.w));
        mStateCacheManager->activateGLTextureUnit(0);
    }

    GLenum GLES2RenderSystem::getBlendMode(SceneBlendFactor ogreBlend) const
    {
        switch (ogreBlend)
        {
            case SBF_ONE:
                return GL_ONE;
            case SBF_ZERO:
                return GL_ZERO;
            case SBF_DEST_COLOUR:
                return GL_DST_COLOR;
            case SBF_SOURCE_COLOUR:
                return GL_SRC_COLOR;
            case SBF_ONE_MINUS_DEST_COLOUR:
                return GL_ONE_MINUS_DST_COLOR;
            case SBF_ONE_MINUS_SOURCE_COLOUR:
                return GL_ONE_MINUS_SRC_COLOR;
            case SBF_DEST_ALPHA:
                return GL_DST_ALPHA;
            case SBF_SOURCE_ALPHA:
                return GL_SRC_ALPHA;
            case SBF_ONE_MINUS_DEST_ALPHA:
                return GL_ONE_MINUS_DST_ALPHA;
            case SBF_ONE_MINUS_SOURCE_ALPHA:
                return GL_ONE_MINUS_SRC_ALPHA;
        };

        // To keep compiler happy
        return GL_ONE;
    }

    void GLES2RenderSystem::_setSceneBlending(SceneBlendFactor sourceFactor, SceneBlendFactor destFactor, SceneBlendOperation op)
    {
        GLenum sourceBlend = getBlendMode(sourceFactor);
        GLenum destBlend = getBlendMode(destFactor);
        if(sourceFactor == SBF_ONE && destFactor == SBF_ZERO)
        {
            mStateCacheManager->setDisabled(GL_BLEND);
        }
        else
        {
            mStateCacheManager->setEnabled(GL_BLEND);
            mStateCacheManager->setBlendFunc(sourceBlend, destBlend);
        }
        
        GLint func = GL_FUNC_ADD;
        switch(op)
        {
        case SBO_ADD:
            func = GL_FUNC_ADD;
            break;
        case SBO_SUBTRACT:
            func = GL_FUNC_SUBTRACT;
            break;
        case SBO_REVERSE_SUBTRACT:
            func = GL_FUNC_REVERSE_SUBTRACT;
            break;
        case SBO_MIN:
            if(mGLSupport->checkExtension("GL_EXT_blend_minmax") || mHasGLES30)
                func = GL_MIN_EXT;
            break;
        case SBO_MAX:
            if(mGLSupport->checkExtension("GL_EXT_blend_minmax") || mHasGLES30)
                func = GL_MAX_EXT;
            break;
        }

        mStateCacheManager->setBlendEquation(func);
    }

    void GLES2RenderSystem::_setSeparateSceneBlending(
        SceneBlendFactor sourceFactor, SceneBlendFactor destFactor,
        SceneBlendFactor sourceFactorAlpha, SceneBlendFactor destFactorAlpha,
        SceneBlendOperation op, SceneBlendOperation alphaOp )
    {
        GLenum sourceBlend = getBlendMode(sourceFactor);
        GLenum destBlend = getBlendMode(destFactor);
        GLenum sourceBlendAlpha = getBlendMode(sourceFactorAlpha);
        GLenum destBlendAlpha = getBlendMode(destFactorAlpha);
        
        if(sourceFactor == SBF_ONE && destFactor == SBF_ZERO && 
           sourceFactorAlpha == SBF_ONE && destFactorAlpha == SBF_ZERO)
        {
            mStateCacheManager->setDisabled(GL_BLEND);
        }
        else
        {
            mStateCacheManager->setEnabled(GL_BLEND);
            OGRE_CHECK_GL_ERROR(glBlendFuncSeparate(sourceBlend, destBlend, sourceBlendAlpha, destBlendAlpha));
        }
        
        GLint func = GL_FUNC_ADD, alphaFunc = GL_FUNC_ADD;
        
        switch(op)
        {
            case SBO_ADD:
                func = GL_FUNC_ADD;
                break;
            case SBO_SUBTRACT:
                func = GL_FUNC_SUBTRACT;
                break;
            case SBO_REVERSE_SUBTRACT:
                func = GL_FUNC_REVERSE_SUBTRACT;
                break;
            case SBO_MIN:
                if(mGLSupport->checkExtension("GL_EXT_blend_minmax") || mHasGLES30)
                    func = GL_MIN_EXT;
                break;
            case SBO_MAX:
                if(mGLSupport->checkExtension("GL_EXT_blend_minmax") || mHasGLES30)
                    func = GL_MAX_EXT;
                break;
        }
        
        switch(alphaOp)
        {
            case SBO_ADD:
                alphaFunc = GL_FUNC_ADD;
                break;
            case SBO_SUBTRACT:
                alphaFunc = GL_FUNC_SUBTRACT;
                break;
            case SBO_REVERSE_SUBTRACT:
                alphaFunc = GL_FUNC_REVERSE_SUBTRACT;
                break;
            case SBO_MIN:
                if(mGLSupport->checkExtension("GL_EXT_blend_minmax") || mHasGLES30)
                    alphaFunc = GL_MIN_EXT;
                break;
            case SBO_MAX:
                if(mGLSupport->checkExtension("GL_EXT_blend_minmax") || mHasGLES30)
                    alphaFunc = GL_MAX_EXT;
                break;
        }
        
        OGRE_CHECK_GL_ERROR(glBlendEquationSeparate(func, alphaFunc));
    }

    void GLES2RenderSystem::_setAlphaRejectSettings(CompareFunction func, unsigned char value, bool alphaToCoverage)
    {
        bool a2c = false;
        static bool lasta2c = false;

        if(func != CMPF_ALWAYS_PASS)
        {
            a2c = alphaToCoverage;
        }

        if (a2c != lasta2c && getCapabilities()->hasCapability(RSC_ALPHA_TO_COVERAGE))
        {
            if (a2c)
                mStateCacheManager->setEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
            else
                mStateCacheManager->setDisabled(GL_SAMPLE_ALPHA_TO_COVERAGE);

            lasta2c = a2c;
        }
    }

    void GLES2RenderSystem::_setViewport(Viewport *vp)
    {
        // Check if viewport is different
        if (!vp)
        {
            mActiveViewport = NULL;
            _setRenderTarget(NULL);
        }
        else if (vp != mActiveViewport || vp->_isUpdated())
        {
            RenderTarget* target;
            
            target = vp->getTarget();
            _setRenderTarget(target);
            mActiveViewport = vp;
            
            GLsizei x, y, w, h;
            
            // Calculate the "lower-left" corner of the viewport
            w = vp->getActualWidth();
            h = vp->getActualHeight();
            x = vp->getActualLeft();
            y = vp->getActualTop();
            
            if (!target->requiresTextureFlipping())
            {
                // Convert "upper-left" corner to "lower-left"
                y = target->getHeight() - h - y;
            }
            
#if OGRE_NO_VIEWPORT_ORIENTATIONMODE == 0
            ConfigOptionMap::const_iterator opt;
            ConfigOptionMap::const_iterator end = mGLSupport->getConfigOptions().end();
            
            if ((opt = mGLSupport->getConfigOptions().find("Orientation")) != end)
            {
                String val = opt->second.currentValue;
                String::size_type pos = val.find("Landscape");
                
                if (pos != String::npos)
                {
                    GLsizei temp = h;
                    h = w;
                    w = temp;
                }
            }
#endif
            if(mViewport[0] != x || mViewport[1] != y ||
               mViewport[2] != w || mViewport[3] != h)
            {
                mViewport[0] = x; mViewport[1] = y;
                mViewport[2] = w; mViewport[3] = h;
                OGRE_CHECK_GL_ERROR(glViewport(x, y, w, h));
            }

            if(mScissor[0] != x || mScissor[1] != y ||
               mScissor[2] != w || mScissor[3] != h)
            {
                // Configure the viewport clipping
                mScissor[0] = x; mScissor[1] = y;
                mScissor[2] = w; mScissor[3] = h;
                OGRE_CHECK_GL_ERROR(glScissor(x, y, w, h));
            }
            
            vp->_clearUpdatedFlag();
        }
    }

    void GLES2RenderSystem::_beginFrame(void)
    {
        if (!mActiveViewport)
            OGRE_EXCEPT(Exception::ERR_INVALID_STATE,
                        "Cannot begin frame - no viewport selected.",
                        "GLES2RenderSystem::_beginFrame");

        mStateCacheManager->setEnabled(GL_SCISSOR_TEST);
    }

    void GLES2RenderSystem::_endFrame(void)
    {
        // Deactivate the viewport clipping.
        mStateCacheManager->setDisabled(GL_SCISSOR_TEST);

        // unbind GPU programs at end of frame
        // this is mostly to avoid holding bound programs that might get deleted
        // outside via the resource manager
        unbindGpuProgram(GPT_VERTEX_PROGRAM);
		unbindGpuProgram(GPT_FRAGMENT_PROGRAM);

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        static_cast<EAGLES2Context*>(mMainContext)->bindSampleFramebuffer();
#endif
    }

    void GLES2RenderSystem::setVertexDeclaration(VertexDeclaration* decl)
    {
        OGRE_EXCEPT( Exception::ERR_INTERNAL_ERROR, 
                    "Cannot directly call setVertexDeclaration in the GLES2 render system - cast then use 'setVertexDeclaration(VertexDeclaration* decl, VertexBufferBinding* binding)' .", 
                    "GLES2RenderSystem::setVertexDeclaration" );
    }

    void GLES2RenderSystem::setVertexDeclaration(VertexDeclaration* decl, VertexBufferBinding* binding)
    {
        GLES2VertexDeclaration* gles2decl = 
            static_cast<GLES2VertexDeclaration*>(decl);

        if(gles2decl)
            gles2decl->bind();
    }

    void GLES2RenderSystem::_setCullingMode(CullingMode mode)
    {
        mCullingMode = mode;
        // NB: Because two-sided stencil API dependence of the front face, we must
        // use the same 'winding' for the front face everywhere. As the OGRE default
        // culling mode is clockwise, we also treat anticlockwise winding as front
        // face for consistently. On the assumption that, we can't change the front
        // face by glFrontFace anywhere.

        GLenum cullMode;

        switch( mode )
        {
            case CULL_NONE:
                mStateCacheManager->setDisabled(GL_CULL_FACE);
                return;

            default:
            case CULL_CLOCKWISE:
                if (mActiveRenderTarget &&
                    ((mActiveRenderTarget->requiresTextureFlipping() && !mInvertVertexWinding) ||
                     (!mActiveRenderTarget->requiresTextureFlipping() && mInvertVertexWinding)))
                {
                    cullMode = GL_FRONT;
                }
                else
                {
                    cullMode = GL_BACK;
                }
                break;
            case CULL_ANTICLOCKWISE:
                if (mActiveRenderTarget && 
                    ((mActiveRenderTarget->requiresTextureFlipping() && !mInvertVertexWinding) ||
                    (!mActiveRenderTarget->requiresTextureFlipping() && mInvertVertexWinding)))
                {
                    cullMode = GL_BACK;
                }
                else
                {
                    cullMode = GL_FRONT;
                }
                break;
        }

        mStateCacheManager->setEnabled(GL_CULL_FACE);
        mStateCacheManager->setCullFace(cullMode);
    }

    void GLES2RenderSystem::_setDepthBufferParams(bool depthTest, bool depthWrite, CompareFunction depthFunction)
    {
        _setDepthBufferCheckEnabled(depthTest);
        _setDepthBufferWriteEnabled(depthWrite);
        _setDepthBufferFunction(depthFunction);
    }

    void GLES2RenderSystem::_setDepthBufferCheckEnabled(bool enabled)
    {
        if (enabled)
        {
            mStateCacheManager->setClearDepth(1.0f);
            mStateCacheManager->setEnabled(GL_DEPTH_TEST);
        }
        else
        {
            mStateCacheManager->setDisabled(GL_DEPTH_TEST);
        }
    }

    void GLES2RenderSystem::_setDepthBufferWriteEnabled(bool enabled)
    {
        // Store for reference in _beginFrame
        mStateCacheManager->setDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    void GLES2RenderSystem::_setDepthBufferFunction(CompareFunction func)
    {
        mStateCacheManager->setDepthFunc(convertCompareFunction(func));
    }

    void GLES2RenderSystem::_setDepthBias(float constantBias, float slopeScaleBias)
    {
        if (constantBias != 0 || slopeScaleBias != 0)
        {
            mStateCacheManager->setEnabled(GL_POLYGON_OFFSET_FILL);
            OGRE_CHECK_GL_ERROR(glPolygonOffset(-slopeScaleBias, -constantBias));
        }
        else
        {
            mStateCacheManager->setDisabled(GL_POLYGON_OFFSET_FILL);
        }
    }

    void GLES2RenderSystem::_setColourBufferWriteEnabled(bool red, bool green, bool blue, bool alpha)
    {
        mStateCacheManager->setColourMask(red, green, blue, alpha);
    }

    //---------------------------------------------------------------------
    HardwareOcclusionQuery* GLES2RenderSystem::createHardwareOcclusionQuery(void)
    {
        if(mGLSupport->checkExtension("GL_EXT_occlusion_query_boolean") || mHasGLES30)
        {
            GLES2HardwareOcclusionQuery* ret = new GLES2HardwareOcclusionQuery(); 
            mHwOcclusionQueries.push_back(ret);
            return ret;
        }
        else
        {
            return NULL;
        }
    }

    void GLES2RenderSystem::_setPolygonMode(PolygonMode level)
    {
        switch(level)
        {
        case PM_POINTS:
            mStateCacheManager->setPolygonMode(GL_POINTS);
            break;
        case PM_WIREFRAME:
            mStateCacheManager->setPolygonMode(GL_LINE_STRIP);
            break;
        default:
        case PM_SOLID:
            mStateCacheManager->setPolygonMode(GL_FILL);
            break;
        }
    }

    void GLES2RenderSystem::setStencilCheckEnabled(bool enabled)
    {
        if (enabled)
        {
            mStateCacheManager->setEnabled(GL_STENCIL_TEST);
        }
        else
        {
            mStateCacheManager->setDisabled(GL_STENCIL_TEST);
        }
    }

    void GLES2RenderSystem::setStencilBufferParams(CompareFunction func,
                                                uint32 refValue, uint32 compareMask, uint32 writeMask,
                                                StencilOperation stencilFailOp,
                                                StencilOperation depthFailOp,
                                                StencilOperation passOp,
                                                bool twoSidedOperation,
                                                bool readBackAsTexture)
    {
        bool flip = false;

        if (twoSidedOperation)
        {
            if (!mCurrentCapabilities->hasCapability(RSC_TWO_SIDED_STENCIL))
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "2-sided stencils are not supported",
                            "GLES2RenderSystem::setStencilBufferParams");
            
            // NB: We should always treat CCW as front face for consistent with default
            // culling mode. Therefore, we must take care with two-sided stencil settings.
            flip = (mInvertVertexWinding && !mActiveRenderTarget->requiresTextureFlipping()) ||
            (!mInvertVertexWinding && mActiveRenderTarget->requiresTextureFlipping());
            // Back
            OGRE_CHECK_GL_ERROR(glStencilMaskSeparate(GL_BACK, writeMask));
            OGRE_CHECK_GL_ERROR(glStencilFuncSeparate(GL_BACK, convertCompareFunction(func), refValue, compareMask));
            OGRE_CHECK_GL_ERROR(glStencilOpSeparate(GL_BACK,
                                                    convertStencilOp(stencilFailOp, !flip),
                                                    convertStencilOp(depthFailOp, !flip), 
                                                    convertStencilOp(passOp, !flip)));

            // Front
            OGRE_CHECK_GL_ERROR(glStencilMaskSeparate(GL_FRONT, writeMask));
            OGRE_CHECK_GL_ERROR(glStencilFuncSeparate(GL_FRONT, convertCompareFunction(func), refValue, compareMask));
            OGRE_CHECK_GL_ERROR(glStencilOpSeparate(GL_FRONT,
                                                    convertStencilOp(stencilFailOp, flip),
                                                    convertStencilOp(depthFailOp, flip), 
                                                    convertStencilOp(passOp, flip)));
        }
        else
        {
            flip = false;
            mStateCacheManager->setStencilMask(writeMask);
            OGRE_CHECK_GL_ERROR(glStencilFunc(convertCompareFunction(func), refValue, compareMask));
            OGRE_CHECK_GL_ERROR(glStencilOp(
                                            convertStencilOp(stencilFailOp, flip),
                                            convertStencilOp(depthFailOp, flip), 
                                            convertStencilOp(passOp, flip)));
        }
    }

    GLint GLES2RenderSystem::getCombinedMinMipFilter(void) const
    {
        switch(mMinFilter)
        {
            case FO_ANISOTROPIC:
            case FO_LINEAR:
                switch (mMipFilter)
                {
                    case FO_ANISOTROPIC:
                    case FO_LINEAR:
                        // linear min, linear mip
                        return GL_LINEAR_MIPMAP_LINEAR;
                    case FO_POINT:
                        // linear min, point mip
                        return GL_LINEAR_MIPMAP_NEAREST;
                    case FO_NONE:
                        // linear min, no mip
                        return GL_LINEAR;
                }
                break;
            case FO_POINT:
            case FO_NONE:
                switch (mMipFilter)
                {
                    case FO_ANISOTROPIC:
                    case FO_LINEAR:
                        // nearest min, linear mip
                        return GL_NEAREST_MIPMAP_LINEAR;
                    case FO_POINT:
                        // nearest min, point mip
                        return GL_NEAREST_MIPMAP_NEAREST;
                    case FO_NONE:
                        // nearest min, no mip
                        return GL_NEAREST;
                }
                break;
        }

        // should never get here
        return 0;
    }

    void GLES2RenderSystem::_setTextureUnitFiltering(size_t unit, FilterOptions minFilter,
                FilterOptions magFilter, FilterOptions mipFilter)
    {       
        mMipFilter = mipFilter;
        if(mCurTexMipCount == 0 && mMipFilter != FO_NONE)
        {
            mMipFilter = FO_NONE;           
        }
        _setTextureUnitFiltering(unit, FT_MAG, magFilter);
        _setTextureUnitFiltering(unit, FT_MIN, minFilter);
    }
                
    void GLES2RenderSystem::_setTextureUnitFiltering(size_t unit, FilterType ftype, FilterOptions fo)
    {
        if (!mStateCacheManager->activateGLTextureUnit(unit))
            return;

        switch (ftype)
        {
            case FT_MIN:
                mMinFilter = fo;
                // Combine with existing mip filter
                mStateCacheManager->setTexParameteri(mTextureTypes[unit],
                                GL_TEXTURE_MIN_FILTER,
                                getCombinedMinMipFilter());
                break;
            case FT_MAG:
                switch (fo)
                {
                    case FO_ANISOTROPIC: // GL treats linear and aniso the same
                    case FO_LINEAR:
                        mStateCacheManager->setTexParameteri(mTextureTypes[unit],
                                        GL_TEXTURE_MAG_FILTER,
                                        GL_LINEAR);
                        break;
                    case FO_POINT:
                    case FO_NONE:
                        mStateCacheManager->setTexParameteri(mTextureTypes[unit],
                                        GL_TEXTURE_MAG_FILTER,
                                        GL_NEAREST);
                        break;
                }
                break;
            case FT_MIP:
                mMipFilter = fo;

                // Combine with existing min filter
                mStateCacheManager->setTexParameteri(mTextureTypes[unit],
                                                     GL_TEXTURE_MIN_FILTER,
                                                     getCombinedMinMipFilter());
                
                break;
        }

        mStateCacheManager->activateGLTextureUnit(0);
    }

    GLfloat GLES2RenderSystem::_getCurrentAnisotropy(size_t unit)
    {
        GLfloat curAniso = 0;
        if(mGLSupport->checkExtension("GL_EXT_texture_filter_anisotropic"))
            mStateCacheManager->getTexParameterfv(mTextureTypes[unit],
                                                  GL_TEXTURE_MAX_ANISOTROPY_EXT, &curAniso);

        return curAniso ? curAniso : 1;
    }
    
    void GLES2RenderSystem::_setTextureLayerAnisotropy(size_t unit, unsigned int maxAnisotropy)
    {
        if (!mCurrentCapabilities->hasCapability(RSC_ANISOTROPY))
            return;

        if (!mStateCacheManager->activateGLTextureUnit(unit))
            return;

        if(mGLSupport->checkExtension("GL_EXT_texture_filter_anisotropic"))
        {
            if (maxAnisotropy > mCurrentCapabilities->getMaxSupportedAnisotropy())
                maxAnisotropy = mCurrentCapabilities->getMaxSupportedAnisotropy() ? 
                static_cast<uint>(mCurrentCapabilities->getMaxSupportedAnisotropy()) : 1;

            mStateCacheManager->setTexParameterf(mTextureTypes[unit],
                                                  GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)maxAnisotropy);
        }

        mStateCacheManager->activateGLTextureUnit(0);
    }

    void GLES2RenderSystem::_render(const RenderOperation& op)
    {
        // Call super class
        RenderSystem::_render(op);

        HardwareVertexBufferSharedPtr globalInstanceVertexBuffer;
        VertexDeclaration* globalVertexDeclaration = 0;
        bool hasInstanceData = false;
        size_t numberOfInstances = 0;
        if(getCapabilities()->hasCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA))
        {
            globalInstanceVertexBuffer = getGlobalInstanceVertexBuffer();
            globalVertexDeclaration = getGlobalInstanceVertexBufferVertexDeclaration();
            hasInstanceData = (op.useGlobalInstancingVertexBufferIsAvailable &&
                                globalInstanceVertexBuffer && (globalVertexDeclaration != NULL))
                                || op.vertexData->vertexBufferBinding->getHasInstanceData();

            numberOfInstances = op.numberOfInstances;

            if (op.useGlobalInstancingVertexBufferIsAvailable)
            {
                numberOfInstances *= getGlobalNumberOfInstances();
            }
        }

        void* pBufferData = 0;

        const VertexDeclaration::VertexElementList& decl =
            op.vertexData->vertexDeclaration->getElements();
        VertexDeclaration::VertexElementList::const_iterator elemIter, elemEnd;
        elemEnd = decl.end();
        GLES2VertexDeclaration* gles2decl = 
            static_cast<GLES2VertexDeclaration*>(op.vertexData->vertexDeclaration);

        // Use a little shorthand
        bool useVAO = (gles2decl && gles2decl->isInitialised());

        if(useVAO)
            setVertexDeclaration(op.vertexData->vertexDeclaration, op.vertexData->vertexBufferBinding);

        for (elemIter = decl.begin(); elemIter != elemEnd; ++elemIter)
        {
            const VertexElement & elem = *elemIter;
            unsigned short elemSource = elem.getSource();

            if (!op.vertexData->vertexBufferBinding->isBufferBound(elemSource))
                continue; // skip unbound elements
 
            HardwareVertexBufferSharedPtr vertexBuffer =
                op.vertexData->vertexBufferBinding->getBuffer(elemSource);
            bindVertexElementToGpu(elem, vertexBuffer, op.vertexData->vertexStart,
                                   mRenderAttribsBound, mRenderInstanceAttribsBound, true);
        }

        if(getCapabilities()->hasCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA))
        {
            if( globalInstanceVertexBuffer && globalVertexDeclaration != NULL )
            {
                elemEnd = globalVertexDeclaration->getElements().end();
                for (elemIter = globalVertexDeclaration->getElements().begin(); elemIter != elemEnd; ++elemIter)
                {
                    const VertexElement & elem = *elemIter;
                    bindVertexElementToGpu(elem, globalInstanceVertexBuffer, 0,
                                           mRenderAttribsBound, mRenderInstanceAttribsBound, true);
                    continue;
                }
            }
        }

        // Find the correct type to render
        GLint primType;
        switch (op.operationType)
        {
            case RenderOperation::OT_POINT_LIST:
                primType = GL_POINTS;
                break;
            case RenderOperation::OT_LINE_LIST:
                primType = GL_LINES;
                break;
            case RenderOperation::OT_LINE_STRIP:
                primType = GL_LINE_STRIP;
                break;
            default:
            case RenderOperation::OT_TRIANGLE_LIST:
                primType = GL_TRIANGLES;
                break;
            case RenderOperation::OT_TRIANGLE_STRIP:
                primType = GL_TRIANGLE_STRIP;
                break;
            case RenderOperation::OT_TRIANGLE_FAN:
                primType = GL_TRIANGLE_FAN;
                break;
        }

        GLenum polyMode = mStateCacheManager->getPolygonMode();
        if (op.useIndexes)
        {
            // If we are using VAO's then only bind the buffer the first time through. Otherwise, always bind.
            if (!useVAO || (useVAO && gles2decl && !gles2decl->isInitialised()))
                mStateCacheManager->bindGLBuffer(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLES2HardwareIndexBuffer*>(op.indexData->indexBuffer.get())->getGLBufferId());

            pBufferData = VBO_BUFFER_OFFSET(op.indexData->indexStart *
                                            op.indexData->indexBuffer->getIndexSize());

            GLenum indexType = (op.indexData->indexBuffer->getType() == HardwareIndexBuffer::IT_16BIT) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

            do
            {
                // Update derived depth bias
                if (mDerivedDepthBias && mCurrentPassIterationNum > 0)
                {
                    _setDepthBias(mDerivedDepthBiasBase +
                                  mDerivedDepthBiasMultiplier * mCurrentPassIterationNum,
                                  mDerivedDepthBiasSlopeScale);
                }

                if(hasInstanceData && getCapabilities()->hasCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA))
                {
                    OGRE_CHECK_GL_ERROR(glDrawElementsInstancedEXT((polyMode == GL_FILL) ? primType : polyMode, static_cast<GLsizei>(op.indexData->indexCount), indexType, pBufferData, static_cast<GLsizei>(numberOfInstances)));
                }
                else
                {
#if OGRE_NO_GLES3_SUPPORT == 0
                    GLuint indexEnd = op.indexData->indexCount - op.indexData->indexStart;
                    OGRE_CHECK_GL_ERROR(glDrawRangeElements((polyMode == GL_FILL) ? primType : polyMode, op.indexData->indexStart, indexEnd, static_cast<GLsizei>(op.indexData->indexCount), indexType, pBufferData));
#else
                    OGRE_CHECK_GL_ERROR(glDrawElements((polyMode == GL_FILL) ? primType : polyMode, static_cast<GLsizei>(op.indexData->indexCount), indexType, pBufferData));
#endif
                }

            } while (updatePassIterationRenderState());
        }
        else
        {
            do
            {
                // Update derived depth bias
                if (mDerivedDepthBias && mCurrentPassIterationNum > 0)
                {
                    _setDepthBias(mDerivedDepthBiasBase +
                                  mDerivedDepthBiasMultiplier * mCurrentPassIterationNum,
                                  mDerivedDepthBiasSlopeScale);
                }

                if(getCapabilities()->hasCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA) && hasInstanceData)
                {
                    OGRE_CHECK_GL_ERROR(glDrawArraysInstancedEXT((polyMode == GL_FILL) ? primType : polyMode, 0, static_cast<GLsizei>(op.vertexData->vertexCount), static_cast<GLsizei>(numberOfInstances)));
                }
                else
                {
                    OGRE_CHECK_GL_ERROR(glDrawArrays((polyMode == GL_FILL) ? primType : polyMode, 0, static_cast<GLsizei>(op.vertexData->vertexCount)));
                }
            } while (updatePassIterationRenderState());
        }

        if (useVAO && gles2decl && !gles2decl->isInitialised())
        {
            gles2decl->setInitialised(true);
        }

        if(getCapabilities()->hasCapability(RSC_VAO))
            // Unbind the vertex array object.  Marks the end of what state will be included.
            OGRE_CHECK_GL_ERROR(glBindVertexArrayOES(0));

        // Unbind all attributes
        for (vector<GLuint>::type::iterator ai = mRenderAttribsBound.begin(); ai != mRenderAttribsBound.end(); ++ai)
        {
            mStateCacheManager->setVertexAttribDisabled(*ai);
//          OGRE_CHECK_GL_ERROR(glDisableVertexAttribArray(*ai));
        }

        // Unbind any instance attributes
        for (vector<GLuint>::type::iterator ai = mRenderInstanceAttribsBound.begin(); ai != mRenderInstanceAttribsBound.end(); ++ai)
        {
            glVertexAttribDivisorEXT(*ai, 0);
        }

        mRenderAttribsBound.clear();
        mRenderInstanceAttribsBound.clear();
    }

    void GLES2RenderSystem::setScissorTest(bool enabled, size_t left,
                                        size_t top, size_t right,
                                        size_t bottom)
    {
        // If request texture flipping, use "upper-left", otherwise use "lower-left"
        bool flipping = mActiveRenderTarget->requiresTextureFlipping();
        //  GL measures from the bottom, not the top
        size_t targetHeight = mActiveRenderTarget->getHeight();
        // Calculate the "lower-left" corner of the viewport
        size_t w, h, x, y;

        if (enabled)
        {
            mStateCacheManager->setEnabled(GL_SCISSOR_TEST);
            // NB GL uses width / height rather than right / bottom
            x = left;
            if (flipping)
                y = top;
            else
                y = targetHeight - bottom;
            w = right - left;
            h = bottom - top;
            OGRE_CHECK_GL_ERROR(glScissor(static_cast<GLsizei>(x),
                                          static_cast<GLsizei>(y),
                                          static_cast<GLsizei>(w),
                                          static_cast<GLsizei>(h)));
        }
        else
        {
            mStateCacheManager->setDisabled(GL_SCISSOR_TEST);
            // GL requires you to reset the scissor when disabling
            w = mActiveViewport->getActualWidth();
            h = mActiveViewport->getActualHeight();
            x = mActiveViewport->getActualLeft();
            if (flipping)
                y = mActiveViewport->getActualTop();
            else
                y = targetHeight - mActiveViewport->getActualTop() - h;
            OGRE_CHECK_GL_ERROR(glScissor(static_cast<GLsizei>(x),
                                          static_cast<GLsizei>(y),
                                          static_cast<GLsizei>(w),
                                          static_cast<GLsizei>(h)));
        }
    }

    void GLES2RenderSystem::clearFrameBuffer(unsigned int buffers,
                                            const ColourValue& colour,
                                            Real depth, unsigned short stencil)
    {
        vector<GLboolean>::type &colourWrite = mStateCacheManager->getColourMask();
        bool colourMask = !colourWrite[0] || !colourWrite[1] ||
                          !colourWrite[2] || !colourWrite[3];
        GLuint stencilMask = mStateCacheManager->getStencilMask();
        GLbitfield flags = 0;

        if (buffers & FBT_COLOUR)
        {
            flags |= GL_COLOR_BUFFER_BIT;
            // Enable buffer for writing if it isn't
            if (colourMask)
            {
                mStateCacheManager->setColourMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            }
            mStateCacheManager->setClearColour(colour.r, colour.g, colour.b, colour.a);
        }
        if (buffers & FBT_DEPTH)
        {
            flags |= GL_DEPTH_BUFFER_BIT;
            // Enable buffer for writing if it isn't
            mStateCacheManager->setDepthMask(GL_TRUE);
            mStateCacheManager->setClearDepth(depth);
        }
        if (buffers & FBT_STENCIL)
        {
            flags |= GL_STENCIL_BUFFER_BIT;
            // Enable buffer for writing if it isn't
            mStateCacheManager->setStencilMask(0xFFFFFFFF);
            OGRE_CHECK_GL_ERROR(glClearStencil(stencil));
        }

        // Should be enable scissor test due the clear region is
        // relied on scissor box bounds.
         mStateCacheManager->setEnabled(GL_SCISSOR_TEST);

        // Sets the scissor box as same as viewport
        GLint viewport[4], scissor[4];
        OGRE_CHECK_GL_ERROR(glGetIntegerv(GL_VIEWPORT, viewport));
        OGRE_CHECK_GL_ERROR(glGetIntegerv(GL_SCISSOR_BOX, scissor));
        bool scissorBoxDifference =
            mViewport[0] != mScissor[0] || mViewport[1] != mScissor[1] ||
            mViewport[2] != mScissor[2] || mViewport[3] != mScissor[3];
        if (scissorBoxDifference)
        {
            OGRE_CHECK_GL_ERROR(glScissor(mViewport[0], mViewport[1], mViewport[2], mViewport[3]));
        }

        mStateCacheManager->setDiscardBuffers(buffers);

        // Clear buffers
        OGRE_CHECK_GL_ERROR(glClear(flags));

        // Restore scissor box
        if (scissorBoxDifference)
        {
            OGRE_CHECK_GL_ERROR(glScissor(mScissor[0], mScissor[1], mScissor[2], mScissor[3]));
        }

        // Restore scissor test
        mStateCacheManager->setDisabled(GL_SCISSOR_TEST);

        // Reset buffer write state
        if (!mStateCacheManager->getDepthMask() && (buffers & FBT_DEPTH))
        {
            mStateCacheManager->setDepthMask(GL_FALSE);
        }

        if (colourMask && (buffers & FBT_COLOUR))
        {
            mStateCacheManager->setColourMask(colourWrite[0], colourWrite[1], colourWrite[2], colourWrite[3]);
        }

        if (buffers & FBT_STENCIL)
        {
            mStateCacheManager->setStencilMask(stencilMask);
        }
    }

    void GLES2RenderSystem::_switchContext(GLES2Context *context)
    {
        // Unbind GPU programs and rebind to new context later, because
        // scene manager treat render system as ONE 'context' ONLY, and it
        // cached the GPU programs using state.
        if (mCurrentVertexProgram)
            mCurrentVertexProgram->unbindProgram();
        if (mCurrentFragmentProgram)
            mCurrentFragmentProgram->unbindProgram();
        
        // Disable textures
        _disableTextureUnitsFrom(0);

        // It's ready for switching
        if(mCurrentContext)
            mCurrentContext->endCurrent();
        mCurrentContext = context;
        mCurrentContext->setCurrent();

        // Check if the context has already done one-time initialisation
        if (!mCurrentContext->getInitialized())
        {
            _oneTimeContextInitialization();
            mCurrentContext->setInitialized();
        }

        // Rebind GPU programs to new context
        if (mCurrentVertexProgram)
            mCurrentVertexProgram->bindProgram();
        if (mCurrentFragmentProgram)
            mCurrentFragmentProgram->bindProgram();
        
        // Must reset depth/colour write mask to according with user desired, otherwise,
        // clearFrameBuffer would be wrong because the value we are recorded may be
        // difference with the really state stored in GL context.
        vector<GLboolean>::type &colourWrite = mStateCacheManager->getColourMask();
        GLuint stencilMask = mStateCacheManager->getStencilMask();
        GLboolean depthMask = mStateCacheManager->getDepthMask();
        mStateCacheManager->setStencilMask(stencilMask);
        mStateCacheManager->setColourMask(colourWrite[0], colourWrite[1], colourWrite[2], colourWrite[3]);
        mStateCacheManager->setDepthMask(depthMask);
    }

    void GLES2RenderSystem::_unregisterContext(GLES2Context *context)
    {
        if (mCurrentContext == context)
        {
            // Change the context to something else so that a valid context
            // remains active. When this is the main context being unregistered,
            // we set the main context to 0.
            if (mCurrentContext != mMainContext)
            {
                _switchContext(mMainContext);
            }
            else
            {
                // No contexts remain
                mCurrentContext->endCurrent();
                mCurrentContext = 0;
                mMainContext = 0;
            }
        }
    }

    void GLES2RenderSystem::_oneTimeContextInitialization()
    {
        mStateCacheManager->setDisabled(GL_DITHER);
        static_cast<GLES2TextureManager*>(mTextureManager)->createWarningTexture();

#if OGRE_NO_GLES3_SUPPORT == 0
        // Enable primitive restarting with fixed indices depending upon the data type
        OGRE_CHECK_GL_ERROR(glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX));
#endif
    }

    void GLES2RenderSystem::initialiseContext(RenderWindow* primary)
    {
        // Set main and current context
        mMainContext = 0;
        primary->getCustomAttribute("GLCONTEXT", &mMainContext);
        mCurrentContext = mMainContext;

        // Set primary context as active
        if (mCurrentContext)
            mCurrentContext->setCurrent();

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
        // EAGL2Support redirects to glesw for get_proc. Overwriting it there would create an infinite loop.
        if (gleswInit())
#else
        if (gleswInit2(get_proc))
#endif
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                        "Could not initialize glesw",
                        "GLES2RenderSystem::initialiseContext");
        }

        // Setup GLSupport
        mGLSupport->initialiseExtensions();

        mHasGLES30 = mGLSupport->hasMinGLVersion(3, 0);

        if(mHasGLES30) {
            gl2ext_to_gl3core();
        }

        LogManager::getSingleton().logMessage("**************************************");
        LogManager::getSingleton().logMessage("*** OpenGL ES 2.x Renderer Started ***");
        LogManager::getSingleton().logMessage("**************************************");
    }

    void GLES2RenderSystem::_setRenderTarget(RenderTarget *target)
    {
        // Unbind frame buffer object
        if(mActiveRenderTarget && mRTTManager)
            mRTTManager->unbind(mActiveRenderTarget);

        mActiveRenderTarget = target;
        if (target && mRTTManager)
        {
            // Switch context if different from current one
            GLES2Context *newContext = 0;
            target->getCustomAttribute("GLCONTEXT", &newContext);
            if (newContext && mCurrentContext != newContext)
            {
                _switchContext(newContext);
            }

            // Check the FBO's depth buffer status
            GLES2DepthBuffer *depthBuffer = static_cast<GLES2DepthBuffer*>(target->getDepthBuffer());

            if( target->getDepthBufferPool() != DepthBuffer::POOL_NO_DEPTH &&
                (!depthBuffer || depthBuffer->getGLContext() != mCurrentContext ) )
            {
                // Depth is automatically managed and there is no depth buffer attached to this RT
                // or the Current context doesn't match the one this Depth buffer was created with
                setDepthBufferFor( target );
            }

            // Bind frame buffer object
            mRTTManager->bind(target);
        }
    }

    GLint GLES2RenderSystem::convertCompareFunction(CompareFunction func) const
    {
        switch(func)
        {
            case CMPF_ALWAYS_FAIL:
                return GL_NEVER;
            case CMPF_ALWAYS_PASS:
                return GL_ALWAYS;
            case CMPF_LESS:
                return GL_LESS;
            case CMPF_LESS_EQUAL:
                return GL_LEQUAL;
            case CMPF_EQUAL:
                return GL_EQUAL;
            case CMPF_NOT_EQUAL:
                return GL_NOTEQUAL;
            case CMPF_GREATER_EQUAL:
                return GL_GEQUAL;
            case CMPF_GREATER:
                return GL_GREATER;
        };
        // To keep compiler happy
        return GL_ALWAYS;
    }

    GLint GLES2RenderSystem::convertStencilOp(StencilOperation op, bool invert) const
    {
        switch(op)
        {
        case SOP_KEEP:
            return GL_KEEP;
        case SOP_ZERO:
            return GL_ZERO;
        case SOP_REPLACE:
            return GL_REPLACE;
        case SOP_INCREMENT:
            return invert ? GL_DECR : GL_INCR;
        case SOP_DECREMENT:
            return invert ? GL_INCR : GL_DECR;
        case SOP_INCREMENT_WRAP:
            return invert ? GL_DECR_WRAP : GL_INCR_WRAP;
        case SOP_DECREMENT_WRAP:
            return invert ? GL_INCR_WRAP : GL_DECR_WRAP;
        case SOP_INVERT:
            return GL_INVERT;
        };
        // to keep compiler happy
        return SOP_KEEP;
    }

    //---------------------------------------------------------------------
    void GLES2RenderSystem::bindGpuProgram(GpuProgram* prg)
    {
        if (!prg)
        {
            OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR, 
                        "Null program bound.",
                        "GLES2RenderSystem::bindGpuProgram");
        }
        
        GLSLESProgram* glprg = static_cast<GLSLESProgram*>(prg);
        
        // Unbind previous gpu program first.
        //
        // Note:
        //  1. Even if both previous and current are the same object, we can't
        //     bypass re-bind completely since the object itself may be modified.
        //     But we can bypass unbind based on the assumption that object
        //     internally GL program type shouldn't be changed after it has
        //     been created. The behavior of bind to a GL program type twice
        //     should be same as unbind and rebind that GL program type, even
        //     for different objects.
        //  2. We also assumed that the program's type (vertex or fragment) should
        //     not be changed during it's in using. If not, the following switch
        //     statement will confuse GL state completely, and we can't fix it
        //     here. To fix this case, we must coding the program implementation
        //     itself, if type is changing (during load/unload, etc), and it's in use,
        //     unbind and notify render system to correct for its state.
        //
        switch (glprg->getType())
        {
            case GPT_VERTEX_PROGRAM:
                if (mCurrentVertexProgram != glprg)
                {
                    if (mCurrentVertexProgram)
                        mCurrentVertexProgram->unbindProgram();
                    mCurrentVertexProgram = glprg;
                }
                break;
                
            case GPT_FRAGMENT_PROGRAM:
                if (mCurrentFragmentProgram != glprg)
                {
                    if (mCurrentFragmentProgram)
                        mCurrentFragmentProgram->unbindProgram();
                    mCurrentFragmentProgram = glprg;
                }
                break;
            default:
                break;
        }
        
        // Bind the program
        glprg->bindProgram();

        RenderSystem::bindGpuProgram(prg);
    }

    void GLES2RenderSystem::unbindGpuProgram(GpuProgramType gptype)
    {
        if (gptype == GPT_VERTEX_PROGRAM && mCurrentVertexProgram)
        {
            mActiveVertexGpuProgramParameters.reset();
            mCurrentVertexProgram->unbindProgram();
            mCurrentVertexProgram = 0;
        }
        else if (gptype == GPT_FRAGMENT_PROGRAM && mCurrentFragmentProgram)
        {
            mActiveFragmentGpuProgramParameters.reset();
            mCurrentFragmentProgram->unbindProgram();
            mCurrentFragmentProgram = 0;
        }
        RenderSystem::unbindGpuProgram(gptype);
    }

    void GLES2RenderSystem::bindGpuProgramParameters(GpuProgramType gptype, GpuProgramParametersSharedPtr params, uint16 mask)
    {
        // Just copy
        params->_copySharedParams();
        switch (gptype)
        {
            case GPT_VERTEX_PROGRAM:
                mActiveVertexGpuProgramParameters = params;
                mCurrentVertexProgram->bindProgramSharedParameters(params, mask);
                break;
            case GPT_FRAGMENT_PROGRAM:
                mActiveFragmentGpuProgramParameters = params;
                mCurrentFragmentProgram->bindProgramSharedParameters(params, mask);
                break;
            default:
                break;
        }

        switch (gptype)
        {
            case GPT_VERTEX_PROGRAM:
                mActiveVertexGpuProgramParameters = params;
                mCurrentVertexProgram->bindProgramParameters(params, mask);
                break;
            case GPT_FRAGMENT_PROGRAM:
                mActiveFragmentGpuProgramParameters = params;
                mCurrentFragmentProgram->bindProgramParameters(params, mask);
                break;
            default:
                break;
        }
    }

    void GLES2RenderSystem::bindGpuProgramPassIterationParameters(GpuProgramType gptype)
    {
        switch (gptype)
        {
            case GPT_VERTEX_PROGRAM:
                mCurrentVertexProgram->bindProgramPassIterationParameters(mActiveVertexGpuProgramParameters);
                break;
            case GPT_FRAGMENT_PROGRAM:
                mCurrentFragmentProgram->bindProgramPassIterationParameters(mActiveFragmentGpuProgramParameters);
                break;
            default:
                break;
        }
    }

    void GLES2RenderSystem::registerThread()
    {
        OGRE_LOCK_MUTEX(mThreadInitMutex);
        // This is only valid once we've created the main context
        if (!mMainContext)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "Cannot register a background thread before the main context "
                        "has been created.",
                        "GLES2RenderSystem::registerThread");
        }

        // Create a new context for this thread. Cloning from the main context
        // will ensure that resources are shared with the main context
        // We want a separate context so that we can safely create GL
        // objects in parallel with the main thread
        GLES2Context* newContext = mMainContext->clone();
        mBackgroundContextList.push_back(newContext);

        // Bind this new context to this thread.
        newContext->setCurrent();

        _oneTimeContextInitialization();
        newContext->setInitialized();
    }

    void GLES2RenderSystem::unregisterThread()
    {
        // nothing to do here?
        // Don't need to worry about active context, just make sure we delete
        // on shutdown.
    }

    void GLES2RenderSystem::preExtraThreadsStarted()
    {
        OGRE_LOCK_MUTEX(mThreadInitMutex);
        // free context, we'll need this to share lists
        if(mCurrentContext)
            mCurrentContext->endCurrent();
    }

    void GLES2RenderSystem::postExtraThreadsStarted()
    {
        OGRE_LOCK_MUTEX(mThreadInitMutex);
        // reacquire context
        if(mCurrentContext)
            mCurrentContext->setCurrent();
    }

    unsigned int GLES2RenderSystem::getDisplayMonitorCount() const
    {
        return 1;
    }

    //---------------------------------------------------------------------
    void GLES2RenderSystem::beginProfileEvent( const String &eventName )
    {
        if(getCapabilities()->hasCapability(RSC_DEBUG))
            glPushGroupMarkerEXT(0, eventName.c_str());
    }
    //---------------------------------------------------------------------
    void GLES2RenderSystem::endProfileEvent( void )
    {
        if(getCapabilities()->hasCapability(RSC_DEBUG))
            glPopGroupMarkerEXT();
    }
    //---------------------------------------------------------------------
    void GLES2RenderSystem::markProfileEvent( const String &eventName )
    {
        if( eventName.empty() )
            return;

        if(getCapabilities()->hasCapability(RSC_DEBUG))
           glInsertEventMarkerEXT(0, eventName.c_str());
    }
    //---------------------------------------------------------------------
#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID || OGRE_PLATFORM == OGRE_PLATFORM_EMSCRIPTEN
    void GLES2RenderSystem::notifyOnContextLost() {
        GLES2RenderSystem::mResourceManager->notifyOnContextLost();
    }

    void GLES2RenderSystem::resetRenderer(RenderWindow* win)
    {
        LogManager::getSingleton().logMessage("********************************************");
        LogManager::getSingleton().logMessage("*** OpenGL ES 2.x Reset Renderer Started ***");
        LogManager::getSingleton().logMessage("********************************************");
                
        initialiseContext(win);
        
        static_cast<GLES2FBOManager*>(mRTTManager)->_reload();
        
        _destroyDepthBuffer(win);
        
        GLES2DepthBuffer *depthBuffer = OGRE_NEW GLES2DepthBuffer( DepthBuffer::POOL_DEFAULT, this,
                                                                  mMainContext, 0, 0,
                                                                  win->getWidth(), win->getHeight(),
                                                                  win->getFSAA(), 0, true );
        
        mDepthBufferPool[depthBuffer->getPoolId()].push_back( depthBuffer );
        win->attachDepthBuffer( depthBuffer );
        
        GLES2RenderSystem::mResourceManager->notifyOnContextReset();
        
        mStateCacheManager->clearCache();
        _setViewport(NULL);
        _setRenderTarget(win);
    }
    
    GLES2ManagedResourceManager* GLES2RenderSystem::getResourceManager()
    {
        return GLES2RenderSystem::mResourceManager;
    }
#endif

    void GLES2RenderSystem::_setTextureUnitCompareFunction(size_t unit, CompareFunction function)
    {
        //no effect in GLES2 rendersystem
    }

    void GLES2RenderSystem::_setTextureUnitCompareEnabled(size_t unit, bool compare)
    {
        //no effect in GLES2 rendersystem
    }

    unsigned int GLES2RenderSystem::getDiscardBuffers(void)
    {
        return mStateCacheManager->getDiscardBuffers();
    }

    void GLES2RenderSystem::bindVertexElementToGpu( const VertexElement &elem,
                                                     HardwareVertexBufferSharedPtr vertexBuffer, const size_t vertexStart,
                                                     vector<GLuint>::type &attribsBound,
                                                     vector<GLuint>::type &instanceAttribsBound,
                                                     bool updateVAO)
    {
        void* pBufferData = 0;
        const GLES2HardwareVertexBuffer* hwGlBuffer = static_cast<const GLES2HardwareVertexBuffer*>(vertexBuffer.get());

        if (updateVAO)
        {
            mStateCacheManager->bindGLBuffer(GL_ARRAY_BUFFER,
                                             hwGlBuffer->getGLBufferId());
            pBufferData = VBO_BUFFER_OFFSET(elem.getOffset());

            if (vertexStart)
            {
                pBufferData = static_cast<char*>(pBufferData) + vertexStart * vertexBuffer->getVertexSize();
            }

            VertexElementSemantic sem = elem.getSemantic();
            unsigned short typeCount = VertexElement::getTypeCount(elem.getType());
            GLboolean normalised = GL_FALSE;
            GLuint attrib = 0;
            unsigned short elemIndex = elem.getIndex();

            if(getCapabilities()->hasCapability(RSC_SEPARATE_SHADER_OBJECTS))
            {
                GLSLESProgramPipeline* programPipeline =
                GLSLESProgramPipelineManager::getSingleton().getActiveProgramPipeline();
                if (!programPipeline || !programPipeline->isAttributeValid(sem, elemIndex))
                {
                    return;
                }

                attrib = (GLuint)programPipeline->getAttributeIndex(sem, elemIndex);
            }
            else
            {
                GLSLESLinkProgram* linkProgram = GLSLESLinkProgramManager::getSingleton().getActiveLinkProgram();
                if (!linkProgram || !linkProgram->isAttributeValid(sem, elemIndex))
                {
                    return;
                }

                attrib = (GLuint)linkProgram->getAttributeIndex(sem, elemIndex);
            }

            if(getCapabilities()->hasCapability(RSC_VERTEX_BUFFER_INSTANCE_DATA))
            {
                if (mCurrentVertexProgram)
                {
                    if (hwGlBuffer->getIsInstanceData())
                    {
                        OGRE_CHECK_GL_ERROR(glVertexAttribDivisorEXT(attrib, static_cast<GLuint>(hwGlBuffer->getInstanceDataStepRate())));
                        instanceAttribsBound.push_back(attrib);
                    }
                }
            }

            switch(elem.getType())
            {
                case VET_COLOUR:
                case VET_COLOUR_ABGR:
                case VET_COLOUR_ARGB:
                    // Because GL takes these as a sequence of single unsigned bytes, count needs to be 4
                    // VertexElement::getTypeCount treats them as 1 (RGBA)
                    // Also need to normalise the fixed-point data
                    typeCount = 4;
                    normalised = GL_TRUE;
                    break;
                default:
                    break;
            };

            OGRE_CHECK_GL_ERROR(glVertexAttribPointer(attrib,
                                                      typeCount,
                                                      GLES2HardwareBufferManager::getGLType(elem.getType()),
                                                      normalised,
                                                      static_cast<GLsizei>(vertexBuffer->getVertexSize()),
                                                      pBufferData));

            mStateCacheManager->setVertexAttribEnabled(attrib);
//                OGRE_CHECK_GL_ERROR(glEnableVertexAttribArray(attrib));
            attribsBound.push_back(attrib);
        }
    }

    void GLES2RenderSystem::_copyContentsToMemory(Viewport* vp, const Box& src, const PixelBox& dst,
                                                  RenderWindow::FrameBuffer buffer) {
        GLenum format = GLES2PixelUtil::getGLOriginFormat(dst.format);
        GLenum type = GLES2PixelUtil::getGLOriginDataType(dst.format);

        if ((format == 0) || (type == 0))
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "Unsupported format.",
                "GLES2RenderSystem::_copyContentsToMemory" );
        }

        bool hasPackImage = mHasGLES30 || mGLSupport->checkExtension("GL_NV_pack_subimage");
        OgreAssert(dst.getWidth() == dst.rowPitch || hasPackImage, "GL_PACK_ROW_LENGTH not supported");

        // Switch context if different from current one
        _setViewport(vp);

        OGRE_CHECK_GL_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0));

        if(dst.getWidth() != dst.rowPitch && hasPackImage)
            glPixelStorei(GL_PACK_ROW_LENGTH_NV, dst.rowPitch);

        // Must change the packing to ensure no overruns!
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        if(mHasGLES30) {
            glReadBuffer((buffer == RenderWindow::FB_FRONT) ? GL_FRONT : GL_BACK);
        }

        uint32_t height = vp->getTarget()->getHeight();

        glReadPixels((GLint)src.left, (GLint)(height - src.bottom),
                     (GLsizei)dst.getWidth(), (GLsizei)dst.getHeight(),
                     format, type, dst.getTopLeftFrontPixelPtr());

        // restore default alignment
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ROW_LENGTH_NV, 0);

        PixelUtil::bulkPixelVerticalFlip(dst);
    }

    }
