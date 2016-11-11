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

#include "Sample.h"

static Sample* sampleInst = NULL;

int main( int argc, const char* argv[] ) {	
	try
	{
		sampleInst = new Sample();
		sampleInst->initApp();
	    emscripten_set_main_loop_arg(Sample::_mainLoop, sampleInst, 0, 1);
		sampleInst->closeApp();
        delete sampleInst;
        sampleInst = NULL;
	}
	catch (std::exception& e)
	{
		emscripten_run_script((std::string("alert('") + e.what() + "')").c_str());
	}
	return 0;
}

extern "C"
{
    int passAssetAsArrayBuffer(unsigned char* buf, int length) {
        sampleInst->passAssetAsArrayBuffer(buf, length);
        return 0;
    }
    
    int clearScene() {
        sampleInst->clearScene();
        return 0;
    }
}
