module;

#include <GLFW/glfw3.h>

export module kiln.wsi.event.KeyAction;

namespace kiln::wsi {

export enum struct KeyAction
{
    eRelease = GLFW_RELEASE,
    ePress   = GLFW_PRESS,
    eRepeat  = GLFW_REPEAT,
};

}   // namespace kiln::wsi
