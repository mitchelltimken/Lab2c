#include "platform.h"
#include "xparameters.h"
#include "xscugic.h"
#include "xaxidma.h"
#include "xhls_accel.h"
#include "lib_xmmult_hw.h"
#include "xil_printf.h"
#include "xil_printf.h"

volatile static int RunExample = 0;
volatile static int ResultExample = 0;

XHls_accel xmmult_dev;
XHls_accel_Config xmmult_config = {
	0,
	XPAR_HLS_ACCEL_0_S_AXI_CONTROL_BUS_BASEADDR
};
XScuGic ScuGic;
extern XAxiDma AxiDma;

int XMmultSetup(){
	return XHls_accel_CfgInitialize(&xmmult_dev,&xmmult_config);
}

void XMmultStart(void *InstancePtr){
	XHls_accel *pExample = (XHls_accel *)InstancePtr;
	XHls_accel_InterruptEnable(pExample,1);
	XHls_accel_InterruptGlobalEnable(pExample);
	XHls_accel_Start(pExample);
}

void XMmultIsr(void *InstancePtr){
	XHls_accel *pExample = (XHls_accel *)InstancePtr;

	XHls_accel_InterruptGlobalDisable(pExample);
	XHls_accel_InterruptDisable(pExample,0xffffffff);
	XHls_accel_InterruptClear(pExample,1);

	ResultExample = 1;
	if(RunExample){
		XMmultStart(pExample);
	}
}

int XMmultSetupInterrupt()
{
	int result;
	XScuGic_Config *pCfg = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
	if (pCfg == NULL){
		print("Interrupt Configuration Lookup Failed\n\r");
		return XST_FAILURE;
	}
	result = XScuGic_CfgInitialize(&ScuGic,pCfg,pCfg->CpuBaseAddress);

	if(result != XST_SUCCESS){
		return result;
	}

	result = XScuGic_SelfTest(&ScuGic);

	if(result != XST_SUCCESS){
		return result;
	}

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,&ScuGic);
	Xil_ExceptionEnable();
	result = XScuGic_Connect(&ScuGic,XPAR_FABRIC_HLS_ACCEL_0_INTERRUPT_INTR,(Xil_InterruptHandler)XMmultIsr,&xmmult_dev);

	if(result != XST_SUCCESS){
		return result;
	}

	XScuGic_Enable(&ScuGic,XPAR_FABRIC_HLS_ACCEL_0_INTERRUPT_INTR);
	return XST_SUCCESS;
}

int Setup_HW_Accelerator(float A[DIM][DIM], float B[DIM][DIM], float res_hw[DIM][DIM], int dma_size)
{
	int status = XMmultSetup();
	if(status != XST_SUCCESS){
		print("Error: example setup failed\n");
		return XST_FAILURE;
	}
	status =  XMmultSetupInterrupt();
	if(status != XST_SUCCESS){
		print("Error: interrupt setup failed\n");
		return XST_FAILURE;
	}

	XMmultStart(&xmmult_dev);

	Xil_DCacheFlushRange((unsigned int)A,dma_size);
	Xil_DCacheFlushRange((unsigned int)B,dma_size);
	Xil_DCacheFlushRange((unsigned int)res_hw,dma_size);
	printf("\rCache cleared\n\r");

	return 0;
}


void matrix_multiply_ref(float a[DIM][DIM], float b[DIM][DIM], float out[DIM][DIM])
{
	//*insert code here*
}


void Start_HW_Accelerator(void)
{
	int status = XMmultSetup();

	if(status != XST_SUCCESS){
		print("Error: example setup failed\n");
		return XST_FAILURE;
	}

	status =  XMmultSetupInterrupt();

	if(status != XST_SUCCESS){
		print("Error: interrupt setup failed\n");
		return XST_FAILURE;
	}

	XMmultStart(&xmmult_dev);
}

int Run_HW_Accelerator(float A[DIM][DIM], float B[DIM][DIM], float res_hw[DIM][DIM], int dma_size)
{
	int status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) A, dma_size, XAXIDMA_DMA_TO_DEVICE);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE)) ;
	status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) B, dma_size, XAXIDMA_DMA_TO_DEVICE);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE)) ;
	status = XAxiDma_SimpleTransfer(&AxiDma, (unsigned int) res_hw, dma_size,
			XAXIDMA_DEVICE_TO_DMA);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE)) ;
	while ((XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) || (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE))) ;

	return 0;
}
