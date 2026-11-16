#pragma once

// Keeps the screen on while the option is enabled. Android uses the
// activity window flag. Linux asks the session not to idle.
class ScreenAwake {
public:
    static void setKeepAwake(bool keep);
};
