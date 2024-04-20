/*
 * EvtMsgIDs.h
 *
 *  Created on: 21 ���. 2017 �.
 *      Author: Kreyl
 */

#ifndef EVTMSGIDS_H__
#define EVTMSGIDS_H__

enum EvtMsgId_t {
    evtIdNone = 0, // Always

    // Pretending to eternity
    evtIdUartCmdRcvd,
    evtIdEverySecond,

    evtIdPointsSet,
    evtIdPointsAdd,
    evtIdPointsRemove,
    evtIdPointsHide,
    evtIdPointsShow,

    evtIdBckgStop,

    evtIdUsbConnect,
    evtIdUsbDisconnect,
    evtIdUsbReady,
};

#endif //EVTMSGIDS_H__
