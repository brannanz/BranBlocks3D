#pragma once
#include <ode/ode.h>
#include "V2DataModel/Instance.h"
#include "V2DataModel/Part.h"

class XplicitNgine : public Instance
{
public:
	XplicitNgine();
	~XplicitNgine();
	dWorldID physWorld;
	dSpaceID physSpace;
	dJointGroupID contactgroup;

	void step(float stepSize);
	void createBody(PartInstance* partInstance);
	void deleteBody(PartInstance* partInstance);
	void updateBody(PartInstance* partInstance);
	void resetBody(PartInstance* partInstance);
};