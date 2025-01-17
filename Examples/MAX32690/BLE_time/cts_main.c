/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Data transmitter sample application.
 *
 *  Copyright (c) 2012-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

#include <string.h>
#include <time.h>
#include "wsf_types.h"
#include "util/bstream.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_buf.h"
#include "wsf_nvm.h"
#include "wsf_timer.h"
#include "hci_api.h"
#include "sec_api.h"
#include "dm_api.h"
#include "smp_api.h"
#include "att_api.h"
#include "app_api.h"
#include "app_main.h"
#include "app_db.h"
#include "app_ui.h"
#include "svc_ch.h"
#include "svc_core.h"
#include "svc_wp.h"
#include "util/calc128.h"
#include "gatt/gatt_api.h"
#include "cts_api.h"
#include "wut.h"
#include "trimsir_regs.h"
#include "pal_btn.h"
#include "pal_uart.h"
#include "tmr.h"
#include "svc_sds.h"
#include "svc_time.h"
#include "rtc.h"



/**************************************************************************************************
  Macros
**************************************************************************************************/
#if (BT_VER > 8)

/* PHY Test Modes */
#define DATS_PHY_1M 1
#define DATS_PHY_2M 2
#define DATS_PHY_CODED 3

#endif /* BT_VER */

#define TRIM_TIMER_EVT 0x99

#define TRIM_TIMER_PERIOD_MS 60000

/*! Button press handling constants */
#define BTN_SHORT_MS 200
#define BTN_MED_MS 500
#define BTN_LONG_MS 1000

#define BTN_1_TMR MXC_TMR2
#define BTN_2_TMR MXC_TMR3

