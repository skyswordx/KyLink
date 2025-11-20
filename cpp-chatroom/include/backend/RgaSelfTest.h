#pragma once

#include <QtCore/QString>

namespace diagnostics {

// Runs a lightweight RGA self test, returning true when the copy operation
// completes successfully or when the platform intentionally skips the test.
bool runRgaSelfTest(QString* errorOut = nullptr);

} // namespace diagnostics
