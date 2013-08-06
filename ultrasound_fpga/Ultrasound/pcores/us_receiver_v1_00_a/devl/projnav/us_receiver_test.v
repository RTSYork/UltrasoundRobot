`timescale 1ns / 1ps

////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer:
//
// Create Date:   17:48:25 07/31/2013
// Design Name:   us_receiver
// Module Name:   C:/Ultrasound/Ultrasound/pcores/us_receiver_v1_00_a/devl/projnav/us_receiver_test.v
// Project Name:  us_receiver
// Target Device:  
// Tool versions:  
// Description: 
//
// Verilog Test Fixture created by ISE for module: us_receiver
//
// Dependencies:
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
////////////////////////////////////////////////////////////////////////////////

module us_receiver_test;

	// Inputs
	reg SPI_MISO;
	reg ADC_DONE;
	reg FSL_Clk;
	reg FSL_Rst;
	reg FSL_S_Clk;
	reg [31:0] FSL_S_Data;
	reg FSL_S_Control;
	reg FSL_S_Exists;
	reg FSL_M_Clk;
	reg FSL_M_Full;

	// Outputs
	wire SPI_SCK;
	wire SPI_MOSI;
	wire SPI_SS;
	wire [2:0] DEBUG_OUT;
	wire FSL_S_Read;
	wire FSL_M_Write;
	wire [31:0] FSL_M_Data;
	wire FSL_M_Control;

	// Instantiate the Unit Under Test (UUT)
	us_receiver uut (
		.SPI_SCK(SPI_SCK), 
		.SPI_MISO(SPI_MISO), 
		.SPI_MOSI(SPI_MOSI), 
		.SPI_SS(SPI_SS), 
		.ADC_DONE(ADC_DONE), 
		.DEBUG_OUT(DEBUG_OUT), 
		.FSL_Clk(FSL_Clk), 
		.FSL_Rst(FSL_Rst), 
		.FSL_S_Clk(FSL_S_Clk), 
		.FSL_S_Read(FSL_S_Read), 
		.FSL_S_Data(FSL_S_Data), 
		.FSL_S_Control(FSL_S_Control), 
		.FSL_S_Exists(FSL_S_Exists), 
		.FSL_M_Clk(FSL_M_Clk), 
		.FSL_M_Write(FSL_M_Write), 
		.FSL_M_Data(FSL_M_Data), 
		.FSL_M_Control(FSL_M_Control), 
		.FSL_M_Full(FSL_M_Full)
	);

	initial begin
		// Initialize Inputs
		SPI_MISO = 0;
		ADC_DONE = 0;
		FSL_Clk = 0;
		FSL_Rst = 0;
		FSL_S_Clk = 0;
		FSL_S_Data = 0;
		FSL_S_Control = 0;
		FSL_S_Exists = 0;
		FSL_M_Clk = 0;
		FSL_M_Full = 0;

		// Wait 20 ns for global reset to finish
		#20;
		
		FSL_Rst = 1'b1;
		#10;
		FSL_Rst = 1'b0;
		#10;
		
		// Test Echo command
		FSL_S_Exists <= 1'b1;
		FSL_S_Data <= 32'hAAAAAAA_0;
		#15;
		FSL_S_Exists <= 1'b0;
		FSL_S_Data <= 32'b0;
		
		#45;
		/*
		// Test Reset command
		FSL_S_Exists <= 1'b1;
		FSL_S_Data <= 32'h0000000_F;
		#15;
		FSL_S_Exists <= 1'b0;
		FSL_S_Data <= 32'b0;
		
		#45;*/
		/*
		// Test Init command
		FSL_S_Exists <= 1'b1;
		FSL_S_Data <= 32'h0000000_3;
		#15;
		FSL_S_Exists <= 1'b0;
		FSL_S_Data <= 32'b0;
		
		#45;*/
		/*
		// Test Temp command
		FSL_S_Exists <= 1'b1;
		FSL_S_Data <= 32'h0000000_1;
		#15;
		FSL_S_Exists <= 1'b0;
		FSL_S_Data <= 32'b0;
		
		#45;*/
		
		// Test Sample command (2 samples, sensor 5)
		FSL_S_Exists <= 1'b1;
		FSL_S_Data <= 32'h000002_5_2;
		#15;
		FSL_S_Exists <= 1'b0;
		FSL_S_Data <= 32'b0;
		
	end
	
	
	always begin
		#5 FSL_Clk = ~FSL_Clk; // Toggle clock every 5 ticks
	end
      
endmodule

