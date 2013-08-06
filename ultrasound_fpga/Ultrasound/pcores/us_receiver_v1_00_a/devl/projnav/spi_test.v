`timescale 1ns / 1ps

////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer:
//
// Create Date:   13:13:27 08/01/2013
// Design Name:   mkSPI
// Module Name:   C:/Ultrasound/Ultrasound/pcores/us_receiver_v1_00_a/devl/projnav/spi_test.v
// Project Name:  us_receiver
// Target Device:  
// Tool versions:  
// Description: 
//
// Verilog Test Fixture created by ISE for module: mkSPI
//
// Dependencies:
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
////////////////////////////////////////////////////////////////////////////////

module spi_test;

	// Inputs
	reg CLK;
	reg RST_N;
	reg [7:0] startSendRead_data;
	reg EN_startSendRead;
	reg EN_startRead;
	reg EN_readByte;
	reg miso_val;

	// Outputs
	wire RDY_startSendRead;
	wire RDY_startRead;
	wire [7:0] readByte;
	wire RDY_readByte;
	wire busy;
	wire sck;
	wire mosi;
	wire ss;

	// Instantiate the Unit Under Test (UUT)
	mkSPI uut (
		.CLK(CLK), 
		.RST_N(RST_N), 
		.startSendRead_data(startSendRead_data), 
		.EN_startSendRead(EN_startSendRead), 
		.RDY_startSendRead(RDY_startSendRead), 
		.EN_startRead(EN_startRead), 
		.RDY_startRead(RDY_startRead), 
		.EN_readByte(EN_readByte), 
		.readByte(readByte), 
		.RDY_readByte(RDY_readByte), 
		.busy(busy), 
		.miso_val(miso_val), 
		.sck(sck), 
		.mosi(mosi), 
		.ss(ss)
	);

	initial begin
		// Initialize Inputs
		CLK = 0;
		RST_N = 0;
		startSendRead_data = 0;
		EN_startSendRead = 0;
		EN_startRead = 0;
		EN_readByte = 0;
		miso_val = 0;

		#10;
		RST_N = 1;
		#10;
        
		// Test sending data
		startSendRead_data = 8'hAA;
		EN_startSendRead = 1'b1;
		#10
		EN_startSendRead = 1'b0;
		startSendRead_data = 8'h00;
		miso_val = 8'hFF;

	end
	
	
	always begin
		#5 CLK = ~CLK; // Toggle clock every 5 ticks
	end
      
endmodule

