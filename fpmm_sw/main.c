#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xparameters.h"
#include "xtmrctr.h"
#include "xaxidma.h"
#include "lib_xmmult_hw.h"
#include "xil_printf.h"

#define NUM_TESTS 5
#define DIM	32
#define SIZE	(DIM*DIM)
#define XPAR_AXI_TIMER_DEVICE_ID	(XPAR_AXI_TIMER_0_DEVICE_ID)

XTmrCtr timer_dev;
XAxiDma AxiDma;

int init_dma(){
	XAxiDma_Config *CfgPtr;
	int status;

	CfgPtr = XAxiDma_LookupConfig( (XPAR_AXI_DMA_0_DEVICE_ID) );
	if(!CfgPtr){
		print("Error looking for AXI DMA config\n\r");
		return XST_FAILURE;
	}
	status = XAxiDma_CfgInitialize(&AxiDma,CfgPtr);
	if(status != XST_SUCCESS){
		print("Error initializing DMA\n\r");
		return XST_FAILURE;
	}

	if(XAxiDma_HasSg(&AxiDma)){
		print("Error DMA configured in SG mode\n\r");
		return XST_FAILURE;
	}

	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);

	XAxiDma_Reset(&AxiDma);
	while (!XAxiDma_ResetIsDone(&AxiDma)) {}

	return XST_SUCCESS;
}


int main(int argc, char **argv)
{
	int i, j, k;
	int error=0;
	int status;
	int num_of_trials = 1;
	float A[DIM][DIM];
	float B[DIM][DIM];
	float AxB_hw[DIM][DIM];
	float AxB_sw[DIM][DIM];
	unsigned int dma_size = SIZE * sizeof(float);
    float acc_factor;
	unsigned int init_time, curr_time, calibration, begin_time, end_time;
	unsigned int run_time_sw, run_time_hw = 0;

	init_platform();

	status = init_dma();
	if(status != XST_SUCCESS){
		printf("\rError: DMA initialization failed\n");
		return XST_FAILURE;
	}
	printf("\r\nDMA initialization done\n\r");

	status = XTmrCtr_Initialize(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	if(status != XST_SUCCESS)
	{
		printf("\rError: Timer setup failed\n");
		return XST_FAILURE;
	}
	XTmrCtr_SetOptions(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID, XTC_ENABLE_ALL_OPTION);

	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	init_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	curr_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	calibration = curr_time - init_time;

	XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	for (i = 0; i< NUM_TESTS; i++);
	end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
	run_time_sw = end_time - begin_time - calibration;
	printf("\rLoop time for %d iterations is %d cycles.\r\n\n", NUM_TESTS, run_time_sw);

	//*insert code here*

	for (k=0; k<num_of_trials; k++)
	{
		printf("\rRunning Matrix Mult in SW\n");
		XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
		begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);

		for (i = 0; i < NUM_TESTS; i++)
		{
			matrix_multiply_ref(A, B, AxB_sw);
		}

		end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
		run_time_sw = end_time - begin_time - calibration;
		printf("\rTotal run time for SW on Processor is %d cycles over %d tests.\r\n", run_time_sw/NUM_TESTS, NUM_TESTS);

		printf("\r\nRunning Matrix Mult in HW\n");
		XTmrCtr_Reset(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
		begin_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
		Setup_HW_Accelerator(A, B, AxB_hw, dma_size);

		for (i = 0; i < NUM_TESTS; i++) {
			Start_HW_Accelerator();
			Run_HW_Accelerator(A, B, AxB_hw, dma_size);
		}

		end_time = XTmrCtr_GetValue(&timer_dev, XPAR_AXI_TIMER_DEVICE_ID);
		run_time_hw = end_time - begin_time - calibration;
		printf("\rTotal run time for AXI DMA + HW accelerator is %d cycles over %d tests.\r\n", run_time_hw/NUM_TESTS, NUM_TESTS);

		//*insert code here*

		acc_factor = (float) run_time_sw / (float) run_time_hw;
		printf("\r\n\033[1mAcceleration factor is: %d.%d \033[0m\r\n\r\n", (int) acc_factor, (int) (acc_factor * 1000) % 1000);

	}

	if (error == 0)
		printf("\rSW and HW results match.\n\r");
	else
		printf("\rError: Results do not match.\n\r");


    cleanup_platform();
    return 0;
}
