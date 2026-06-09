#pragma once

#include "App/Signals.h"

class CaptureController;

namespace CaptureActions {
    class Handler : public Signals::Trackable {
    public:
        Handler(CaptureController& captureController);
    };
}