/*! Enumeration of client characteristic configuration descriptors */
enum {
    CTS_GATT_SC_CCC_IDX, /*! GATT service, service changed characteristic */
    CTS_CCC_IDX, /*! Arm Ltd. proprietary service, data transfer characteristic */
    DATS_SEC_DAT_CCC_IDX,
    DATS_NUM_CCC_IDX
};

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for advertising */
static const appAdvCfg_t ctsAdvCfg = {
    { 0, 0, 0 }, /*! Advertising durations in ms */
    { 300, 1600, 0 } /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for slave */
static const appSlaveCfg_t ctsSlaveCfg = {
    1, /*! Maximum connections */
};

/*! Configurable security parameters to set
*   pairing and authentication requirements.
*
*   Authentication and bonding flags
*       -DM_AUTH_BOND_FLAG  : Bonding requested 
*       -DM_AUTH_SC_FLAG    : LE Secure Connections requested 
*       -DM_AUTH_KP_FLAG    : Keypress notifications requested 
*       -DM_AUTH_MITM_FLAG  : MITM (authenticated pairing) requested
                              pairing method is determined by IO capabilities below
*
*   Initiator key distribution flags
*       -DM_KEY_DIST_LTK   : Distribute LTK used for encryption
*       -DM_KEY_DIST_IRK   : Distribute IRK used for privacy 
*       -DM_KEY_DIST_CSRK  : Distribute CSRK used for signed data 
*/
static const appSecCfg_t ctsSecCfg = {
    DM_AUTH_BOND_FLAG | DM_AUTH_SC_FLAG | DM_AUTH_MITM_FLAG, /*! Authentication and bonding flags */
    DM_KEY_DIST_IRK, /*! Initiator key distribution flags */
    DM_KEY_DIST_LTK | DM_KEY_DIST_IRK, /*! Responder key distribution flags */
    FALSE, /*! TRUE if Out-of-band pairing data is present */
    FALSE /*! TRUE to initiate security upon connection */
};

/* OOB UART parameters */
#define OOB_BAUD 115200
#define OOB_FLOW FALSE

/*! TRUE if Out-of-band pairing data is to be sent */
static const bool_t ctsSendOobData = FALSE;

/* OOB Connection identifier */
dmConnId_t oobConnId;

/*! SMP security parameter configuration 
*
*   I/O Capability Codes to be set for 
*   Pairing Request (SMP_CMD_PAIR_REQ) packets and Pairing Response (SMP_CMD_PAIR_RSP) packets
*   when the MITM flag is set in Configurable security parameters above.
*       -SMP_IO_DISP_ONLY         : Display only. 
*       -SMP_IO_DISP_YES_NO       : Display yes/no. 
*       -SMP_IO_KEY_ONLY          : Keyboard only.
*       -SMP_IO_NO_IN_NO_OUT      : No input, no output. 
*       -SMP_IO_KEY_DISP          : Keyboard display. 
*/
static const smpCfg_t ctsSmpCfg = {
    500, /*! 'Repeated attempts' timeout in msec */
    SMP_IO_KEY_ONLY, /*! I/O Capability */
    7, /*! Minimum encryption key length */
    16, /*! Maximum encryption key length */
    1, /*! Attempts to trigger 'repeated attempts' timeout */
    0, /*! Device authentication requirements */
    64000, /*! Maximum repeated attempts timeout in msec */
    64000, /*! Time msec before attemptExp decreases */
    2 /*! Repeated attempts multiplier exponent */
};

/* iOS connection parameter update requirements

 The connection parameter request may be rejected if it does not meet the following guidelines:
 * Peripheral Latency of up to 30 connection intervals.
 * Supervision Timeout from 2 seconds to 6 seconds.
 * Interval Min of at least 15 ms.
 * Interval Min is a multiple of 15 ms.
 * One of the following:
   * Interval Max at least 15 ms greater than Interval Min.
   * Interval Max and Interval Min both set to 15 ms.
   * Interval Max * (Peripheral Latency + 1) of 2 seconds or less.
   * Supervision Timeout greater than Interval Max * (Peripheral Latency + 1) * 3.
*/

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t ctsUpdateCfg = {
    0,
    /*! ^ Connection idle period in ms before attempting
    connection parameter update. set to zero to disable */
    (15 * 8 / 1.25), /*! Minimum connection interval in 1.25ms units */
    (15 * 12 / 1.25), /*! Maximum connection interval in 1.25ms units */
    0, /*! Connection latency */
    600, /*! Supervision timeout in 10ms units */
    5 /*! Number of update attempts before giving up */
};

/*! ATT configurable parameters (increase MTU) */
static const attCfg_t ctsAttCfg = {
    15, /* ATT server service discovery connection idle timeout in seconds */
    241, /* desired ATT MTU */
    ATT_MAX_TRANS_TIMEOUT, /* transcation timeout in seconds */
    4 /* number of queued prepare writes supported by server */
};

/*! local IRK */
static uint8_t localIrk[] = { 0x95, 0xC8, 0xEE, 0x6F, 0xC5, 0x0D, 0xEF, 0x93,
                              0x35, 0x4E, 0x7C, 0x57, 0x08, 0xE2, 0xA3, 0x85 };

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t ctsAdvDataDisc[] = {
    /*! flags */
    2, /*! length */
    DM_ADV_TYPE_FLAGS, /*! AD type */
    DM_FLAG_LE_GENERAL_DISC | /*! flags */
        DM_FLAG_LE_BREDR_NOT_SUP,

    /*! manufacturer specific data */
    3, /*! length */
    DM_ADV_TYPE_MANUFACTURER, /*! AD type */
    UINT16_TO_BYTES(HCI_ID_ANALOG) /*! company ID */
};

/*! scan data, discoverable mode */
static const uint8_t ctsScanDataDisc[] = {
    /*! device name */
    4, /*! length */
    DM_ADV_TYPE_LOCAL_NAME, /*! AD type */
    'C', 'T', 'S'
};

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t ctsCccSet[DATS_NUM_CCC_IDX] = {
    /* cccd handle          value range               security level */
    { GATT_SC_CH_CCC_HDL, ATT_CLIENT_CFG_INDICATE, DM_SEC_LEVEL_NONE }, /* CTS_GATT_SC_CCC_IDX */
    { TIME_CTS_CT_CH_HDL, ATT_CLIENT_CFG_NOTIFY, DM_SEC_LEVEL_NONE }, /* CTS_CCC_IDX */

};

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
static struct {
    wsfHandlerId_t handlerId; /* WSF handler ID */
    appDbHdl_t resListRestoreHdl; /*! Resolving List last restored handle */
    bool_t restoringResList; /*! Restoring resolving list from NVM */
} ctsCb;

/* LESC OOB configuration */
static dmSecLescOobCfg_t *ctsOobCfg;

/* Timer for trimming of the 32 kHz crystal */
wsfTimer_t trimTimer;

extern void setAdvTxPower(void);

typedef enum {
    TIME_RTU_STATE_IDLE,
    TIME_RTU_STATE_PENDING,
    TIME_RTU_STATE_SUCCESS,
    TIME_RTU_STATE_CANCELED,
    TIME_RTU_STATE_NO_CONN_TO_REF,
    TIME_RTU_STATE_RESP_W_ERR,
    TIME_RTU_STATE_RESP_W_TIMEOUT,
} timeUpdateState_t;

enum{
    TIME_UPDATE_START = 0x1,
    TIME_UPDATE_CANCEL
};
static timeUpdateState_t updateState = TIME_RTU_STATE_IDLE;
static struct tm* getMxcRtcTime(void)
{
    uint32_t now;
    if (MXC_RTC_GetSeconds(&now) != E_NO_ERROR) {
        return NULL;
    }
    const time_t nowCasted = now;
    return localtime(&nowCasted);
}

static bool_t mxcRtcSetup(struct tm *t)
{
    volatile time_t nowSeconds = mktime(t);

    if (MXC_RTC_Init((uint32_t)nowSeconds, 0) != E_NO_ERROR) {
        APP_TRACE_INFO0("Failed to initialize RTC");
        return FALSE;
    }

    if (MXC_RTC_SquareWaveStart(MXC_RTC_F_1HZ) == E_BUSY) {
        APP_TRACE_INFO0("Couldnt start to RTC");
        return FALSE;
    }
    if (MXC_RTC_Start() != E_NO_ERROR) {
        APP_TRACE_INFO0("Couldnt start RTC");
        return FALSE;
    }

    return TRUE;
}

static uint8_t ctsReadTime(uint8_t *timedata)
{
    struct tm *t = getMxcRtcTime();

    const uint16_t  actual_year = t->tm_year + 1900;
    
    APP_TRACE_INFO4("YR: %d, %d:%d:%d",actual_year, t->tm_hour, t->tm_min, t->tm_sec);

    timedata[0] = actual_year & 0xFF;
    timedata[1] = (actual_year & 0xFF00) >> 8;
    timedata[2] = t->tm_mon;
    timedata[3] = t->tm_mday;
    timedata[4] = t->tm_hour;
    timedata[5] = t->tm_min;
    timedata[6] = t->tm_sec;


    return ATT_SUCCESS;
}
static timeUpdateState_t ctsGetUpdateTimeState(void)
{
    return updateState;
}
static uint8_t ctsTimeUpdate(uint8_t command)
{
    uint8_t ret;
    updateState = TIME_RTU_STATE_IDLE;
    
    switch (command)
    {
    case TIME_UPDATE_START:
        APP_TRACE_INFO0("No Ref to update from");
        ret = ATT_ERR_NOT_SUP;
        break;
    case TIME_UPDATE_CANCEL:
        APP_TRACE_INFO0("Cancelling ref upate");
        ret = ATT_ERR_NOT_SUP;
        break;
    default:
        ret = ATT_ERR_INVALID_PDU;
        break;
    }

    return ret;
}

uint8_t timeWriteCb(dmConnId_t connId, uint16_t handle, uint8_t operation, uint16_t offset,
                    uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    uint8_t ret = ATT_SUCCESS;
    APP_TRACE_INFO1("Write %d", handle);
    switch (handle) {
    case TIME_RTU_CP_HDL:         
        ret = ctsTimeUpdate(pAttr->pValue[0]);
        break;

    default:
        break;
    }

    return ret;
}
uint8_t timeReadCb(dmConnId_t connId, uint16_t handle, uint8_t operation, uint16_t offset,
                   attsAttr_t *pAttr)
{
    APP_TRACE_INFO1("Read %d", handle);

    uint8_t ret = ATT_SUCCESS;

    switch (handle) {
    case TIME_CTS_CT_HDL:
        ret = ctsReadTime(pAttr->pValue);
        break;
    case TIME_RTU_STATE_HDL:
        pAttr->pValue[1] = ctsGetUpdateTimeState();
        break;
    default:
        break;
    }

    return ret;
}
/*************************************************************************************************/
/*!
 *  \brief  OOB RX callback.
 *
 *  \return None.
 */
/*************************************************************************************************/
void oobRxCback(void)
{
    if (ctsOobCfg != NULL) {
        DmSecSetOob(oobConnId, ctsOobCfg);
    }

    DmSecAuthRsp(oobConnId, 0, NULL);
}

/*************************************************************************************************/
/*!
 *  \brief  Send notification containing data.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsSendData(dmConnId_t connId)
{
    uint8_t str[] = "hello back";

    if (AttsCccEnabled(connId, CTS_CCC_IDX)) {
        /* send notification */
        AttsHandleValueNtf(connId, WP_DAT_HDL, sizeof(str), str);
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsDmCback(dmEvt_t *pDmEvt)
{
    dmEvt_t *pMsg;
    uint16_t len;

    if (pDmEvt->hdr.event == DM_SEC_ECC_KEY_IND) {
        DmSecSetEccKey(&pDmEvt->eccMsg.data.key);

        /* If the local device sends OOB data. */
        if (ctsSendOobData) {
            uint8_t oobLocalRandom[SMP_RAND_LEN];
            SecRand(oobLocalRandom, SMP_RAND_LEN);
            DmSecCalcOobReq(oobLocalRandom, pDmEvt->eccMsg.data.key.pubKey_x);

            /* Setup HCI UART for OOB */
            PalUartConfig_t hciUartCfg;
            hciUartCfg.rdCback = oobRxCback;
            hciUartCfg.wrCback = NULL;
            hciUartCfg.baud = OOB_BAUD;
            hciUartCfg.hwFlow = OOB_FLOW;

            PalUartInit(PAL_UART_ID_CHCI, &hciUartCfg);
        }
    } else if (pDmEvt->hdr.event == DM_SEC_CALC_OOB_IND) {
        if (ctsOobCfg == NULL) {
            ctsOobCfg = WsfBufAlloc(sizeof(dmSecLescOobCfg_t));
            memset(ctsOobCfg, 0, sizeof(dmSecLescOobCfg_t));
        }

        if (ctsOobCfg) {
            Calc128Cpy(ctsOobCfg->localConfirm, pDmEvt->oobCalcInd.confirm);
            Calc128Cpy(ctsOobCfg->localRandom, pDmEvt->oobCalcInd.random);

            /* Start the RX for the peer OOB data */
            PalUartReadData(PAL_UART_ID_CHCI, ctsOobCfg->peerRandom,
                            (SMP_RAND_LEN + SMP_CONFIRM_LEN));
        } else {
            APP_TRACE_ERR0("Error allocating OOB data");
        }
    } else {
        len = DmSizeOfEvt(pDmEvt);

        if ((pMsg = WsfMsgAlloc(len)) != NULL) {
            memcpy(pMsg, pDmEvt, len);
            WsfMsgSend(ctsCb.handlerId, pMsg);
        }
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsAttCback(attEvt_t *pEvt)
{
    attEvt_t *pMsg;

    if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL) {
        memcpy(pMsg, pEvt, sizeof(attEvt_t));
        pMsg->pValue = (uint8_t *)(pMsg + 1);
        memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
        WsfMsgSend(ctsCb.handlerId, pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsCccCback(attsCccEvt_t *pEvt)
{
    appDbHdl_t dbHdl;

    /* If CCC not set from initialization and there's a device record and currently bonded */
    if ((pEvt->handle != ATT_HANDLE_NONE) &&
        ((dbHdl = AppDbGetHdl((dmConnId_t)pEvt->hdr.param)) != APP_DB_HDL_NONE) &&
        AppCheckBonded((dmConnId_t)pEvt->hdr.param)) {
        /* Store value in device database. */
        AppDbSetCccTblValue(dbHdl, pEvt->idx, pEvt->value);
        AppDbNvmStoreCccTbl(dbHdl);
    }
}

/*************************************************************************************************/
/*!
 *  \brief   Start the trim procedure for the 32 kHz crystal.
 *  \details 32 kHz crystal will drift over temperature, execute this trim procedure to align with 32 MHz clock.
 *  System will not be able to enter standby mode while this procedure is in progress (200-600 ms).
 *  Larger period values will save power. Do not execute this procedure while BLE hardware is disabled.
 *  Disable this periodic trim if under constant temperature. Refer to 32 kHz crystal data sheet for temperature variance.
 *
 *  \return  None.
 */
/*************************************************************************************************/
static void trimStart(void)
{
    int err;
    extern void wutTrimCb(int err);

    /* Start the 32 kHz crystal trim procedure */
    err = MXC_WUT_TrimCrystalAsync(wutTrimCb);
    if (err != E_NO_ERROR) {
        APP_TRACE_INFO1("Error starting 32kHz crystal trim %d", err);
    }
}

/*************************************************************************************************/
/*!
 *  \brief  ATTS write callback for proprietary data service.
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t ctsWpWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation, uint16_t offset,
                        uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    static uint32_t packetCount = 0;

    if (len < 64) {
        /* print received data if not a speed test message */
        APP_TRACE_INFO0((const char *)pValue);

        /* send back some data */
        ctsSendData(connId);
    } else {
        APP_TRACE_INFO1("Speed test packet Count [%d]", packetCount++);
        if (packetCount >= 5000) {
            packetCount = 0;
        }
    }
    return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \brief  ATTS write callback for secured data service.
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t secDatWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation, uint16_t offset,
                         uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
    uint8_t str[] = "Secure data received!";
    APP_TRACE_INFO0(">> Received secure data <<");
    APP_TRACE_INFO0((const char *)pValue);

    /* Write data recevied into characteristic */
    AttsSetAttr(SEC_DAT_HDL, len, (uint8_t *)pValue);
    /* if notifications are enabled send one */
    if (AttsCccEnabled(connId, DATS_SEC_DAT_CCC_IDX)) {
        /* send notification */
        AttsHandleValueNtf(connId, SEC_DAT_HDL, sizeof(str), str);
    }
    return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
*
*  \brief  Add device to resolving list.
*
*  \param  dbHdl   Device DB record handle.
*
*  \return None.
*/
/*************************************************************************************************/
static void ctsPrivAddDevToResList(appDbHdl_t dbHdl)
{
    dmSecKey_t *pPeerKey;

    /* if peer IRK present */
    if ((pPeerKey = AppDbGetKey(dbHdl, DM_KEY_IRK, NULL)) != NULL) {
        /* set advertising peer address */
        AppSetAdvPeerAddr(pPeerKey->irk.addrType, pPeerKey->irk.bdAddr);
    }
}

/*************************************************************************************************/
/*!
*
*  \brief  Handle remove device from resolving list indication.
*
*  \param  pMsg    Pointer to DM callback event message.
*
*  \return None.
*/
/*************************************************************************************************/
static void ctsPrivRemDevFromResListInd(dmEvt_t *pMsg)
{
    if (pMsg->hdr.status == HCI_SUCCESS) {
        if (AppDbGetHdl((dmConnId_t)pMsg->hdr.param) != APP_DB_HDL_NONE) {
            uint8_t addrZeros[BDA_ADDR_LEN] = { 0 };

            /* clear advertising peer address and its type */
            AppSetAdvPeerAddr(HCI_ADDR_TYPE_PUBLIC, addrZeros);
        }
    }
}

/*************************************************************************************************/
/*!
 *
 *  \brief  Display stack version.
 *
 *  \param  version    version number.
 *
 *  \return None.
 */
/*************************************************************************************************/
void ctsDisplayStackVersion(const char *pVersion)
{
    APP_TRACE_INFO1("Stack Version: %s", pVersion);
}

/*************************************************************************************************/
/*!
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsSetup(dmEvt_t *pMsg)
{
    /* Initialize control information */
    ctsCb.restoringResList = FALSE;

    /* set advertising and scan response data for discoverable mode */
    AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(ctsAdvDataDisc), (uint8_t *)ctsAdvDataDisc);
    AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(ctsScanDataDisc), (uint8_t *)ctsScanDataDisc);

    /* set advertising and scan response data for connectable mode */
    AppAdvSetData(APP_ADV_DATA_CONNECTABLE, sizeof(ctsAdvDataDisc), (uint8_t *)ctsAdvDataDisc);
    AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, sizeof(ctsScanDataDisc), (uint8_t *)ctsScanDataDisc);

    /* start advertising; automatically set connectable/discoverable mode and bondable mode */
    AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \brief  Begin restoring the resolving list.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsRestoreResolvingList(dmEvt_t *pMsg)
{
    /* Restore first device to resolving list in Controller. */
    ctsCb.resListRestoreHdl = AppAddNextDevToResList(APP_DB_HDL_NONE);

    if (ctsCb.resListRestoreHdl == APP_DB_HDL_NONE) {
        /* No device to restore.  Setup application. */
        ctsSetup(pMsg);
    } else {
        ctsCb.restoringResList = TRUE;
    }
}

/*************************************************************************************************/
/*!
*  \brief  Handle add device to resolving list indication.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsPrivAddDevToResListInd(dmEvt_t *pMsg)
{
    /* Check if in the process of restoring the Device List from NV */
    if (ctsCb.restoringResList) {
        /* Set the advertising peer address. */
        ctsPrivAddDevToResList(ctsCb.resListRestoreHdl);

        /* Retore next device to resolving list in Controller. */
        ctsCb.resListRestoreHdl = AppAddNextDevToResList(ctsCb.resListRestoreHdl);

        if (ctsCb.resListRestoreHdl == APP_DB_HDL_NONE) {
            /* No additional device to restore. Setup application. */
            ctsSetup(pMsg);
        }
    } else {
        ctsPrivAddDevToResList(AppDbGetHdl((dmConnId_t)pMsg->hdr.param));
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsProcMsg(dmEvt_t *pMsg)
{
    uint8_t uiEvent = APP_UI_NONE;

    switch (pMsg->hdr.event) {
    case DM_RESET_CMPL_IND:
        AttsCalculateDbHash();
        DmSecGenerateEccKeyReq();
        AppDbNvmReadAll();
        ctsRestoreResolvingList(pMsg);
        setAdvTxPower();
        uiEvent = APP_UI_RESET_CMPL;
        break;

    case DM_ADV_START_IND:
        WsfTimerStartMs(&trimTimer, TRIM_TIMER_PERIOD_MS);

        uiEvent = APP_UI_ADV_START;
        break;

    case DM_ADV_STOP_IND:
        WsfTimerStop(&trimTimer);
        uiEvent = APP_UI_ADV_STOP;
        break;

    case DM_CONN_OPEN_IND:
        uiEvent = APP_UI_CONN_OPEN;
        if (ctsSecCfg.initiateSec) {
            AppSlaveSecurityReq((dmConnId_t)pMsg->hdr.param);
        }
        break;

    case DM_CONN_CLOSE_IND:
        WsfTimerStop(&trimTimer);

        APP_TRACE_INFO2("Connection closed status 0x%x, reason 0x%x", pMsg->connClose.status,
                        pMsg->connClose.reason);
        switch (pMsg->connClose.reason) {
        case HCI_ERR_CONN_TIMEOUT:
            APP_TRACE_INFO0(" TIMEOUT");
            break;
        case HCI_ERR_LOCAL_TERMINATED:
            APP_TRACE_INFO0(" LOCAL TERM");
            break;
        case HCI_ERR_REMOTE_TERMINATED:
            APP_TRACE_INFO0(" REMOTE TERM");
            break;
        case HCI_ERR_CONN_FAIL:
            APP_TRACE_INFO0(" FAIL ESTABLISH");
            break;
        case HCI_ERR_MIC_FAILURE:
            APP_TRACE_INFO0(" MIC FAILURE");
            break;
        }
        uiEvent = APP_UI_CONN_CLOSE;
        break;

    case DM_SEC_PAIR_CMPL_IND:
        DmSecGenerateEccKeyReq();
        AppDbNvmStoreBond(AppDbGetHdl((dmConnId_t)pMsg->hdr.param));
        uiEvent = APP_UI_SEC_PAIR_CMPL;
        break;

    case DM_SEC_PAIR_FAIL_IND:
        DmSecGenerateEccKeyReq();
        uiEvent = APP_UI_SEC_PAIR_FAIL;
        break;

    case DM_SEC_ENCRYPT_IND:
        uiEvent = APP_UI_SEC_ENCRYPT;
        break;

    case DM_SEC_ENCRYPT_FAIL_IND:
        uiEvent = APP_UI_SEC_ENCRYPT_FAIL;
        break;

    case DM_SEC_AUTH_REQ_IND:

        if (pMsg->authReq.oob) {
            dmConnId_t connId = (dmConnId_t)pMsg->hdr.param;

            APP_TRACE_INFO0("Sending OOB data");
            oobConnId = connId;

            /* Start the TX to send the local OOB data */
            PalUartWriteData(PAL_UART_ID_CHCI, ctsOobCfg->localRandom,
                             (SMP_RAND_LEN + SMP_CONFIRM_LEN));

        } else {
            AppHandlePasskey(&pMsg->authReq);
        }
        break;

    case DM_SEC_COMPARE_IND:
        AppHandleNumericComparison(&pMsg->cnfInd);
        break;

    case DM_PRIV_ADD_DEV_TO_RES_LIST_IND:
        ctsPrivAddDevToResListInd(pMsg);
        break;

    case DM_PRIV_REM_DEV_FROM_RES_LIST_IND:
        ctsPrivRemDevFromResListInd(pMsg);
        break;

    case DM_ADV_NEW_ADDR_IND:
        break;

    case DM_PRIV_CLEAR_RES_LIST_IND:
        APP_TRACE_INFO1("Clear resolving list status 0x%02x", pMsg->hdr.status);
        break;

#if (BT_VER > 8)
    case DM_PHY_UPDATE_IND:
        APP_TRACE_INFO2("DM_PHY_UPDATE_IND - RX: %d, TX: %d", pMsg->phyUpdate.rxPhy,
                        pMsg->phyUpdate.txPhy);
        break;
#endif /* BT_VER */

    case TRIM_TIMER_EVT:
        trimStart();
        WsfTimerStartMs(&trimTimer, TRIM_TIMER_PERIOD_MS);
        break;

    default:
        break;
    }

    if (uiEvent != APP_UI_NONE) {
        AppUiAction(uiEvent);
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void CtsHandlerInit(wsfHandlerId_t handlerId)
{
    uint8_t addr[6] = { 0 };

    AppGetBdAddr(addr);
    APP_TRACE_INFO6("MAC Addr: %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2],
                    addr[1], addr[0]);
    APP_TRACE_INFO1("Adv local name: %s", &ctsScanDataDisc[2]);

    /* store handler ID */
    ctsCb.handlerId = handlerId;

    /* Set configuration pointers */
    pAppSlaveCfg = (appSlaveCfg_t *)&ctsSlaveCfg;
    pAppAdvCfg = (appAdvCfg_t *)&ctsAdvCfg;
    pAppSecCfg = (appSecCfg_t *)&ctsSecCfg;
    pAppUpdateCfg = (appUpdateCfg_t *)&ctsUpdateCfg;
    pSmpCfg = (smpCfg_t *)&ctsSmpCfg;
    pAttCfg = (attCfg_t *)&ctsAttCfg;

    /* Initialize application framework */
    AppSlaveInit();
    AppServerInit();

    /* Set IRK for the local device */
    DmSecSetLocalIrk(localIrk);

    /* Setup 32 kHz crystal trim timer */
    trimTimer.handlerId = handlerId;
    trimTimer.msg.event = TRIM_TIMER_EVT;
}

/*************************************************************************************************/
/*!
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsBtnCback(uint8_t btn)
{
#if (BT_VER > 8)
    dmConnId_t connId;
    if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
#else
    if (AppConnIsOpen() != DM_CONN_ID_NONE)
#endif /* BT_VER */
    {
        switch (btn) {
#if (BT_VER > 8)
        case APP_UI_BTN_2_SHORT: {
            static uint32_t coded_phy_cnt = 0;
            /* Toggle PHY Test Mode */
            coded_phy_cnt++;
            switch (coded_phy_cnt & 0x3) {
            case 0:
                /* 1M PHY */
                APP_TRACE_INFO0("1 MBit TX and RX PHY Requested");
                DmSetPhy(connId, HCI_ALL_PHY_ALL_PREFERENCES, HCI_PHY_LE_1M_BIT, HCI_PHY_LE_1M_BIT,
                         HCI_PHY_OPTIONS_NONE);
                break;
            case 1:
                /* 2M PHY */
                APP_TRACE_INFO0("2 MBit TX and RX PHY Requested");
                DmSetPhy(connId, HCI_ALL_PHY_ALL_PREFERENCES, HCI_PHY_LE_2M_BIT, HCI_PHY_LE_2M_BIT,
                         HCI_PHY_OPTIONS_NONE);
                break;
            case 2:
                /* Coded S2 PHY */
                APP_TRACE_INFO0("LE Coded S2 TX and RX PHY Requested");
                DmSetPhy(connId, HCI_ALL_PHY_ALL_PREFERENCES, HCI_PHY_LE_CODED_BIT,
                         HCI_PHY_LE_CODED_BIT, HCI_PHY_OPTIONS_S2_PREFERRED);
                break;
            case 3:
                /* Coded S8 PHY */
                APP_TRACE_INFO0("LE Coded S8 TX and RX PHY Requested");
                DmSetPhy(connId, HCI_ALL_PHY_ALL_PREFERENCES, HCI_PHY_LE_CODED_BIT,
                         HCI_PHY_LE_CODED_BIT, HCI_PHY_OPTIONS_S8_PREFERRED);
                break;
            }
            break;
        }
#endif /* BT_VER */

        default:
            APP_TRACE_INFO0(" - No action assigned");
            break;
        }
    } else {
        switch (btn) {
        case APP_UI_BTN_1_SHORT:
            /* start advertising */
            AppAdvStart(APP_MODE_AUTO_INIT);
            break;

        case APP_UI_BTN_1_MED:
            /* Enter bondable mode */
            AppSetBondable(TRUE);
            break;

        case APP_UI_BTN_1_LONG:
            /* clear all bonding info */
            AppSlaveClearAllBondingInfo();
            AppDbNvmDeleteAll();
            break;

        case APP_UI_BTN_1_EX_LONG: {
            const char *pVersion;
            StackGetVersionNumber(&pVersion);
            ctsDisplayStackVersion(pVersion);
        } break;

        case APP_UI_BTN_2_SHORT:
            /* stop advertising */
            AppAdvStop();
            break;

        default:
            APP_TRACE_INFO0(" - No action assigned");
            break;
        }
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Callback for WSF buffer diagnostic messages.
 *
 *  \param  pInfo     Diagnostics message
 *
 *  \return None.
 */
/*************************************************************************************************/
static void ctsWsfBufDiagnostics(WsfBufDiag_t *pInfo)
{
    if (pInfo->type == WSF_BUF_ALLOC_FAILED) {
        APP_TRACE_INFO2("Cts got WSF Buffer Allocation Failure - Task: %d Len: %d",
                        pInfo->param.alloc.taskId, pInfo->param.alloc.len);
    }
}

/*************************************************************************************************/
/*!
 *  \brief     Platform button press handler.
 *
 *  \param[in] btnId  button ID.
 *  \param[in] state  button state. See ::PalBtnPos_t.
 *
 *  \return    None.
 */
/*************************************************************************************************/
static void btnPressHandler(uint8_t btnId, PalBtnPos_t state)
{
    if (btnId == 1) {
        /* Start/stop button timer */
        if (state == PAL_BTN_POS_UP) {
            /* Button Up, stop the timer, call the action function */
            unsigned btnUs = MXC_TMR_SW_Stop(BTN_1_TMR);
            if ((btnUs > 0) && (btnUs < BTN_SHORT_MS * 1000)) {
                AppUiBtnTest(APP_UI_BTN_1_SHORT);
            } else if (btnUs < BTN_MED_MS * 1000) {
                AppUiBtnTest(APP_UI_BTN_1_MED);
            } else if (btnUs < BTN_LONG_MS * 1000) {
                AppUiBtnTest(APP_UI_BTN_1_LONG);
            } else {
                AppUiBtnTest(APP_UI_BTN_1_EX_LONG);
            }
        } else {
            /* Button down, start the timer */
            MXC_TMR_SW_Start(BTN_1_TMR);
        }
    } else if (btnId == 2) {
        /* Start/stop button timer */
        if (state == PAL_BTN_POS_UP) {
            /* Button Up, stop the timer, call the action function */
            unsigned btnUs = MXC_TMR_SW_Stop(BTN_2_TMR);
            if ((btnUs > 0) && (btnUs < BTN_SHORT_MS * 1000)) {
                AppUiBtnTest(APP_UI_BTN_2_SHORT);
            } else if (btnUs < BTN_MED_MS * 1000) {
                AppUiBtnTest(APP_UI_BTN_2_MED);
            } else if (btnUs < BTN_LONG_MS * 1000) {
                AppUiBtnTest(APP_UI_BTN_2_LONG);
            } else {
                AppUiBtnTest(APP_UI_BTN_2_EX_LONG);
            }
        } else {
            /* Button down, start the timer */
            MXC_TMR_SW_Start(BTN_2_TMR);
        }
    } else {
        APP_TRACE_ERR0("Undefined button");
    }
}

/*************************************************************************************************/
/*!
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void CtsHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
    if (pMsg != NULL) {
        APP_TRACE_INFO1("Cts got evt %d", pMsg->event);

        /* process ATT messages */
        if (pMsg->event >= ATT_CBACK_START && pMsg->event <= ATT_CBACK_END) {
            /* process server-related ATT messages */
            AppServerProcAttMsg(pMsg);
        } else if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END) {
            /* process DM messages */
            /* process advertising and connection-related messages */
            AppSlaveProcDmMsg((dmEvt_t *)pMsg);

            /* process security-related messages */
            AppSlaveSecProcDmMsg((dmEvt_t *)pMsg);
        }

        /* perform profile and user interface-related operations */
        ctsProcMsg((dmEvt_t *)pMsg);
    }
}

/*************************************************************************************************/
/*!
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void CtsStart(void)
{
    struct tm t = { .tm_hour = 12, .tm_mon = 10, .tm_year = 2023 - 1900 };
    mxcRtcSetup(&t);

    /* Register for stack callbacks */
    DmRegister(ctsDmCback);
    DmConnRegister(DM_CLIENT_ID_APP, ctsDmCback);
    AttRegister(ctsAttCback);
    AttConnRegister(AppServerConnCback);
    AttsCccRegister(DATS_NUM_CCC_IDX, (attsCccSet_t *)ctsCccSet, ctsCccCback);

    /* Initialize attribute server database */
    SvcCoreGattCbackRegister(GattReadCback, GattWriteCback);
    SvcCoreAddGroup();

    SvcTimeRegisterCback(timeReadCb, timeWriteCb);
    SvcTimeAddGroup();

    /* Register for app framework button callbacks */
    AppUiBtnRegister(ctsBtnCback);

#if (BT_VER > 8)
    DmPhyInit();
#endif /* BT_VER */

    WsfNvmInit();

    WsfBufDiagRegister(ctsWsfBufDiagnostics);

    /* Initialize with button press handler */
    PalBtnInit(btnPressHandler);

    /* Reset the device */
    DmDevReset();
}
