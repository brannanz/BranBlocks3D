#pragma once

// Instances
#include "Workspace.h"
#include "Level.h"
#include "Part.h"
#include "SelectionService.h"
#include "GuiRootInstance.h"
#include "ThumbnailGenerator.h"
#include "Util/XplicitNgine.h"
#include "Util/SoundService.h"
#include "Lighting.h"

// Libraries 
#include "rapidxml/rapidxml.hpp"

class GuiRootInstance;

class DataModelManager :
	public Instance
{
public:
	DataModelManager(void);
	~DataModelManager(void);
	void					setMessage(std::string);
	void					setMessageBrickCount();
	void					clearMessage();
	bool					debugGetOpen();
	bool					getOpen();
	bool					load(const char* filename,bool clearObjects);	
	bool					readXMLFileStream(std::ifstream* file);
	void					drawMessage(RenderDevice*);
	
	// Instance getters
	WorkspaceInstance*				getWorkspace();
	LevelInstance*					getLevel();
	XplicitNgine*					getEngine();
	ThumbnailGeneratorInstance*		getThumbnailGenerator();
	SoundService*					getSoundService();
	LightingInstance*				getLighting();

	std::string				message;
	std::string				_loadedFileName;
	bool					showMessage;
	G3D::GFontRef			font;
	GuiRootInstance*		getGuiRoot();
	SelectionService*		getSelectionService();
	PartInstance*			makePart();
	void					clearLevel();
	void					toggleRun();
	bool					isRunning();
	void					resetEngine();
	bool					scanXMLObject(rapidxml::xml_node<>* node);
#if _DEBUG
	void					modXMLLevel(float modY);
#endif
private:
	bool isBrickCount;
	rapidxml::xml_node<>*	getNode(rapidxml::xml_node<> * node,const char* name );
	float					getFloatValue(rapidxml::xml_node<> * node,const char* name);
	bool					_successfulLoad;
	std::string				_errMsg;
	bool					_legacyLoad;
	float					_modY;
	
	// Instances
	WorkspaceInstance*		workspace;
	LevelInstance*			level;
	GuiRootInstance*		guiRoot;
	SelectionService*		selectionService;
	ThumbnailGeneratorInstance* thumbnailGenerator;
	XplicitNgine*			xplicitNgine;
	SoundService*			soundService;
	LightingInstance*		lightingInstance;
	bool					running;
	
};
