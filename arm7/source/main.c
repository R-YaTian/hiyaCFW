/*---------------------------------------------------------------------------------

	ARM7 code for PXI example
	-- fincs

---------------------------------------------------------------------------------*/
#include <nds.h>

// Management structure and stack space for PXI server thread
static Thread hiyaThread;
alignas(8) static u8 hiyaThreadStack[1024];

#define SD_IRQ_STATUS (*(vu32*)0x400481C)

//---------------------------------------------------------------------------------
static int hiyaThreadMain(void* arg) {
//---------------------------------------------------------------------------------
	// Set up PXI mailbox, used to receive PXI command words
	Mailbox mb;
	u32 mb_slots[4];
	mailboxPrepare(&mb, mb_slots, sizeof(mb_slots)/4);
	pxiSetMailbox(PxiChannel_User0, &mb);

	// Main PXI message loop
	for (;;) {
		// Receive a message
		u32 msg = mailboxRecv(&mb);
		u32 retval = 0;

		switch (msg) {
			default: break;

			// Command 0: Write I2C: Bootflag = Warmboot/SkipHealthSafety
			case 0: {
				i2cLock();
				i2cWriteRegister(0x4A, 0x70, 0x01);
				i2cUnlock();

				break;
			}

			// Command 1: Send back SD_IRQ_STATUS
			case 1: {
				retval = SD_IRQ_STATUS;
				break;
			}
		}

		// Send a reply back to the ARM9
		pxiReply(PxiChannel_User0, retval);
	}

	return 0;
}

//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	// Read settings from NVRAM
	envReadNvramSettings();

	// Set up extended keypad server (X/Y/hinge)
	keypadStartExtServer();

	// Configure and enable VBlank interrupt
	lcdSetIrqMask(DISPSTAT_IE_ALL, DISPSTAT_IE_VBLANK);
	irqEnable(IRQ_VBLANK);

	// Set up RTC
	rtcInit();
	rtcSyncTime();

	// Initialize power management
	pmInit();

	// Set up touch screen driver
	touchInit();
	touchStartServer(80, MAIN_THREAD_PRIO);

	// Set up server thread
	threadPrepare(&hiyaThread, hiyaThreadMain, NULL, &hiyaThreadStack[sizeof(hiyaThreadStack)], MAIN_THREAD_PRIO);
	threadStart(&hiyaThread);

	// Keep the ARM7 mostly idle
	while (pmMainLoop()) {
		threadWaitForVBlank();
	}

	return 0;
}
