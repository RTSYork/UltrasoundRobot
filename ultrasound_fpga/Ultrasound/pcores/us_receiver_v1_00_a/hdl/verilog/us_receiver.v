//----------------------------------------------------------------------------
// us_receiver - module
//----------------------------------------------------------------------------
// IMPORTANT:
// DO NOT MODIFY THIS FILE EXCEPT IN THE DESIGNATED SECTIONS.
//
// SEARCH FOR --USER TO DETERMINE WHERE CHANGES ARE ALLOWED.
//
// TYPICALLY, THE ONLY ACCEPTABLE CHANGES INVOLVE ADDING NEW
// PORTS AND GENERICS THAT GET PASSED THROUGH TO THE INSTANTIATION
// OF THE USER_LOGIC ENTITY.
//----------------------------------------------------------------------------
//
// ***************************************************************************
// ** Copyright (c) 1995-2012 Xilinx, Inc.  All rights reserved.            **
// **                                                                       **
// ** Xilinx, Inc.                                                          **
// ** XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS"         **
// ** AS A COURTESY TO YOU, SOLELY FOR USE IN DEVELOPING PROGRAMS AND       **
// ** SOLUTIONS FOR XILINX DEVICES.  BY PROVIDING THIS DESIGN, CODE,        **
// ** OR INFORMATION AS ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE,        **
// ** APPLICATION OR STANDARD, XILINX IS MAKING NO REPRESENTATION           **
// ** THAT THIS IMPLEMENTATION IS FREE FROM ANY CLAIMS OF INFRINGEMENT,     **
// ** AND YOU ARE RESPONSIBLE FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE      **
// ** FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY DISCLAIMS ANY              **
// ** WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE               **
// ** IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR        **
// ** REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF       **
// ** INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS       **
// ** FOR A PARTICULAR PURPOSE.                                             **
// **                                                                       **
// ***************************************************************************
//
//----------------------------------------------------------------------------
// Filename:          us_receiver
// Version:           1.00.a
// Description:       Example FSL core (Verilog).
// Date:              Tue Jul 23 17:46:04 2013 (by Create and Import Peripheral Wizard)
// Verilog Standard:  Verilog-2001
//----------------------------------------------------------------------------
// Naming Conventions:
//   active low signals:                    "*_n"
//   clock signals:                         "clk", "clk_div#", "clk_#x"
//   reset signals:                         "rst", "rst_n"
//   generics:                              "C_*"
//   user defined types:                    "*_TYPE"
//   state machine next state:              "*_ns"
//   state machine current state:           "*_cs"
//   combinatorial signals:                 "*_com"
//   pipelined or register delay signals:   "*_d#"
//   counter signals:                       "*cnt*"
//   clock enable signals:                  "*_ce"
//   internal version of output port:       "*_i"
//   device pins:                           "*_pin"
//   ports:                                 "- Names begin with Uppercase"
//   processes:                             "*_PROCESS"
//   component instantiations:              "<ENTITY_>I_<#|FUNC>"
//----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
//
//
// Definition of Ports
// FSL_Clk         : Synchronous clock
// FSL_Rst         : System reset, should always come from FSL bus
// FSL_S_Clk       : Slave asynchronous clock
// FSL_S_Read      : Read signal, requiring next available input to be read
// FSL_S_Data      : Input data
// FSL_S_Control   : Control Bit, indicating the input data are control word
// FSL_S_Exists    : Data Exist Bit, indicating data exist in the input FSL bus
// FSL_M_Clk       : Master asynchronous clock
// FSL_M_Write     : Write signal, enabling writing to output FSL bus
// FSL_M_Data      : Output data
// FSL_M_Control   : Control Bit, indicating the output data are contol word
// FSL_M_Full      : Full Bit, indicating output FSL bus is full
//
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------
// Module Section
//----------------------------------------
module us_receiver 
	(
		// ADD USER PORTS BELOW THIS LINE 
		SPI_SCK,
		SPI_MISO,
		SPI_MOSI,
		SPI_SS,
		ADC_DONE,
		DEBUG_OUT,
		// ADD USER PORTS ABOVE THIS LINE 

		// DO NOT EDIT BELOW THIS LINE ////////////////////
		// Bus protocol ports, do not add or delete. 
		FSL_Clk,
		FSL_Rst,
		FSL_S_Clk,
		FSL_S_Read,
		FSL_S_Data,
		FSL_S_Control,
		FSL_S_Exists,
		FSL_M_Clk,
		FSL_M_Write,
		FSL_M_Data,
		FSL_M_Control,
		FSL_M_Full
		// DO NOT EDIT ABOVE THIS LINE ////////////////////
	);

// ADD USER PORTS BELOW THIS LINE 
output                                    SPI_SCK;
input                                     SPI_MISO;
output                                    SPI_MOSI;
output                                    SPI_SS;
input                                     ADC_DONE;
output      [2 : 0]                       DEBUG_OUT;
// ADD USER PORTS ABOVE THIS LINE 

