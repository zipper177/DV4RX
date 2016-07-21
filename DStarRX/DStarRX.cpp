/*
*   Copyright (C) 2016 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "DStarRX.h"
#include "Version.h"
#include "DV4mini.h"
#include "AMBEFEC.h"
#include "Utils.h"
#include "CRC.h"
#include "Log.h"

#include "DStarDefines.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

const uint8_t BIT_MASK_TABLE0[] = { 0x7FU, 0xBFU, 0xDFU, 0xEFU, 0xF7U, 0xFBU, 0xFDU, 0xFEU };
const uint8_t BIT_MASK_TABLE1[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };
const uint8_t BIT_MASK_TABLE2[] = { 0xFEU, 0xFDU, 0xFBU, 0xF7U, 0xEFU, 0xDFU, 0xBFU, 0x7FU };
const uint8_t BIT_MASK_TABLE3[] = { 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U };

#define WRITE_BIT1(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE1[(i)&7]) : (p[(i)>>3] & BIT_MASK_TABLE0[(i)&7])
#define READ_BIT1(p,i)    (p[(i)>>3] & BIT_MASK_TABLE1[(i)&7])

#define WRITE_BIT2(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE3[(i)&7]) : (p[(i)>>3] & BIT_MASK_TABLE2[(i)&7])
#define READ_BIT2(p,i)    (p[(i)>>3] & BIT_MASK_TABLE3[(i)&7])

const uint8_t INTERLEAVE_TABLE_RX[] = {
	0x00U, 0x00U, 0x03U, 0x00U, 0x06U, 0x00U, 0x09U, 0x00U, 0x0CU, 0x00U,
	0x0FU, 0x00U, 0x12U, 0x00U, 0x15U, 0x00U, 0x18U, 0x00U, 0x1BU, 0x00U,
	0x1EU, 0x00U, 0x21U, 0x00U, 0x24U, 0x00U, 0x27U, 0x00U, 0x2AU, 0x00U,
	0x2DU, 0x00U, 0x30U, 0x00U, 0x33U, 0x00U, 0x36U, 0x00U, 0x39U, 0x00U,
	0x3CU, 0x00U, 0x3FU, 0x00U, 0x42U, 0x00U, 0x45U, 0x00U, 0x48U, 0x00U,
	0x4BU, 0x00U, 0x4EU, 0x00U, 0x51U, 0x00U, 0x00U, 0x01U, 0x03U, 0x01U,
	0x06U, 0x01U, 0x09U, 0x01U, 0x0CU, 0x01U, 0x0FU, 0x01U, 0x12U, 0x01U,
	0x15U, 0x01U, 0x18U, 0x01U, 0x1BU, 0x01U, 0x1EU, 0x01U, 0x21U, 0x01U,
	0x24U, 0x01U, 0x27U, 0x01U, 0x2AU, 0x01U, 0x2DU, 0x01U, 0x30U, 0x01U,
	0x33U, 0x01U, 0x36U, 0x01U, 0x39U, 0x01U, 0x3CU, 0x01U, 0x3FU, 0x01U,
	0x42U, 0x01U, 0x45U, 0x01U, 0x48U, 0x01U, 0x4BU, 0x01U, 0x4EU, 0x01U,
	0x51U, 0x01U, 0x00U, 0x02U, 0x03U, 0x02U, 0x06U, 0x02U, 0x09U, 0x02U,
	0x0CU, 0x02U, 0x0FU, 0x02U, 0x12U, 0x02U, 0x15U, 0x02U, 0x18U, 0x02U,
	0x1BU, 0x02U, 0x1EU, 0x02U, 0x21U, 0x02U, 0x24U, 0x02U, 0x27U, 0x02U,
	0x2AU, 0x02U, 0x2DU, 0x02U, 0x30U, 0x02U, 0x33U, 0x02U, 0x36U, 0x02U,
	0x39U, 0x02U, 0x3CU, 0x02U, 0x3FU, 0x02U, 0x42U, 0x02U, 0x45U, 0x02U,
	0x48U, 0x02U, 0x4BU, 0x02U, 0x4EU, 0x02U, 0x51U, 0x02U, 0x00U, 0x03U,
	0x03U, 0x03U, 0x06U, 0x03U, 0x09U, 0x03U, 0x0CU, 0x03U, 0x0FU, 0x03U,
	0x12U, 0x03U, 0x15U, 0x03U, 0x18U, 0x03U, 0x1BU, 0x03U, 0x1EU, 0x03U,
	0x21U, 0x03U, 0x24U, 0x03U, 0x27U, 0x03U, 0x2AU, 0x03U, 0x2DU, 0x03U,
	0x30U, 0x03U, 0x33U, 0x03U, 0x36U, 0x03U, 0x39U, 0x03U, 0x3CU, 0x03U,
	0x3FU, 0x03U, 0x42U, 0x03U, 0x45U, 0x03U, 0x48U, 0x03U, 0x4BU, 0x03U,
	0x4EU, 0x03U, 0x51U, 0x03U, 0x00U, 0x04U, 0x03U, 0x04U, 0x06U, 0x04U,
	0x09U, 0x04U, 0x0CU, 0x04U, 0x0FU, 0x04U, 0x12U, 0x04U, 0x15U, 0x04U,
	0x18U, 0x04U, 0x1BU, 0x04U, 0x1EU, 0x04U, 0x21U, 0x04U, 0x24U, 0x04U,
	0x27U, 0x04U, 0x2AU, 0x04U, 0x2DU, 0x04U, 0x30U, 0x04U, 0x33U, 0x04U,
	0x36U, 0x04U, 0x39U, 0x04U, 0x3CU, 0x04U, 0x3FU, 0x04U, 0x42U, 0x04U,
	0x45U, 0x04U, 0x48U, 0x04U, 0x4BU, 0x04U, 0x4EU, 0x04U, 0x51U, 0x04U,
	0x00U, 0x05U, 0x03U, 0x05U, 0x06U, 0x05U, 0x09U, 0x05U, 0x0CU, 0x05U,
	0x0FU, 0x05U, 0x12U, 0x05U, 0x15U, 0x05U, 0x18U, 0x05U, 0x1BU, 0x05U,
	0x1EU, 0x05U, 0x21U, 0x05U, 0x24U, 0x05U, 0x27U, 0x05U, 0x2AU, 0x05U,
	0x2DU, 0x05U, 0x30U, 0x05U, 0x33U, 0x05U, 0x36U, 0x05U, 0x39U, 0x05U,
	0x3CU, 0x05U, 0x3FU, 0x05U, 0x42U, 0x05U, 0x45U, 0x05U, 0x48U, 0x05U,
	0x4BU, 0x05U, 0x4EU, 0x05U, 0x51U, 0x05U, 0x00U, 0x06U, 0x03U, 0x06U,
	0x06U, 0x06U, 0x09U, 0x06U, 0x0CU, 0x06U, 0x0FU, 0x06U, 0x12U, 0x06U,
	0x15U, 0x06U, 0x18U, 0x06U, 0x1BU, 0x06U, 0x1EU, 0x06U, 0x21U, 0x06U,
	0x24U, 0x06U, 0x27U, 0x06U, 0x2AU, 0x06U, 0x2DU, 0x06U, 0x30U, 0x06U,
	0x33U, 0x06U, 0x36U, 0x06U, 0x39U, 0x06U, 0x3CU, 0x06U, 0x3FU, 0x06U,
	0x42U, 0x06U, 0x45U, 0x06U, 0x48U, 0x06U, 0x4BU, 0x06U, 0x4EU, 0x06U,
	0x51U, 0x06U, 0x00U, 0x07U, 0x03U, 0x07U, 0x06U, 0x07U, 0x09U, 0x07U,
	0x0CU, 0x07U, 0x0FU, 0x07U, 0x12U, 0x07U, 0x15U, 0x07U, 0x18U, 0x07U,
	0x1BU, 0x07U, 0x1EU, 0x07U, 0x21U, 0x07U, 0x24U, 0x07U, 0x27U, 0x07U,
	0x2AU, 0x07U, 0x2DU, 0x07U, 0x30U, 0x07U, 0x33U, 0x07U, 0x36U, 0x07U,
	0x39U, 0x07U, 0x3CU, 0x07U, 0x3FU, 0x07U, 0x42U, 0x07U, 0x45U, 0x07U,
	0x48U, 0x07U, 0x4BU, 0x07U, 0x4EU, 0x07U, 0x51U, 0x07U, 0x01U, 0x00U,
	0x04U, 0x00U, 0x07U, 0x00U, 0x0AU, 0x00U, 0x0DU, 0x00U, 0x10U, 0x00U,
	0x13U, 0x00U, 0x16U, 0x00U, 0x19U, 0x00U, 0x1CU, 0x00U, 0x1FU, 0x00U,
	0x22U, 0x00U, 0x25U, 0x00U, 0x28U, 0x00U, 0x2BU, 0x00U, 0x2EU, 0x00U,
	0x31U, 0x00U, 0x34U, 0x00U, 0x37U, 0x00U, 0x3AU, 0x00U, 0x3DU, 0x00U,
	0x40U, 0x00U, 0x43U, 0x00U, 0x46U, 0x00U, 0x49U, 0x00U, 0x4CU, 0x00U,
	0x4FU, 0x00U, 0x52U, 0x00U, 0x01U, 0x01U, 0x04U, 0x01U, 0x07U, 0x01U,
	0x0AU, 0x01U, 0x0DU, 0x01U, 0x10U, 0x01U, 0x13U, 0x01U, 0x16U, 0x01U,
	0x19U, 0x01U, 0x1CU, 0x01U, 0x1FU, 0x01U, 0x22U, 0x01U, 0x25U, 0x01U,
	0x28U, 0x01U, 0x2BU, 0x01U, 0x2EU, 0x01U, 0x31U, 0x01U, 0x34U, 0x01U,
	0x37U, 0x01U, 0x3AU, 0x01U, 0x3DU, 0x01U, 0x40U, 0x01U, 0x43U, 0x01U,
	0x46U, 0x01U, 0x49U, 0x01U, 0x4CU, 0x01U, 0x4FU, 0x01U, 0x52U, 0x01U,
	0x01U, 0x02U, 0x04U, 0x02U, 0x07U, 0x02U, 0x0AU, 0x02U, 0x0DU, 0x02U,
	0x10U, 0x02U, 0x13U, 0x02U, 0x16U, 0x02U, 0x19U, 0x02U, 0x1CU, 0x02U,
	0x1FU, 0x02U, 0x22U, 0x02U, 0x25U, 0x02U, 0x28U, 0x02U, 0x2BU, 0x02U,
	0x2EU, 0x02U, 0x31U, 0x02U, 0x34U, 0x02U, 0x37U, 0x02U, 0x3AU, 0x02U,
	0x3DU, 0x02U, 0x40U, 0x02U, 0x43U, 0x02U, 0x46U, 0x02U, 0x49U, 0x02U,
	0x4CU, 0x02U, 0x4FU, 0x02U, 0x52U, 0x02U, 0x01U, 0x03U, 0x04U, 0x03U,
	0x07U, 0x03U, 0x0AU, 0x03U, 0x0DU, 0x03U, 0x10U, 0x03U, 0x13U, 0x03U,
	0x16U, 0x03U, 0x19U, 0x03U, 0x1CU, 0x03U, 0x1FU, 0x03U, 0x22U, 0x03U,
	0x25U, 0x03U, 0x28U, 0x03U, 0x2BU, 0x03U, 0x2EU, 0x03U, 0x31U, 0x03U,
	0x34U, 0x03U, 0x37U, 0x03U, 0x3AU, 0x03U, 0x3DU, 0x03U, 0x40U, 0x03U,
	0x43U, 0x03U, 0x46U, 0x03U, 0x49U, 0x03U, 0x4CU, 0x03U, 0x4FU, 0x03U,
	0x52U, 0x03U, 0x01U, 0x04U, 0x04U, 0x04U, 0x07U, 0x04U, 0x0AU, 0x04U,
	0x0DU, 0x04U, 0x10U, 0x04U, 0x13U, 0x04U, 0x16U, 0x04U, 0x19U, 0x04U,
	0x1CU, 0x04U, 0x1FU, 0x04U, 0x22U, 0x04U, 0x25U, 0x04U, 0x28U, 0x04U,
	0x2BU, 0x04U, 0x2EU, 0x04U, 0x31U, 0x04U, 0x34U, 0x04U, 0x37U, 0x04U,
	0x3AU, 0x04U, 0x3DU, 0x04U, 0x40U, 0x04U, 0x43U, 0x04U, 0x46U, 0x04U,
	0x49U, 0x04U, 0x4CU, 0x04U, 0x4FU, 0x04U, 0x01U, 0x05U, 0x04U, 0x05U,
	0x07U, 0x05U, 0x0AU, 0x05U, 0x0DU, 0x05U, 0x10U, 0x05U, 0x13U, 0x05U,
	0x16U, 0x05U, 0x19U, 0x05U, 0x1CU, 0x05U, 0x1FU, 0x05U, 0x22U, 0x05U,
	0x25U, 0x05U, 0x28U, 0x05U, 0x2BU, 0x05U, 0x2EU, 0x05U, 0x31U, 0x05U,
	0x34U, 0x05U, 0x37U, 0x05U, 0x3AU, 0x05U, 0x3DU, 0x05U, 0x40U, 0x05U,
	0x43U, 0x05U, 0x46U, 0x05U, 0x49U, 0x05U, 0x4CU, 0x05U, 0x4FU, 0x05U,
	0x01U, 0x06U, 0x04U, 0x06U, 0x07U, 0x06U, 0x0AU, 0x06U, 0x0DU, 0x06U,
	0x10U, 0x06U, 0x13U, 0x06U, 0x16U, 0x06U, 0x19U, 0x06U, 0x1CU, 0x06U,
	0x1FU, 0x06U, 0x22U, 0x06U, 0x25U, 0x06U, 0x28U, 0x06U, 0x2BU, 0x06U,
	0x2EU, 0x06U, 0x31U, 0x06U, 0x34U, 0x06U, 0x37U, 0x06U, 0x3AU, 0x06U,
	0x3DU, 0x06U, 0x40U, 0x06U, 0x43U, 0x06U, 0x46U, 0x06U, 0x49U, 0x06U,
	0x4CU, 0x06U, 0x4FU, 0x06U, 0x01U, 0x07U, 0x04U, 0x07U, 0x07U, 0x07U,
	0x0AU, 0x07U, 0x0DU, 0x07U, 0x10U, 0x07U, 0x13U, 0x07U, 0x16U, 0x07U,
	0x19U, 0x07U, 0x1CU, 0x07U, 0x1FU, 0x07U, 0x22U, 0x07U, 0x25U, 0x07U,
	0x28U, 0x07U, 0x2BU, 0x07U, 0x2EU, 0x07U, 0x31U, 0x07U, 0x34U, 0x07U,
	0x37U, 0x07U, 0x3AU, 0x07U, 0x3DU, 0x07U, 0x40U, 0x07U, 0x43U, 0x07U,
	0x46U, 0x07U, 0x49U, 0x07U, 0x4CU, 0x07U, 0x4FU, 0x07U, 0x02U, 0x00U,
	0x05U, 0x00U, 0x08U, 0x00U, 0x0BU, 0x00U, 0x0EU, 0x00U, 0x11U, 0x00U,
	0x14U, 0x00U, 0x17U, 0x00U, 0x1AU, 0x00U, 0x1DU, 0x00U, 0x20U, 0x00U,
	0x23U, 0x00U, 0x26U, 0x00U, 0x29U, 0x00U, 0x2CU, 0x00U, 0x2FU, 0x00U,
	0x32U, 0x00U, 0x35U, 0x00U, 0x38U, 0x00U, 0x3BU, 0x00U, 0x3EU, 0x00U,
	0x41U, 0x00U, 0x44U, 0x00U, 0x47U, 0x00U, 0x4AU, 0x00U, 0x4DU, 0x00U,
	0x50U, 0x00U, 0x02U, 0x01U, 0x05U, 0x01U, 0x08U, 0x01U, 0x0BU, 0x01U,
	0x0EU, 0x01U, 0x11U, 0x01U, 0x14U, 0x01U, 0x17U, 0x01U, 0x1AU, 0x01U,
	0x1DU, 0x01U, 0x20U, 0x01U, 0x23U, 0x01U, 0x26U, 0x01U, 0x29U, 0x01U,
	0x2CU, 0x01U, 0x2FU, 0x01U, 0x32U, 0x01U, 0x35U, 0x01U, 0x38U, 0x01U,
	0x3BU, 0x01U, 0x3EU, 0x01U, 0x41U, 0x01U, 0x44U, 0x01U, 0x47U, 0x01U,
	0x4AU, 0x01U, 0x4DU, 0x01U, 0x50U, 0x01U, 0x02U, 0x02U, 0x05U, 0x02U,
	0x08U, 0x02U, 0x0BU, 0x02U, 0x0EU, 0x02U, 0x11U, 0x02U, 0x14U, 0x02U,
	0x17U, 0x02U, 0x1AU, 0x02U, 0x1DU, 0x02U, 0x20U, 0x02U, 0x23U, 0x02U,
	0x26U, 0x02U, 0x29U, 0x02U, 0x2CU, 0x02U, 0x2FU, 0x02U, 0x32U, 0x02U,
	0x35U, 0x02U, 0x38U, 0x02U, 0x3BU, 0x02U, 0x3EU, 0x02U, 0x41U, 0x02U,
	0x44U, 0x02U, 0x47U, 0x02U, 0x4AU, 0x02U, 0x4DU, 0x02U, 0x50U, 0x02U,
	0x02U, 0x03U, 0x05U, 0x03U, 0x08U, 0x03U, 0x0BU, 0x03U, 0x0EU, 0x03U,
	0x11U, 0x03U, 0x14U, 0x03U, 0x17U, 0x03U, 0x1AU, 0x03U, 0x1DU, 0x03U,
	0x20U, 0x03U, 0x23U, 0x03U, 0x26U, 0x03U, 0x29U, 0x03U, 0x2CU, 0x03U,
	0x2FU, 0x03U, 0x32U, 0x03U, 0x35U, 0x03U, 0x38U, 0x03U, 0x3BU, 0x03U,
	0x3EU, 0x03U, 0x41U, 0x03U, 0x44U, 0x03U, 0x47U, 0x03U, 0x4AU, 0x03U,
	0x4DU, 0x03U, 0x50U, 0x03U, 0x02U, 0x04U, 0x05U, 0x04U, 0x08U, 0x04U,
	0x0BU, 0x04U, 0x0EU, 0x04U, 0x11U, 0x04U, 0x14U, 0x04U, 0x17U, 0x04U,
	0x1AU, 0x04U, 0x1DU, 0x04U, 0x20U, 0x04U, 0x23U, 0x04U, 0x26U, 0x04U,
	0x29U, 0x04U, 0x2CU, 0x04U, 0x2FU, 0x04U, 0x32U, 0x04U, 0x35U, 0x04U,
	0x38U, 0x04U, 0x3BU, 0x04U, 0x3EU, 0x04U, 0x41U, 0x04U, 0x44U, 0x04U,
	0x47U, 0x04U, 0x4AU, 0x04U, 0x4DU, 0x04U, 0x50U, 0x04U, 0x02U, 0x05U,
	0x05U, 0x05U, 0x08U, 0x05U, 0x0BU, 0x05U, 0x0EU, 0x05U, 0x11U, 0x05U,
	0x14U, 0x05U, 0x17U, 0x05U, 0x1AU, 0x05U, 0x1DU, 0x05U, 0x20U, 0x05U,
	0x23U, 0x05U, 0x26U, 0x05U, 0x29U, 0x05U, 0x2CU, 0x05U, 0x2FU, 0x05U,
	0x32U, 0x05U, 0x35U, 0x05U, 0x38U, 0x05U, 0x3BU, 0x05U, 0x3EU, 0x05U,
	0x41U, 0x05U, 0x44U, 0x05U, 0x47U, 0x05U, 0x4AU, 0x05U, 0x4DU, 0x05U,
	0x50U, 0x05U, 0x02U, 0x06U, 0x05U, 0x06U, 0x08U, 0x06U, 0x0BU, 0x06U,
	0x0EU, 0x06U, 0x11U, 0x06U, 0x14U, 0x06U, 0x17U, 0x06U, 0x1AU, 0x06U,
	0x1DU, 0x06U, 0x20U, 0x06U, 0x23U, 0x06U, 0x26U, 0x06U, 0x29U, 0x06U,
	0x2CU, 0x06U, 0x2FU, 0x06U, 0x32U, 0x06U, 0x35U, 0x06U, 0x38U, 0x06U,
	0x3BU, 0x06U, 0x3EU, 0x06U, 0x41U, 0x06U, 0x44U, 0x06U, 0x47U, 0x06U,
	0x4AU, 0x06U, 0x4DU, 0x06U, 0x50U, 0x06U, 0x02U, 0x07U, 0x05U, 0x07U,
	0x08U, 0x07U, 0x0BU, 0x07U, 0x0EU, 0x07U, 0x11U, 0x07U, 0x14U, 0x07U,
	0x17U, 0x07U, 0x1AU, 0x07U, 0x1DU, 0x07U, 0x20U, 0x07U, 0x23U, 0x07U,
	0x26U, 0x07U, 0x29U, 0x07U, 0x2CU, 0x07U, 0x2FU, 0x07U, 0x32U, 0x07U,
	0x35U, 0x07U, 0x38U, 0x07U, 0x3BU, 0x07U, 0x3EU, 0x07U, 0x41U, 0x07U,
	0x44U, 0x07U, 0x47U, 0x07U, 0x4AU, 0x07U, 0x4DU, 0x07U, 0x50U, 0x07U,
};

const uint8_t SCRAMBLE_TABLE_RX[] = {
	0x70U, 0x4FU, 0x93U, 0x40U, 0x64U, 0x74U, 0x6DU, 0x30U, 0x2BU, 0xE7U,
	0x2DU, 0x54U, 0x5FU, 0x8AU, 0x1DU, 0x7FU, 0xB8U, 0xA7U, 0x49U, 0x20U,
	0x32U, 0xBAU, 0x36U, 0x98U, 0x95U, 0xF3U, 0x16U, 0xAAU, 0x2FU, 0xC5U,
	0x8EU, 0x3FU, 0xDCU, 0xD3U, 0x24U, 0x10U, 0x19U, 0x5DU, 0x1BU, 0xCCU,
	0xCAU, 0x79U, 0x0BU, 0xD5U, 0x97U, 0x62U, 0xC7U, 0x1FU, 0xEEU, 0x69U,
	0x12U, 0x88U, 0x8CU, 0xAEU, 0x0DU, 0x66U, 0xE5U, 0xBCU, 0x85U, 0xEAU,
	0x4BU, 0xB1U, 0xE3U, 0x0FU, 0xF7U, 0x34U, 0x09U, 0x44U, 0x46U, 0xD7U,
	0x06U, 0xB3U, 0x72U, 0xDEU, 0x42U, 0xF5U, 0xA5U, 0xD8U, 0xF1U, 0x87U,
	0x7BU, 0x9AU, 0x04U, 0x22U, 0xA3U, 0x6BU, 0x83U, 0x59U, 0x39U, 0x6FU,
	0x00U };

// D-Star bit order version of 0x55 0x55 0x6E 0x0A
const uint32_t FRAME_SYNC_DATA = 0x00557650U;
const uint32_t FRAME_SYNC_MASK = 0x00FFFFFFU;
const uint8_t  FRAME_SYNC_ERRS = 2U;

// D-Star bit order version of 0x55 0x2D 0x16
const uint32_t DATA_SYNC_DATA = 0x00AAB468U;
const uint32_t DATA_SYNC_MASK = 0x00FFFFFFU;
const uint8_t  DATA_SYNC_ERRS = 2U;

// D-Star bit order version of 0x55 0x55 0xC8 0x7A
const uint32_t END_SYNC_DATA = 0xAAAA135EU;
const uint32_t END_SYNC_MASK = 0xFFFFFFFFU;
const uint8_t  END_SYNC_ERRS = 3U;

int main(int argc, char** argv)
{
	if (argc == 2 && (::strcmp(argv[1], "-v") == 0 || ::strcmp(argv[1], "--version") == 0)) {
		::fprintf(stdout, "DStarRX version %s\n", VERSION);
		return 0;
	}

	if (argc != 3 && argc != 5) {
		::fprintf(stderr, "Usage: DStarRX [-v|--version] <port> <frequency> [<address> <port>]\n");
		return 1;
	}

	std::string port = std::string(argv[1U]);
	unsigned int frequency = ::atoi(argv[2U]);

	CDStarRX rx(port, frequency);

	if (argc == 5) {
		std::string address = std::string(argv[3U]);
		unsigned int port = ::atoi(argv[4U]);

		bool ret = rx.output(address, port);
		if (!ret) {
			::fprintf(stderr, "DStarRX: cannot open the UDP output port\n");
			return 1;
		}
	}

	::LogInitialise(".", "DStarRX", 1U, 1U);

	rx.run();

	::LogFinalise();

	return 0;
}

CDStarRX::CDStarRX(const std::string& port, unsigned int frequency) :
m_port(port),
m_frequency(frequency),
m_udpAddress(),
m_udpPort(0U),
m_socket(NULL),
m_bits(0U),
m_count(0U),
m_pos(0U),
m_state(RX_NONE),
m_buffer(NULL),
m_mar(0U),
m_pathMetric(),
m_pathMemory0(),
m_pathMemory1(),
m_pathMemory2(),
m_pathMemory3(),
m_fecOutput()
{
	m_buffer = new unsigned char[DSTAR_FEC_SECTION_LENGTH_BYTES];
}

CDStarRX::~CDStarRX()
{
	delete[] m_buffer;
}

bool CDStarRX::output(const std::string& address, unsigned int port)
{
	m_udpAddress = CUDPSocket::lookup(address);
	if (m_udpAddress.s_addr == INADDR_NONE)
		return false;

	m_udpPort = port;

	m_socket = new CUDPSocket;

	bool ret = m_socket->open();
	if (!ret) {
		delete m_socket;
		return false;
	}

	return true;
}

void CDStarRX::run()
{
	CDV4mini dv4mini(m_port, m_frequency);

	bool ret = dv4mini.open();
	if (!ret)
		return;

	LogMessage("DStarRX-%s starting", VERSION);

	for (;;) {
		unsigned char buffer[300U];
		unsigned int len = dv4mini.read(buffer, 300U);

		if (len > 0U) {
			unsigned int length = buffer[5U];
			decode(buffer + 6U, length);
		}

#if defined(_WIN32) || defined(_WIN64)
		::Sleep(5UL);		// 5ms
#else
		::usleep(5000);		// 5ms
#endif
	}

	dv4mini.close();

	if (m_socket != NULL) {
		m_socket->close();
		delete m_socket;
	}
}

void CDStarRX::decode(const unsigned char* data, unsigned int length)
{
	for (unsigned int i = 0U; i < (length * 8U); i++) {
		bool b = READ_BIT1(data, i) != 0x00U;

		m_bits <<= 1;
		if (b)
			m_bits |= 0x01U;

		if (m_state == RX_DATA) {
			processData(b);
		} else if (m_state == RX_HEADER) {
			processHeader(b);
		} else {
			uint32_t v = (m_bits & FRAME_SYNC_MASK) ^ FRAME_SYNC_DATA;

			unsigned int errs = 0U;
			while (v != 0U) {
				v &= v - 1U;
				errs++;
			}

			if (errs < FRAME_SYNC_ERRS) {
				m_state = RX_HEADER;
				m_count = 0U;
				m_pos   = 0U;
			}

			v = (m_bits & DATA_SYNC_MASK) ^ DATA_SYNC_DATA;

			errs = 0U;
			while (v != 0U) {
				v &= v - 1U;
				errs++;
			}

			if (errs < DATA_SYNC_ERRS) {
				m_state = RX_DATA;
				m_count = 0U;
				m_pos   = 0U;
			}
		}
	}
}

void CDStarRX::processData(bool b)
{
	WRITE_BIT2(m_buffer, m_pos, b);
	m_pos++;

	uint32_t v = (m_bits & DATA_SYNC_MASK) ^ DATA_SYNC_DATA;

	unsigned int errs = 0U;
	while (v != 0U) {
		v &= v - 1U;
		errs++;
	}

	if (errs < DATA_SYNC_ERRS)
		m_count = 0U;

	v = (m_bits & END_SYNC_MASK) ^ END_SYNC_DATA;

	errs = 0U;
	while (v != 0U) {
		v &= v - 1U;
		errs++;
	}

	if (errs < END_SYNC_ERRS) {
		LogMessage("End of transmission");
		m_state = RX_NONE;
		return;
	}

	if (m_pos == DSTAR_DATA_LENGTH_BITS) {
		CAMBEFEC fec;
		unsigned int ber = fec.regenerateDStar(m_buffer);
		LogMessage("Audio BER=%.1f%% Data=%02X %02X %02X %c %c %c", float(ber) / 0.48F,
			m_buffer[9U]  ^ DSTAR_SCRAMBLER_BYTES[0U],
			m_buffer[10U] ^ DSTAR_SCRAMBLER_BYTES[1U],
			m_buffer[11U] ^ DSTAR_SCRAMBLER_BYTES[2U],
			m_buffer[9U]  ^ DSTAR_SCRAMBLER_BYTES[0U],
			m_buffer[10U] ^ DSTAR_SCRAMBLER_BYTES[1U],
			m_buffer[11U] ^ DSTAR_SCRAMBLER_BYTES[2U]);
		m_pos = 0U;

		if (m_socket != NULL)
			m_socket->write(m_buffer, DSTAR_AMBE_LENGTH_BYTES, m_udpAddress, m_udpPort);

		m_count++;
		if (m_count >= (21U * 3U)) {
			LogMessage("Signal lost");
			m_state = RX_NONE;
			return;
		}
	}
}

void CDStarRX::processHeader(bool b)
{
	WRITE_BIT2(m_buffer, m_pos, b);
	m_pos++;

	if (m_pos == DSTAR_FEC_SECTION_LENGTH_BITS) {
		unsigned char header[DSTAR_HEADER_LENGTH_BYTES];
		bool ok = rxHeader(m_buffer, header);
		if (ok) {
			LogMessage("Header: %02X %02X %02X UR: %8.8s MY: %8.8s/%4.4s R1: %8.8s R2: %8.8s",
				header[0U], header[1U], header[2U], header + 19U, header + 27U, header + 35U, header + 3U, header + 11U);
			m_pos = 0U;
			m_state = RX_DATA;
		} else {
			m_state = RX_NONE;
		}
	}
}

bool CDStarRX::rxHeader(unsigned char* in, unsigned char* out)
{
	int i;

	// Descramble the header
	for (i = 0; i < int(DSTAR_FEC_SECTION_LENGTH_BYTES); i++)
		in[i] ^= SCRAMBLE_TABLE_RX[i];

	unsigned char intermediate[84U];
	for (i = 0; i < 84; i++)
		intermediate[i] = 0x00U;

	// Deinterleave the header
	i = 0;
	while (i < 660) {
		unsigned char d = in[i / 8];

		if (d & 0x01U)
			intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
		i++;

		if (d & 0x02U)
			intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
		i++;

		if (d & 0x04U)
			intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
		i++;

		if (d & 0x08U)
			intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
		i++;

		if (i < 660) {
			if (d & 0x10U)
				intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
			i++;

			if (d & 0x20U)
				intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
			i++;

			if (d & 0x40U)
				intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
			i++;

			if (d & 0x80U)
				intermediate[INTERLEAVE_TABLE_RX[i * 2U]] |= (0x80U >> INTERLEAVE_TABLE_RX[i * 2U + 1U]);
			i++;
		}
	}

	for (i = 0; i < 4; i++)
		m_pathMetric[i] = 0;

	int decodeData[2U];

	m_mar = 0U;
	for (i = 0; i < 660; i += 2) {
		if (intermediate[i >> 3] & (0x80U >> (i & 7)))
			decodeData[1U] = 1U;
		else
			decodeData[1U] = 0U;

		if (intermediate[i >> 3] & (0x40U >> (i & 7)))
			decodeData[0U] = 1U;
		else
			decodeData[0U] = 0U;

		viterbiDecode(decodeData);
	}

	traceBack();

	for (i = 0; i < int(DSTAR_HEADER_LENGTH_BYTES); i++)
		out[i] = 0x00U;

	unsigned int j = 0;
	for (i = 329; i >= 0; i--) {
		if (READ_BIT1(m_fecOutput, i))
			out[j >> 3] |= (0x01U << (j & 7));

		j++;
	}

	return CCRC::checkCCITT161(out, DSTAR_HEADER_LENGTH_BYTES);
}

void CDStarRX::acs(int* metric)
{
	int tempMetric[4U];

	unsigned int j = m_mar >> 3;
	unsigned int k = m_mar & 7;

	// Pres. state = S0, Prev. state = S0 & S2
	int m1 = metric[0U] + m_pathMetric[0U];
	int m2 = metric[4U] + m_pathMetric[2U];
	tempMetric[0U] = m1 < m2 ? m1 : m2;
	if (m1 < m2)
		m_pathMemory0[j] &= BIT_MASK_TABLE0[k];
	else
		m_pathMemory0[j] |= BIT_MASK_TABLE1[k];

	// Pres. state = S1, Prev. state = S0 & S2
	m1 = metric[1U] + m_pathMetric[0U];
	m2 = metric[5U] + m_pathMetric[2U];
	tempMetric[1U] = m1 < m2 ? m1 : m2;
	if (m1 < m2)
		m_pathMemory1[j] &= BIT_MASK_TABLE0[k];
	else
		m_pathMemory1[j] |= BIT_MASK_TABLE1[k];

	// Pres. state = S2, Prev. state = S2 & S3
	m1 = metric[2U] + m_pathMetric[1U];
	m2 = metric[6U] + m_pathMetric[3U];
	tempMetric[2U] = m1 < m2 ? m1 : m2;
	if (m1 < m2)
		m_pathMemory2[j] &= BIT_MASK_TABLE0[k];
	else
		m_pathMemory2[j] |= BIT_MASK_TABLE1[k];

	// Pres. state = S3, Prev. state = S1 & S3
	m1 = metric[3U] + m_pathMetric[1U];
	m2 = metric[7U] + m_pathMetric[3U];
	tempMetric[3U] = m1 < m2 ? m1 : m2;
	if (m1 < m2)
		m_pathMemory3[j] &= BIT_MASK_TABLE0[k];
	else
		m_pathMemory3[j] |= BIT_MASK_TABLE1[k];

	for (unsigned int i = 0U; i < 4U; i++)
		m_pathMetric[i] = tempMetric[i];

	m_mar++;
}

void CDStarRX::viterbiDecode(int* data)
{
	int metric[8U];

	metric[0] = (data[1] ^ 0) + (data[0] ^ 0);
	metric[1] = (data[1] ^ 1) + (data[0] ^ 1);
	metric[2] = (data[1] ^ 1) + (data[0] ^ 0);
	metric[3] = (data[1] ^ 0) + (data[0] ^ 1);
	metric[4] = (data[1] ^ 1) + (data[0] ^ 1);
	metric[5] = (data[1] ^ 0) + (data[0] ^ 0);
	metric[6] = (data[1] ^ 0) + (data[0] ^ 1);
	metric[7] = (data[1] ^ 1) + (data[0] ^ 0);

	acs(metric);
}

void CDStarRX::traceBack()
{
	// Start from the S0, t=31
	unsigned int j = 0U;
	unsigned int k = 0U;
	for (int i = 329; i >= 0; i--) {
		switch (j) {
		case 0U: // if state = S0
			if (!READ_BIT1(m_pathMemory0, i))
				j = 0U;
			else
				j = 2U;
			WRITE_BIT1(m_fecOutput, k, false);
			k++;
			break;


		case 1U: // if state = S1
			if (!READ_BIT1(m_pathMemory1, i))
				j = 0U;
			else
				j = 2U;
			WRITE_BIT1(m_fecOutput, k, true);
			k++;
			break;

		case 2U: // if state = S1
			if (!READ_BIT1(m_pathMemory2, i))
				j = 1U;
			else
				j = 3U;
			WRITE_BIT1(m_fecOutput, k, false);
			k++;
			break;

		case 3U: // if state = S1
			if (!READ_BIT1(m_pathMemory3, i))
				j = 1U;
			else
				j = 3U;
			WRITE_BIT1(m_fecOutput, k, true);
			k++;
			break;
		}
	}
}
