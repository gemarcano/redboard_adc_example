// SPDX-License-Identifier: Apache-2.0
// Copyright: Gabriel Marcano, 2023

#include <adc.h>
#include <syscalls.h>

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

#include <string.h>
#include <assert.h>
#include <stdint.h>

int main(void)
{
	static struct adc adc;

	// Prepare MCU by init-ing clock, cache, and power level operation
	am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);
	am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
	am_hal_cachectrl_enable();
	am_bsp_low_power_init();
	am_hal_sysctrl_fpu_enable();
	am_hal_sysctrl_fpu_stacking_enable(true);

	// Init UART
	struct uart *uart = uart_get_instance(UART_INST0);
	syscalls_uart_init(uart);

	// Initialize the ADC.
	static const uint8_t pins[] = { 16 };
	adc_init(&adc, pins, 1);

	// After init is done, enable interrupts
	am_hal_interrupt_master_enable();

	// Print the banner.
	printf("Hello World!\r\n\r\n");

	// Print the device info.
	am_util_id_t device_id;
	am_util_id_device(&device_id);
	printf("Vendor Name: %s\r\n", device_id.pui8VendorName);
	printf("Device type: %s\r\n", device_id.pui8DeviceName);

	printf("Qualified: %s\r\n",
						 device_id.sMcuCtrlDevice.ui32Qualified ?
						 "Yes" : "No");

	printf("Device Info:\r\n"
						 "\tPart number: 0x%08X\r\n"
						 "\tChip ID0:	0x%08X\r\n"
						 "\tChip ID1:	0x%08X\r\n"
						 "\tRevision:	0x%08X (Rev%c%c)\r\n",
						 device_id.sMcuCtrlDevice.ui32ChipPN,
						 device_id.sMcuCtrlDevice.ui32ChipID0,
						 device_id.sMcuCtrlDevice.ui32ChipID1,
						 device_id.sMcuCtrlDevice.ui32ChipRev,
						 device_id.ui8ChipRevMaj, device_id.ui8ChipRevMin );

	// If not a multiple of 1024 bytes, append a plus sign to the KB.
	uint32_t mem_size = ( device_id.sMcuCtrlDevice.ui32FlashSize % 1024 ) ? '+' : 0;
	printf("\tFlash size:  %7d (%d KB%s)\r\n",
						 device_id.sMcuCtrlDevice.ui32FlashSize,
						 device_id.sMcuCtrlDevice.ui32FlashSize / 1024,
						 &mem_size);

	mem_size = ( device_id.sMcuCtrlDevice.ui32SRAMSize % 1024 ) ? '+' : 0;
	printf("\tSRAM size:   %7d (%d KB%s)\r\n\r\n",
						 device_id.sMcuCtrlDevice.ui32SRAMSize,
						 device_id.sMcuCtrlDevice.ui32SRAMSize / 1024,
						 &mem_size);

	printf("App Compiler:	%s\r\n", COMPILER_VERSION);
	printf("HAL Compiler:	%s\r\n", g_ui8HALcompiler);
	printf("HAL SDK version: %d.%d.%d\r\n",
						 g_ui32HALversion.s.Major,
						 g_ui32HALversion.s.Minor,
						 g_ui32HALversion.s.Revision);
	printf("HAL compiled with %s-style registers\r\n",
						 g_ui32HALversion.s.bAMREGS ? "AM_REG" : "CMSIS");

	am_hal_security_info_t security_info;
	uint32_t status = am_hal_security_get_info(&security_info);
	if (status == AM_HAL_STATUS_SUCCESS)
	{
		char string_buffer[32];
		if (security_info.bInfo0Valid)
		{
			sprintf(string_buffer, "INFO0 valid, ver 0x%X", security_info.info0Version);
		}
		else
		{
			sprintf(string_buffer, "INFO0 invalid");
		}

		printf("SBL ver: 0x%x - 0x%x, %s\r\n",
			security_info.sblVersion, security_info.sblVersionAddInfo, string_buffer);
	}
	else
	{
		printf("am_hal_security_get_info failed 0x%X\r\n", status);
	}

	// Trigger the ADC to start collecting data

	// Wait here for the ISR to grab a buffer of samples.
	while (1)
	{
		// Print the battery voltage and temperature for each interrupt
		//
		uint32_t data = 0;
		adc_trigger(&adc);
		while (!(adc_get_sample(&adc, &data, pins, 1)));
		// The math here is straight forward: we've asked the ADC to give
		// us data in 14 bits (max value of 2^14 -1). We also specified the
		// reference voltage to be 1.5V. A reading of 1.5V would be
		// translated to the maximum value of 2^14-1. So we divide the
		// value from the ADC by this maximum, and multiply it by the
		// reference, which then gives us the actual voltage measured.
		const double reference = 1.5;
		double voltage = data * reference / ((1 << 14) - 1);

		printf(
			"voltage = <%.3f> (0x%04X) ", voltage, data);

		printf("\r\n");
		am_util_delay_ms(100);
	}
}
