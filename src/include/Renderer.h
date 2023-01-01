#ifndef RENDERUTIL
#define RENDERUTIL
#include "Enum.h"
#include "V2DataModel/Instance.h"
void renderShape(const Enum::Shape::Value& shape, const Vector3& size, const Color3& ncolor);
void renderSurface(const char face, const Enum::SurfaceType::Value& surface, const Vector3& size, const Enum::Controller::Value& controller, const Color3& color);
#endif
