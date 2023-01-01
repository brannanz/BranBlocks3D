#pragma once
#include "Group.h"
#include "Part.h"

class WorkspaceInstance :
	public GroupInstance
{
public:
	WorkspaceInstance(void);
	~WorkspaceInstance(void);
	void clearChildren();
	void zoomToExtents();
	std::vector<PartInstance *> partObjects;
};