input                                     FSL_Clk;
input                                     FSL_Rst;
input                                     FSL_S_Clk;
output                                    FSL_S_Read;
input      [31 : 0]                       FSL_S_Data;
input                                     FSL_S_Control;
input                                     FSL_S_Exists;
input                                     FSL_M_Clk;
output                                    FSL_M_Write;
output     [31 : 0]                       FSL_M_Data;
output                                    FSL_M_Control;
input                                     FSL_M_Full;

// ADD USER PARAMETERS BELOW THIS LINE 

// ADD USER PARAMETERS ABOVE THIS LINE


//----------------------------------------
// Implementation Section
//----------------------------------------

	// Define the states of state machine
	localparam Idle            = 12'b000000000001;
	localparam Read_Input      = 12'b000000000010;
	localparam Process_Command = 12'b000000000100;
	localparam Write_Output    = 12'b000000001000;
	localparam Send_Init       = 12'b000000010000;
	localparam Confirm_Init    = 12'b000000100000;
	localparam Request_Temp    = 12'b000001000000;
	localparam Receive_Temp    = 12'b000010000000;
	localparam Start_Sampling  = 12'b000100000000;
	localparam Wait_To_Sample  = 12'b001000000000;
	localparam Request_Sample  = 12'b010000000000;
	localparam Receive_Sample  = 12'b100000000000;
	
	// Commands
	localparam Comm_Echo   = 4'h0;
	localparam Comm_Temp   = 4'h1;
	localparam Comm_Sample = 4'h2;
	localparam Comm_Init   = 4'h3;
	localparam Comm_Reset  = 4'hF;
	
	// Responses
	localparam Resp_Echo    = 3'h0;
	localparam Resp_Temp    = 3'h1;
	localparam Resp_Sample  = 3'h2;
	localparam Resp_None    = 3'h3;
	localparam Resp_Unknown = 3'h7;
	
	// Status
	localparam Status_OK    = 1'b0;
	localparam Status_Error = 1'b1;

	reg  [11:0] state;

	reg  [ 3:0] command;
	reg  [27:0] data_in;
	
	reg  [ 9:0] sample_count;
	reg  [11:0] sample_period;
	reg  [11:0] sample_timer;
	reg         sampling = 1'b0;
	
	reg         status;
	reg  [ 2:0] type;
	reg  [27:0] data_out;
	
	wire        ADC_init;
	wire        ADC_init_rdy;
	
	wire        ADC_request_temp;
	wire        ADC_request_temp_rdy;
	wire        ADC_read_temp;
	wire        ADC_read_temp_rdy;
	wire [15:0] ADC_temp;
	
	wire        ADC_request_sample;
	wire        ADC_request_sample_rdy;
	reg  [ 7:0] ADC_sample_sensor;
	wire        ADC_read_sample;
	wire        ADC_read_sample_rdy;
	wire [15:0] ADC_sample;
	
	
	assign FSL_M_Data  = {data_out, type, status};
	assign FSL_S_Read  = (state == Read_Input) ? FSL_S_Exists : 0;
	assign FSL_M_Write = (state == Write_Output) ? ~FSL_M_Full : 0;
	
	assign ADC_init = (state == Send_Init) ? ADC_init_rdy : 0;
	
	assign ADC_request_temp = (state == Request_Temp) ? ADC_request_temp_rdy : 0;
	assign ADC_read_temp    = (state == Receive_Temp) ? ADC_read_temp_rdy : 0;
	
	assign ADC_request_sample = (state == Request_Sample) ? ADC_request_sample_rdy : 0;
	assign ADC_read_sample    = (state == Receive_Sample) ? ADC_read_sample_rdy : 0;
	

	always @(posedge FSL_Clk) 
	begin
		if (FSL_Rst)
		begin
			state              <= Idle;
			command            <= 0;
			data_in            <= 0;
			status             <= 0;
			type               <= 0;
			data_out           <= 0;
			ADC_sample_sensor  <= 0;
			sample_count       <= 0;
			sample_period      <= 0;
			sample_timer       <= 0;
			sampling           <= 0;
		end
		else
		case (state)
			Idle:
				if (FSL_S_Exists == 1)
				begin
					state     <= Read_Input;
				end

			Read_Input:
				if (FSL_S_Exists == 1) 
				begin
					command   <= FSL_S_Data[3:0];
					data_in   <= FSL_S_Data[31:4];
					state     <= Process_Command;
				end

			Process_Command:
				case (command)
					Comm_Echo:
					begin
						// Echo input data back out to FSL
						status   <= Status_OK;
						type     <= Resp_Echo;
						data_out <= data_in;
						state    <= Write_Output;
					end
					Comm_Temp:
					begin
						// Measure temperature
						state <= Request_Temp;
					end
				 Comm_Sample:
					begin
						// Take samples
						ADC_sample_sensor <= {4'b0, data_in[3:0]};
						sample_count      <= data_in[13:4];
						sample_period     <= data_in[25:14];
						state             <= Start_Sampling;
					end
					Comm_Init:
					begin
						// Initialise ADC
						state <= Send_Init;
					end
					Comm_Reset:
					begin
						// Reset
						data_in           <= 0;
						ADC_sample_sensor <= 0;
						sample_count      <= 0;
						sample_period     <= 0;
						sample_timer      <= 0;
						sampling          <= 0;
						
						status   <= Status_OK;
						type     <= Resp_None;
						data_out <= 0;
						state    <= Write_Output;
					end
					default:
					begin
						// Unknown command, return error
						status   <= Status_Error;
						type     <= Resp_Unknown;
						data_out <= 0;
						state    <= Write_Output;
					end
				endcase

			Write_Output:
				if (sampling == 0)
					state <= Idle;
				else if (sample_count == 0)
				begin
					sampling <= 0;
					state <= Idle;
				end
				else
				begin
					if (sample_timer != 0)
						sample_timer <= sample_timer - 1;
					state <= Wait_To_Sample;
				end
				
			Send_Init:
				if (ADC_init_rdy == 1)
					state <= Confirm_Init;
				
			Confirm_Init:
				if (ADC_init_rdy == 1)
				begin
					status   <= Status_OK;
					type     <= Resp_None;
					data_out <= 0;
					state    <= Write_Output;
				end
			
			Request_Temp:
				if (ADC_request_temp_rdy == 1)
					state <= Receive_Temp;
			
			Receive_Temp:
				if (ADC_read_temp_rdy == 1)
				begin
					status   <= Status_OK;
					type     <= Resp_Temp;
					data_out <= {12'b0, ADC_temp};
					state    <= Write_Output;
				end
			
			Start_Sampling:
				if (sample_count == 0)
				begin
					sampling <= 0;
					state    <= Idle;
				end
				else
				begin
					sampling     <= 1;
					sample_timer <= sample_period - 1;
					state        <= Request_Sample;
				end
			
			Wait_To_Sample:
				if (sample_timer == 0)
				begin
					sample_timer <= sample_period - 1;
					state        <= Request_Sample;
				end
				else
					sample_timer <= sample_timer - 1;
			
			Request_Sample:
				begin
					if (sample_timer != 0)
						sample_timer <= sample_timer - 1;
					if (ADC_request_sample_rdy == 1)
					begin
						sample_count <= sample_count - 1;
						state        <= Receive_Sample;
					end
				end
			
			Receive_Sample:
				begin
					if (sample_timer != 0)
						sample_timer <= sample_timer - 1;
					if (ADC_read_sample_rdy == 1)
					begin
						status   <= Status_OK;
						type     <= Resp_Sample;
						data_out <= {12'b0, ADC_sample};
						state    <= Write_Output;
					end
				end
			
			default:
				state <= Idle;
			
		endcase
	end
	
	
	// ADC Controller
	// ---------------
	// Ports:
	// Name                         I/O  size props
	// RDY_init                       O     1
	// RDY_requestTemp                O     1
	// RDY_requestSample              O     1
	// readTemp                       O    16 reg
	// RDY_readTemp                   O     1 reg
	// readSample                     O    16 reg
	// RDY_readSample                 O     1 reg
	// sck                            O     1 reg
	// mosi                           O     1 reg
	// ss                             O     1 reg
	// CLK                            I     1 clock
	// RST_N                          I     1 reset
	// requestSample_sensor           I     4 unused
	// adc_done                       I     1 reg
	// miso_val                       I     1 reg
	// EN_init                        I     1
	// EN_requestTemp                 I     1
	// EN_requestSample               I     1
	// EN_readTemp                    I     1
	// EN_readSample                  I     1
	// ---------------
	mkADC ADC (
		.CLK                  (FSL_Clk),
		.RST_N                (~FSL_Rst),
		.adc_done             (~ADC_DONE),
		.miso_val             (SPI_MISO),
		.EN_init              (ADC_init),
		.EN_requestTemp       (ADC_request_temp),
		.EN_requestSample     (ADC_request_sample),
		.requestSample_sensor (ADC_sample_sensor),
		.EN_readTemp          (ADC_read_temp),
		.EN_readSample        (ADC_read_sample),
		
		.sck                  (SPI_SCK),
		.mosi                 (SPI_MOSI),
		.ss                   (SPI_SS),
		.RDY_init             (ADC_init_rdy),
		.RDY_requestTemp      (ADC_request_temp_rdy),
		.RDY_requestSample    (ADC_request_sample_rdy),
		.RDY_readTemp         (ADC_read_temp_rdy),
		.readTemp             (ADC_temp),
		.RDY_readSample       (ADC_read_sample_rdy),
		.readSample           (ADC_sample)
	);

endmodule
