#ifndef NESSO_SHELLY_DECODER_H
#define NESSO_SHELLY_DECODER_H

#include "types.h"

#define ZCL_CLUSTER_SCENES 0x0005
#define ZCL_CLUSTER_ONOFF 0x0006
#define ZCL_CLUSTER_LEVEL 0x0008

#define ZCL_CMD_OFF 0x00
#define ZCL_CMD_ON 0x01
#define ZCL_CMD_TOGGLE 0x02
#define ZCL_CMD_RECALL_SCENE 0x05
#define ZCL_CMD_STEP 0x02

#define ZCL_STEP_UP 0
#define ZCL_STEP_DOWN 1

/*
 * Shelly BLU RC Button 4 ZB (SBBT-104CUS).
 * Default On/Off mode: EP1 ON/OFF → knop 1/2, EP2 ON/OFF → knop 3/4.
 * Toggle mode: TOGGLE op EP k → knop k. Scenes: RecallScene op EP k.
 */
bool shelly_decode(const ZclIncoming *cmd, ButtonEvent *out);

#endif
