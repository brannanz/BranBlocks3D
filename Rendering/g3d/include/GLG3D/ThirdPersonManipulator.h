/**
  @file GLG3D/ThirdPersonManipulator.h

  @maintainer Morgan McGuire, matrix@graphics3d.com

  @created 2006-06-09
  @edited  2006-06-09
*/

#ifndef G3D_THIRDPERSONMANIPULATOR_H
#define G3D_THIRDPERSONMANIPULATOR_H

#include "G3D/Array.h"
#include "G3D/Vector3.h"
#include "G3D/LineSegment.h"
#include "G3D/ConvexPolyhedron.h"
#include "G3D/ReferenceCount.h"
#include "GLG3D/ManualCameraController.h"

namespace G3D {

class RenderDevice;

namespace _internal {

static int dummyInt;
template<class SegmentType, class VertexType>
class PolyLineGeneric {
private:

    Array<SegmentType>    m_segment;

public:

    PolyLineGeneric() {}

    /** Set first == last to close */
    PolyLineGeneric(const Array<VertexType>& v, bool reverse = false) {
        m_segment.resize(v.size() - 1);

        if (reverse) {
            int N = v.size() - 1;
            for (int i = 0; i < m_segment.size(); ++i) {
                m_segment[i] = SegmentType::fromTwoPoints(v[N - i], v[N - i - 1]);
            }
        } else {
            for (int i = 0; i < m_segment.size(); ++i) {
                m_segment[i] = SegmentType::fromTwoPoints(v[i], v[i + 1]);
            }
        }
    }

    /** Returns 1 + num segments; If the polyline is closed, the first and last vertex will
        be identical.*/
    int numVertices() const {
        return m_segment.size() + 1;
    }

    bool closed() const {
        if (m_segment.size() == 0) {
            return false;
        } else {
            return m_segment[0].endPoint(0) == m_segment.last().endPoint(1);
        }
    }

    VertexType vertex(int i) const {
        if ((i < m_segment.size()) && (i >= 0)) {
            return m_segment[i].endPoint(0);
        } else if (i == m_segment.size()) {
            return m_segment.last().endPoint(1);
        } else {
            debugAssertM(false, "Index out of bounds");
            return VertexType();
        }
    }

    int numSegments() const {
        return m_segment.size();
    }

    const SegmentType& segment(int s) const {
        return m_segment[s];
    }

    /**
     @param segmentIndex returns the index of the closest segment.
     */
    float distance(const VertexType& p, int& segmentIndex = dummyInt) const {
        float d = inf();
        segmentIndex = 0;
        for (int i = 0; i < m_segment.size(); ++i) {
            float t = m_segment[i].distance(p);
            if (t < d) {
                segmentIndex = i;
                d = t;
            }
        }

        return d;
    }
};

typedef PolyLineGeneric<LineSegment, Vector3> PolyLine;
typedef PolyLineGeneric<LineSegment2D, Vector2> PolyLine2D;

/**
 Piece of 3D geometry that tracks its own 2D projection.
 For use in building 3D clickable widgets using only 2D
 code.

 The 2D approach allows the click width around a line to be
 constant width; it also allows the use of arbitrary projection
 matrices.  However, it has problems at the near plane unlike
 a ray-cast 3D approach.
 */
class UIGeom {
public:
    /** Relative to the "current" object to world matrix*/
    Array<PolyLine>         line3D;

    /** Relative to the "current" object to world matrix*/
    Array<ConvexPolygon>    poly3D;

    bool                    visible;

    /** Recomputed from 3D in the computeProjection method. */
    Array<PolyLine2D>       line2D;
    Array< Array<float> >   lineDepth;
    Array< Array<float> >   lineW;

    /** Recomputed from 3D in the computeProjection method. */
    Array<ConvexPolygon2D>  poly2D;
    Array< float >          polyDepth;

    /** If last time we rendered the poly3D was backwards. */
    Array<bool>             polyBackfacing;

protected:

    /** If true, backface polygons are tested for mouse clicks.*/
    bool                    m_twoSidedPolys;

    /** Returns the normal to a line segment. */
    static Vector3 segmentNormal(const LineSegment& seg, const Vector3& eye);

    /** Returns the object space eye point */
    static Vector3 computeEye(RenderDevice* rd);

public:

    UIGeom() :  m_twoSidedPolys(true), visible(true) {}

    /** 
      Returns true and sets nearestDepth (on the range 0 = close, 1 = far)
      if p is in a polygon (or within lineRadius
      of the nearest line) <b>and</b> the depth of that object
      is less than nearestDepth, otherwise returns false.

      depth values are approximate.

      @param tangent2D Returns the projected tangent (unit length 3D vector projected into 2D,
       which then has non-unit length) to the selected location 
       if it was a line; undefined if it was a plane.

      @param projectionW Returns the if close to a line.
     */
    bool contains(
        const Vector2&  p, 
        float&          nearestDepth, 
        Vector2&        tangent2D,
        float&          projectionW,
        float           lineRadius = 9) const;

    /** Computes the 2D positions from the 3D ones using the renderDevice
        and updates the zOrder.*/
    void computeProjection(RenderDevice* rd);

