#pragma once

#include "gazeora/input/interfaces/pointer_device.h"

namespace gazeora::input {

// Adapter pattern: the Windows mouse pointer, driven with SendInput as if a mouse moved it.
// Windows won't let it reach a game running as administrator unless Gazeora does too.
class SendInputPointer final : public IPointerDevice {
public:
    Vec2 Position() const override;
    float ScreenAspect() const override;
    void MoveTo(Vec2 position) override;
    void Click() override;
    void Scroll(int vertical, int horizontal) override;
};

}  // namespace gazeora::input
