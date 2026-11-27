#ifndef RM_PROFILE_H
#define RM_PROFILE_H

#define RM_SIM 1
#define RM_BENCH 2
#define RM_RUN 3

#ifndef RM_PROFILE
#error Select RM_PROFILE=1 (SIM), 2 (BENCH), or 3 (RUN)
#endif
#if RM_PROFILE < RM_SIM || RM_PROFILE > RM_RUN
#error Unknown build profile
#endif

#define RM_CONTROL_PERIOD_MS 5u
#define RM_MOTION_TIMEOUT_MS 200u
#define RM_FRAME_TIMEOUT_MS 100u
#define RM_DEBUG_BAUD 115200u
/* Real actuation requires a reviewed board implementation. */
#define RM_REAL_OUTPUTS 0
#endif
