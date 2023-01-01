/**
  @file VertexProgram.h

  @maintainer Morgan McGuire, matrix@graphics3d.com

  @created 2003-04-10
  @edited  2004-04-25
*/

#ifndef GLG3D_VERTEXPROGRAM_H
#define GLG3D_VERTEXPROGRAM_H

#include "GLG3D/GPUProgram.h"

namespace G3D {

typedef ReferenceCountedPointer<class VertexProgram> VertexProgramRef;

/**
  Abstraction of OpenGL vertex programs.  This class can be used with raw OpenGL, 
  without RenderDevice or SDL.

  If you use VertexProgramRef instead of VertexProgram*, the texture memory will be
  garbage collected.

  The vertex program must be written in the vertex program <B>assembly languages</B>
  specified by either:
  
    http://oss.sgi.com/projects/ogl-sample/registry/ARB/vertex_program.txt

    http://oss.sgi.com/projects/ogl-sample/registry/NV/vertex_program2.txt

  You can also write your programs in the NVIDIA Cg language and compile them with
  cgc to assembly language.  You cannot load Cg programs directly with a VertexProgram.
 
  If an error is encountered inside a shader in debug mode, that error is printed to the
  debug window (under MSVC) and the programmer can fix the error and reload the
  shader without reloading the program.

  See G3D::GPUProgram for information on how constants and variable assignments
  generated by the Cg compiler are handled.

  @deprecated
  Use G3D::VertexShader
 */
class VertexProgram : public GPUProgram {
private:

    VertexProgram(const std::string& _name, const std::string& filename);

public:

    static VertexProgramRef fromFile(const std::string& filename);

    static VertexProgramRef fromCode(const std::string& name, const std::string& code);

};

}

#endif