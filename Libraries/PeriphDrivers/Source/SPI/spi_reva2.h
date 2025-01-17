/******************************************************************************
 * Copyright (C) 2023 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 ******************************************************************************/

#ifndef LIBRARIES_PERIPHDRIVERS_SOURCE_SPI_SPI_REVA2_H_
#define LIBRARIES_PERIPHDRIVERS_SOURCE_SPI_SPI_REVA2_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "mxc_device.h"
#include "mxc_assert.h"
#include "mxc_lock.h"
#include "mxc_sys.h"
#include "mxc_delay.h"
#include "spi_regs.h"
#include "spi_reva_regs.h"
#include "spi.h"
#include "dma.h"
#include "dma_reva_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* **** Functions **** */

int MXC_SPI_RevA2_Init(mxc_spi_reva_regs_t *spi, mxc_spi_type_t controller_target,
                       mxc_spi_interface_t if_mode, uint32_t freq, uint8_t ts_active_pol_mask);

int MXC_SPI_RevA2_Config(mxc_spi_cfg_t *cfg);

int MXC_SPI_RevA2_Shutdown(mxc_spi_reva_regs_t *spi);

uint32_t MXC_SPI_RevA2_GetFlags(mxc_spi_reva_regs_t *spi);

void MXC_SPI_RevA2_ClearFlags(mxc_spi_reva_regs_t *spi);

void MXC_SPI_RevA2_EnableInt(mxc_spi_reva_regs_t *spi, uint32_t en);

void MXC_SPI_RevA2_DisableInt(mxc_spi_reva_regs_t *spi, uint32_t dis);

int MXC_SPI_RevA2_SetTSControl(mxc_spi_reva_regs_t *spi, mxc_spi_tscontrol_t ts_control);

mxc_spi_tscontrol_t MXC_SPI_RevA2_GetTSControl(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetFrequency(mxc_spi_reva_regs_t *spi, uint32_t freq);

int MXC_SPI_RevA2_GetFrequency(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetFrameSize(mxc_spi_reva_regs_t *spi, int frame_size);

int MXC_SPI_RevA2_GetFrameSize(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetInterface(mxc_spi_reva_regs_t *spi, mxc_spi_interface_t if_mode);

mxc_spi_interface_t MXC_SPI_RevA2_GetInterface(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetClkMode(mxc_spi_reva_regs_t *spi, mxc_spi_clkmode_t clk_mode);

mxc_spi_clkmode_t MXC_SPI_RevA2_GetClkMode(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetCallback(mxc_spi_reva_regs_t *spi, mxc_spi_callback_t callback, void *data);

int MXC_SPI_RevA2_GetActive(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_ReadyForSleep(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetDummyTX(mxc_spi_reva_regs_t *spi, uint16_t tx_value);

int MXC_SPI_RevA2_StartTransmission(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_AbortTransmission(mxc_spi_reva_regs_t *spi);

uint8_t MXC_SPI_RevA2_GetTXFIFOAvailable(mxc_spi_reva_regs_t *spi);

uint8_t MXC_SPI_RevA2_GetRXFIFOAvailable(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_ClearTXFIFO(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_ClearRXFIFO(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_SetTXThreshold(mxc_spi_reva_regs_t *spi, uint8_t thd_val);

int MXC_SPI_RevA2_SetRXThreshold(mxc_spi_reva_regs_t *spi, uint8_t thd_val);

uint8_t MXC_SPI_RevA2_GetTXThreshold(mxc_spi_reva_regs_t *spi);

uint8_t MXC_SPI_RevA2_GetRXThreshold(mxc_spi_reva_regs_t *spi);

/* ** DMA-Specific Functions ** */

int MXC_SPI_RevA2_DMA_Init(mxc_spi_reva_regs_t *spi, mxc_dma_reva_regs_t *dma, bool use_dma_tx,
                           bool use_dma_rx);

bool MXC_SPI_RevA2_DMA_GetInitialized(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_DMA_GetTXChannel(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_DMA_GetRXChannel(mxc_spi_reva_regs_t *spi);

int MXC_SPI_RevA2_DMA_SetRequestSelect(mxc_spi_reva_regs_t *spi, uint32_t tx_reqsel,
                                       uint32_t rx_reqsel);

void MXC_SPI_RevA2_DMA_SwapByte(uint8_t *buffer, uint32_t len_bytes);

/* ** Transaction Functions ** */

int MXC_SPI_RevA2_ControllerTransaction(mxc_spi_reva_regs_t *spi, uint8_t *tx_buffer,
                                        uint32_t tx_length_frames, uint8_t *rx_buffer,
                                        uint32_t rx_length_frames, uint8_t deassert,
                                        uint8_t hw_ts_index);

int MXC_SPI_RevA2_ControllerTransactionAsync(mxc_spi_reva_regs_t *spi, uint8_t *tx_buffer,
                                             uint32_t tx_length_frames, uint8_t *rx_buffer,
                                             uint32_t rx_length_frames, uint8_t deassert,
                                             uint8_t hw_ts_index);

int MXC_SPI_RevA2_ControllerTransactionDMA(mxc_spi_reva_regs_t *spi, uint8_t *tx_buffer,
                                           uint32_t tx_length_frames, uint8_t *rx_buffer,
                                           uint32_t rx_length_frames, uint8_t deassert,
                                           uint8_t hw_ts_index, mxc_dma_reva_regs_t *dma);

int MXC_SPI_RevA2_TargetTransaction(mxc_spi_reva_regs_t *spi, uint8_t *tx_buffer,
                                    uint32_t tx_length_frames, uint8_t *rx_buffer,
                                    uint32_t rx_length_frames);

int MXC_SPI_RevA2_TargetTransactionAsync(mxc_spi_reva_regs_t *spi, uint8_t *tx_buffer,
                                         uint32_t tx_length_frames, uint8_t *rx_buffer,
                                         uint32_t rx_length_frames);

int MXC_SPI_RevA2_TargetTransactionDMA(mxc_spi_reva_regs_t *spi, uint8_t *tx_buffer,
                                       uint32_t tx_length_frames, uint8_t *rx_buffer,
                                       uint32_t rx_length_frames, mxc_dma_reva_regs_t *dma);

/* ** Handler Functions ** */

void MXC_SPI_RevA2_Handler(mxc_spi_reva_regs_t *spi);

void MXC_SPI_RevA2_DMA_TX_Handler(mxc_spi_reva_regs_t *spi);

void MXC_SPI_RevA2_DMA_RX_Handler(mxc_spi_reva_regs_t *spi);

#ifdef __cplusplus
}
#endif

#endif // LIBRARIES_PERIPHDRIVERS_SOURCE_SPI_SPI_REVA2_H_