    /** 
      Render in 3D using the current blending mode, etc.
      Line normals are set to face towards the camera, perpendicular to the line.
     */
    void render(RenderDevice* rd, float lineScale = 1.0f);
};
} // _internal

typedef ReferenceCountedPointer<class ThirdPersonManipulator> ThirdPersonManipulatorRef;

/**
  By default, the ThirdPersonManipulator moves an object relative to its own axes.
  To move the object relative to the world axes, use setOffsetFrame.  getFrame()
  will still return the frame for the object, but the 


  Examples:
  <pre>
    CoordinateFrame obj = CoordinateFrame(Matrix3::fromAxisAngle(Vector3::unitY(), toRadians(30)), Vector3(2, 0, 0));

    // To move object relative to its own axes
    manipulator.setFrame(obj);
    manipulator.setControlFrame(obj);

    // Move object relative to the world axes
    manipulator.setFrame(obj);
    manipulator.setControlFrame(CoordinateFrame());

    // Move object relative to the world axes using its own translation
    manipulator.setFrame(obj);
    manipulator.setControlFrame(CoordinateFrame(obj.translation));
  </pre>

 TODO:
  track-ball rotation circle
     draw
     drag
  
  center translate
     draw 
     drag

  shift key 
    for integer-size increments
    45-degrees
    close to axes

  clip to near plane
  enable/disable

  double-axis 
     drag  
 */
class ThirdPersonManipulator : public Manipulator {
private:

    friend class TPMPosedModel;

    PosedModelRef           m_posedModel;

    /** The frame of the control for movement purposes.  */
    CoordinateFrame         m_offsetFrame;

    /** Current position. */
    CoordinateFrame         m_controlFrame;

    /** Single translation axes, double translation axes, rotation axes.*/
    enum Geom {NO_AXIS = -1, X, Y, Z, XY, YZ, ZX, RX, RY, RZ, NUM_GEOMS};//CENTER};
    _internal::UIGeom       m_geomArray[NUM_GEOMS];

    float                   m_axisScale;

    /** True when the mouse has been pressed and we're dragging the control. */
    bool                    m_dragging;

    /** Key code that beings a drag (typically left mouse). 
        Could be a setting.*/
    int                     m_dragKey;

    /** Enables dragging on multiple axes simultaneously, which is broken in this build. */
    bool                    m_doubleAxisDrag;

    /** Index of the axis that is currently being dragged. */
    int                     m_dragAxis;

    /** Index of the axis that the mouse is currently over.  NO_AXIS for none. */
    int                     m_overAxis;

    /** When using a rotation drag, this is the tangent to the current circle. */
    Vector2                 m_dragTangent;

    float                   m_dragW;

    /** Distance from the axis at which it is still clickable, in pixels.
        Could be a setting.*/
    float                   m_maxAxisDistance2D;

    bool                    m_rotationEnabled;

    bool                    m_translationEnabled;

    bool                    m_enabled;

    /** 
      true for each axis that is currently being used for the drag.
     */
    bool                    m_usingAxis[NUM_GEOMS];

    enum {FIRST_TRANSLATION = X, 
          LAST_TRANSLATION = ZX};

    enum {FIRST_ROTATION = RX, 
          LAST_ROTATION = RZ};

public:

    void render(RenderDevice* rd);


    /** Called when it has been determined that the user first began a drag on
        one of our controls.  Invoked before m_dragging is true. */
    virtual void onDragBegin(const Vector2& start);

    /** Invoked after m_dragging is false. */
    virtual void onDragEnd(const Vector2& stop);

    /** Called from onDrag 
      @param a Axis index.
      */
    Vector3 singleAxisTranslationDrag(int a, const Vector2& delta);

    Vector3 doubleAxisTranslationDrag(int a0, int a1, const Vector2& delta);

    /** Called when the user has dragged on the control. */
    virtual void onDrag(const Vector2& delta);

    /** Assumes that m_controlFrame is the current object to world matrix */
    void computeProjection(RenderDevice* rd);

public:
    // For design reference, see the 3DS Max gizmos at
    // http://www.3dmax-tutorials.com/Transform_Gizmo.html

    ThirdPersonManipulator();

    void setRotationEnabled(bool r);

    bool rotationEnabled() const;

    void setTranslationEnabled(bool r);

    bool translationEnabled() const;

    bool enabled() const;

    void setEnabled(bool e);

    /** Given the desired start frame for the axes and the 
        desired frame for the object, returns the offsetFrame
        that should be used t*/
    static CoordinateFrame computeOffsetFrame(
        const CoordinateFrame& controlFrame, 
        const CoordinateFrame& objectFrame);

    virtual CoordinateFrame frame() const;

    virtual void getPosedModel(
        Array<PosedModelRef>& posedArray, 
        Array<PosedModel2DRef>& posed2DArray);

    /** Keeps the object where it in world space is and moves the control in world space.
        Changes the value of frame() constant. */
    void setControlFrame(const CoordinateFrame& c);

    void getControlFrame(CoordinateFrame& c) const;

     /** 
       Moves the object and the control in world space.
       */
    void setFrame(const CoordinateFrame& c);

    /** Returns the object's new frame */
    void getFrame(CoordinateFrame& c) const;

    void onSimulation(RealTime rdt, SimTime sdt, SimTime idt);

    bool onEvent(const GEvent& event);

    void onUserInput(UserInput* ui);

    virtual void onNetwork();

    virtual void onLogic();
};

}
#endif
