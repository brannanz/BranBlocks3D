#pragma once
#include "V2DataModel/Part.h"
#pragma once
#include "V2DataModel/Workspace.h"

struct MousePoint{
	Vector3 position;
	PartInstance * target;
	MousePoint(Vector3 pos, PartInstance * targ)
	{
		position = pos;
		target = targ;
	}
};

#pragma once
class Mouse
{
public:
	Mouse();
	~Mouse(void);
	int x, y;
	int oldx, oldy;
	PartInstance * getTarget();
	MousePoint getPositionAndPart(std::vector<Instance *> ignore = std::vector<Instance *>());
	Vector3 getPosition(std::vector<Instance *> ignore = std::vector<Instance *>());
	bool isMouseOnScreen();
	bool isMouseDown();
	void setMouseDown(bool mouseDown);
	G3D::Ray getRay();
	G3D::Ray getLastRay();
	G3D::Plane getPlane();
	G3D::Plane getInversePlane();
private:
	bool mouseDown;
};
