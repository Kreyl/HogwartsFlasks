/*
 * EvtMsgIDs.h
 *
 *  Created on: 21 ���. 2017 �.
 *      Author: Kreyl
 */

#pragma once

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdUartCmdRcvd,
    evtIdEverySecond,

    evtIdUsbConnect,
    evtIdUsbDisconnect,
    evtIdUsbReady,

    evtIdButtons,

    evtIdCorrectCode,
    evtIdBadCode,

    evtIdAudioPlayStop,
    evtIdVideoPlayStop,

    evtIdStandbyAudio,
    evtIdStandbyVideo,
};
