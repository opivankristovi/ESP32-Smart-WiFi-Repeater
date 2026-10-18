## 2024-10-15 - Pre-allocating string capacity \n **Learning:** Avoid heap fragmentation in embedded projects by using `.reserve()` before large string operations. \n **Action:** Check for missing `.reserve()` in other places where `String` builds large payloads.
## 2026-10-15 - Fix Missing Input Validation for MQTT Port
**Learning:** The MQTT port must be properly bounded and clamped between 1 and 65535 instead of just checking for zero value when parsing web input in C++.
**Action:** Always sanitize integer-based user inputs properly to prevent logic errors and unexpected behaviors.
