/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2017 Torus Knot Software Ltd

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

#ifndef _OgreTextureGpuManager_H_
#define _OgreTextureGpuManager_H_

#include "OgrePrerequisites.h"
#include "OgreTextureGpu.h"
#include "OgreImage2.h"
#include "Threading/OgreLightweightMutex.h"
#include "Threading/OgreWaitableEvent.h"
#include "Threading/OgreThreads.h"

#include "OgreHeaderPrefix.h"

namespace Ogre
{
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Resources
    *  @{
    */

    typedef vector<TextureGpu*>::type TextureGpuVec;
    class ObjCmdBuffer;

    struct _OgreExport TexturePool
    {
        TextureGpu  *masterTexture;
        uint16                  usedMemory;
        vector<uint16>::type    availableSlots;
        TextureGpuVec           usedSlots;

        bool hasFreeSlot(void) const;
    };

    typedef list<TexturePool>::type TexturePoolList;

    typedef vector<StagingTexture*>::type StagingTextureVec;

    class _OgreExport TextureGpuManager : public ResourceAlloc
    {
    public:
        /// Specifies the minimum squared resolution & number of slices to keep around
        /// all the for time StagingTextures. So:
        /// PixelFormatGpu format = PixelFormatGpuUtils::getFamily(PFG_RGBA8_UNORM);
        /// BudgetEntry( format, 4096u, 2u );
        /// Will keep around 128MB of staging texture, with a resolution of 4096x4096 x 2 slices.
        struct BudgetEntry
        {
            PixelFormatGpu formatFamily;
            uint32 minResolution;
            uint32 minNumSlices;
            BudgetEntry() : formatFamily( PFG_UNKNOWN ), minResolution( 0 ), minNumSlices( 0 ) {}
            BudgetEntry( PixelFormatGpu _formatFamily, uint32 _minResolution, uint32 _minNumSlices ) :
                formatFamily( _formatFamily ), minResolution( _minResolution ),
                minNumSlices( _minNumSlices ) {}

            bool operator () ( const BudgetEntry &_l, const BudgetEntry &_r ) const;
        };

        typedef vector<BudgetEntry>::type BudgetEntryVec;

        struct PoolParameters
        {
            /// Pool shall grow until maxBytesPerPool is reached.
            /// Once that's reached, a new pool will be created.
            /// Otherwise GPU RAM fragmentation may cause out of memory even
            /// though it could otherwise fulfill our request.
            /// Includes mipmaps.
            /// This value may actually be exceeded if a single texture surpasses this limit,
            /// or if minSlicesPerPool is > 1 (it takes higher priority)
            size_t maxBytesPerPool;
            /// Minimum slices per pool, regardless of maxBytesPerPool.
            /// It's also the starting num of slices. See maxResolutionToApplyMinSlices
            uint16 minSlicesPerPool[4];
            /// If texture resolution is <= maxResolutionToApplyMinSlices[i];
            /// we'll apply minSlicesPerPool[i]. Otherwise, we'll apply minSlicesPerPool[i+1]
            /// If resolution > maxResolutionToApplyMinSlices[N]; then minSlicesPerPool = 1;
            uint32 maxResolutionToApplyMinSlices[4];

            /// See BudgetEntry. Must be sorted by size in bytes (biggest entries first).
            BudgetEntryVec budget;
        };

    protected:
        struct ResourceEntry
        {
            String      name;
            String      resourceGroup;
            TextureGpu  *texture;

            ResourceEntry() : texture( 0 ) {}
            ResourceEntry( const String &_name, const String &_resourceGroup, TextureGpu *_texture ) :
                name( _name ), resourceGroup( _resourceGroup ), texture( _texture ) {}
        };
        typedef map<IdString, ResourceEntry>::type ResourceEntryMap;

        struct LoadRequest
        {
            String      name;
            Archive     *archive;
            TextureGpu  *texture;
            GpuResidency::GpuResidency nextResidency;

            LoadRequest( const String &_name, Archive *_archive,
                         TextureGpu *_texture, GpuResidency::GpuResidency _nextResidency ) :
                name( _name ), archive( _archive ),
                texture( _texture ), nextResidency( _nextResidency ) {}
        };

        typedef vector<LoadRequest>::type LoadRequestVec;

        struct UsageStats
        {
            uint32 width;
            uint32 height;
            PixelFormatGpu formatFamily;
            size_t accumSizeBytes;
            size_t prevSizeBytes;
            uint32 frameCount;
            UsageStats( uint32 _width, uint32 _height, uint32 _depthOrSlices,
                        PixelFormatGpu _formatFamily );
        };
        typedef vector<UsageStats>::type UsageStatsVec;

        struct QueuedImage
        {
            Image2      image;
            uint64      mipLevelBitSet[4];
            TextureGpu  *dstTexture;

            QueuedImage( Image2 &srcImage, uint8 numMips, TextureGpu *_dstTexture );
            void destroy(void);
            bool empty(void) const;
            bool isMipQueued( uint8 mipLevel ) const;
            void unqueueMip( uint8 mipLevel );
            uint8 getMinMipLevel(void) const;
            uint8 getMaxMipLevelPlusOne(void) const;
        };

        typedef vector<QueuedImage>::type QueuedImageVec;

        struct ThreadData
        {
            LoadRequestVec  loadRequests;
            ObjCmdBuffer    *objCmdBuffer;
            StagingTextureVec usedStagingTex;
        };
        struct StreamingData
        {
            StagingTextureVec   availableStagingTex;
            QueuedImageVec      queuedImages;  /// Used by worker thread.
            UsageStatsVec       usageStats;
        };

        bool                mShuttingDown;
        ThreadHandlePtr     mWorkerThread;
        /// Main thread wakes, worker waits.
        WaitableEvent       mWorkerWaitableEvent;
        /// Worker wakes, main thread waits. Used by waitForStreamingCompletion();
        WaitableEvent       mRequestToMainThreadEvent;
        LightweightMutex    mLoadRequestsMutex;
        LightweightMutex    mMutex;
        ThreadData          mThreadData[2];
        StreamingData       mStreamingData;

        TexturePoolList     mTexturePool;
        ResourceEntryMap    mEntries;

        size_t              mEntriesToProcessPerIteration;
        PoolParameters      mDefaultPoolParameters;

        StagingTextureVec   mUsedStagingTextures;
        StagingTextureVec   mAvailableStagingTextures;

        StagingTextureVec   mTmpAvailableStagingTex;

        typedef vector<AsyncTextureTicket*>::type AsyncTextureTicketVec;
        AsyncTextureTicketVec   mAsyncTextureTickets;

        VaoManager          *mVaoManager;

        void destroyAll(void);
        void destroyAllStagingBuffers(void);
        void destroyAllTextures(void);
        void destroyAllPools(void);

        virtual TextureGpu* createTextureImpl( GpuPageOutStrategy::GpuPageOutStrategy pageOutStrategy,
                                               IdString name, uint32 textureFlags,
                                               TextureTypes::TextureTypes initialType ) = 0;
        virtual StagingTexture* createStagingTextureImpl( uint32 width, uint32 height, uint32 depth,
                                                          uint32 slices, PixelFormatGpu pixelFormat )=0;
        virtual void destroyStagingTextureImpl( StagingTexture *stagingTexture ) = 0;

        virtual AsyncTextureTicket* createAsyncTextureTicketImpl( uint32 width, uint32 height,
                                                                  uint32 depthOrSlices,
                                                                  TextureTypes::TextureTypes textureType,
                                                                  PixelFormatGpu pixelFormatFamily ) = 0;

        uint16 getNumSlicesFor( TextureGpu *texture ) const;

        /// Fills mTmpAvailableStagingTex with new StagingTextures that support formats &
        /// resolutions the worker thread couldn't upload because it lacked a compatible one.
        /// Assumes we're protected by mMutex! Called from main thread.
        void fullfillUsageStats( ThreadData &workerData );
        /// Fills mTmpAvailableStagingTex with new StagingTextures if there's not enough
        /// in there to meet our minimum budget in poolParams.
        void fullfillMinimumBudget( ThreadData &workerData, const PoolParameters &poolParams );

        void fullfillBudget( ThreadData &workerData );

        /** Finds a StagingTexture that can map the given region defined by the box & pixelFormat.
            Searches in both used & available textures.
            If no staging texture supports this request, it will fill a RareRequest entry.
        @remarks
            Assumes workerData is protected by a mutex.
        @param workerData
            Worker thread data.
        @param box
        @param pixelFormat
        @param outStagingTexture
            StagingTexture that mapped the return value. Unmodified if we couldn't map.
        @return
            The mapped region. If TextureBox::data is null, it couldn't be mapped.
        */
        static TextureBox getStreaming( ThreadData &workerData, StreamingData &streamingData,
                                        const TextureBox &box, PixelFormatGpu pixelFormat,
                                        StagingTexture **outStagingTexture );
        static void processQueuedImage( QueuedImage &queuedImage, ThreadData &workerData,
                                        StreamingData &streamingData );

    public:
        TextureGpuManager( VaoManager *vaoManager );
        virtual ~TextureGpuManager();

        /// Must be called from main thread.
        void _reserveSlotForTexture( TextureGpu *texture );
        /// Must be called from main thread.
        void _releaseSlotFromTexture( TextureGpu *texture );

        unsigned long _updateStreamingWorkerThread( ThreadHandle *threadHandle );
        void _updateStreaming(void);

        /// Returns true if there is no more streaming work to be done yet
        /// (if false, calls to _update could be needed once again)
        /// See waitForStreamingCompletion.
        bool _update(void);

        /// Blocks main thread until are pending textures are fully loaded.
        void waitForStreamingCompletion(void);

        /// Do not use directly. See TextureGpu::waitForMetadata & TextureGpu::waitForDataReady
        void _waitFor( TextureGpu *texture, bool metadataOnly );

        /**
        @param name
        @param pageOutStrategy
        @param textureFlags
            See TextureFlags::TextureFlags
        @param initialType
            Strictly not required (i.e. can be left TextureTypes::Unknown) however it
            can be needed if set to a material before it is fully loaded; and the
            shader expects a particular type (e.g. it expects a cubemap).
            While it's not yet loaded, a dummy texture will that matches the type will
            be used; and it's important that the right dummy texture is selected.
            So if you know in advance a particular type is needed, this parameter
            tells Ogre what dummy to use.
        @param resourceGroup
            Optional, but required if you want to load files from disk
            (or anything provided by the ResourceGroupManager)
        @return
        */
        TextureGpu* createTexture( const String &name,
                                   GpuPageOutStrategy::GpuPageOutStrategy pageOutStrategy,
                                   uint32 textureFlags, TextureTypes::TextureTypes initialType,
                                   const String &resourceGroup=BLANKSTRING );
        /// If the texture doesn't exists, behaves exactly as createTexture. If the texture with
        /// that name already exists, it returns it and the rest of the arguments will be ignored.
        TextureGpu* createOrRetrieveTexture( const String &name,
                                             GpuPageOutStrategy::GpuPageOutStrategy pageOutStrategy,
                                             uint32 textureFlags,
                                             TextureTypes::TextureTypes initialType,
                                             const String &resourceGroup=BLANKSTRING );
        void destroyTexture( TextureGpu *texture );

        /** Creates a StagingTexture which is required to upload data CPU -> GPU into
            a TextureGpu.
            To download data GPU -> CPU see readRequest
        @remarks
            We try to find the smallest available texture (won't stall) that can fit the request.
        @param minConsumptionRatioThreshold
            Value in range [0; 100].
            The smallest available texture we find may still be too big (e.g. you need to upload
            64x64 texture RGBA8 and we return a 8192x8192x4 staging texture which is overkill).
            For these cases, here you can specify how much "is too big". For example by
            specifying a consumptionRatio of 50; it means that the data you asked for
            must occupy at least 50% of the space; otherwise we'll create a new StagingTexture.

            A value of 100 means the StagingTexture must fit exactly (fully used).
            A value of 0 means any StagingTexture will do, no matter how large.

            StagingTextures that haven't been using in a while will be destroyed. However
            if for some reason we end up returning a huge texture every frame for small
            workloads, we'll be keeping that waste potentially forever.
        @return
            StagingTexture that meets the criteria. When you're done, remove it by calling
            removeStagingTexture.
        */
        StagingTexture* getStagingTexture( uint32 width, uint32 height, uint32 depth,
                                           uint32 slices, PixelFormatGpu pixelFormat,
                                           size_t minConsumptionRatioThreshold=25u );
        void removeStagingTexture( StagingTexture *stagingTexture );

        /** Creates an AsyncTextureTicket that can be used to download data GPU -> CPU
            from a TextureGpu.
            To upload data CPU -> GPU see getStagingTexture
        @param width
        @param height
        @param depthOrSlices
        @param pixelFormatFamily
            If the value is not a family value, it will automatically be converted to one.
        @return
        */
        AsyncTextureTicket* createAsyncTextureTicket( uint32 width, uint32 height, uint32 depthOrSlices,
                                                      TextureTypes::TextureTypes textureType,
                                                      PixelFormatGpu pixelFormatFamily );
        void destroyAsyncTextureTicket( AsyncTextureTicket *ticket );
        void destroyAllAsyncTextureTicket(void);

        const String* findNameStr( IdString idName ) const;

        void _scheduleTransitionTo( TextureGpu *texture, GpuResidency::GpuResidency nextResidency );
    };

    /** @} */
    /** @} */
}

#include "OgreHeaderSuffix.h"

#endif