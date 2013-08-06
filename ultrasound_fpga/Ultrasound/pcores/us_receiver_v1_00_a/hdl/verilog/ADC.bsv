// ADC Interface for Ultrasound Robot
package ADC;

import SPI::*;

interface ADC;

	method Action init();
	method Action requestTemp();
	method Action requestSample(Bit#(8) sensor);
	method ActionValue#(Bit#(16)) readTemp();
	method ActionValue#(Bit#(16)) readSample();

	(* always_enabled *)
	method Action adc(Bool done);
	(* always_enabled *)
	method Action miso(Bit#(1) val);
	(* always_ready *)
	method Bool sck();
	(* always_ready *)
	method Bit#(1) mosi();
	(* always_ready *)
	method Bool ss();

endinterface


(* synthesize *)
module mkADC (ADC);

	SPI spi <- mkSPI;
	Reg#(Bool) adcDone <- mkReg(False);
	Reg#(Bool) initialising <- mkReg(False);
	Reg#(Bool) readingTemp <- mkReg(False);
	Reg#(Bool) readingSample <- mkReg(False);
	Reg#(Bool) hasTemp <- mkReg(False);
	Reg#(Bool) hasSample <- mkReg(False);
	Reg#(Bit#(16)) data <- mkReg(0);
	Reg#(UInt#(2)) readState <- mkReg(0);


	rule initDone(initialising && !spi.busy());
		initialising <= False;
	endrule


	rule tempDone1(readingTemp && !readingSample && !spi.busy() && readState == 0 && adcDone);
		readState <= 1;
		spi.startRead();
	endrule

	rule tempDone2(readingTemp && !readingSample && !spi.busy() && readState == 1);
		readState <= 2;
		let dataByte <- spi.readByte();
		data[15:8] <= dataByte;
	endrule

	rule tempDone3(readingTemp && !readingSample && !spi.busy() && readState == 2);
		readState <= 3;
		spi.startRead();
	endrule

	rule tempDone4(readingTemp && !readingSample && !spi.busy() && readState == 3);
		readState <= 0;
		let dataByte <- spi.readByte();
		data[7:0] <= dataByte;
		readingTemp <= False;
		hasTemp <= True;
	endrule


	rule sampleDone1(readingSample && !readingTemp && !spi.busy() && readState == 0 && adcDone);
		readState <= 1;
		spi.startRead();
	endrule

	rule sampleDone2(readingSample && !readingTemp && !spi.busy() && readState == 1);
		readState <= 2;
		let dataByte <- spi.readByte();
		data[15:6] <= {2'b0, dataByte};
	endrule

	rule sampleDone3(readingSample && !readingTemp && !spi.busy() && readState == 2);
		readState <= 3;
		spi.startRead();
	endrule

	rule sampleDone4(readingSample && !readingTemp && !spi.busy() && readState == 3);
		readState <= 0;
		let dataByte <- spi.readByte();
		data[5:0] <= dataByte[7:2];
		readingSample <= False;
		hasSample <= True;
	endrule


	method Action init() if (!initialising && !readingTemp && !readingSample);
		initialising <= True;
		spi.startSendRead(8'h64);
	endmethod


	method Action requestTemp() if (!initialising && !readingTemp && !readingSample);
		readingTemp <= True;
		hasTemp <= False;
		hasSample <= False;
		readState <= 0;
		spi.startSendRead(8'hF9);
	endmethod
	
	method Action requestSample(Bit#(8) sensor) if (!initialising && !readingTemp && !readingSample);
		readingSample <= True;
		hasTemp <= False;
		hasSample <= False;
		readState <= 0;
		spi.startSendRead(8'h86 | ((sensor & 8'h0F) << 3));
	endmethod


	method ActionValue#(Bit#(16)) readTemp() if (hasTemp && !readingTemp);
		hasTemp <= False;
		return data;
	endmethod

	method ActionValue#(Bit#(16)) readSample() if (hasSample && !readingSample);
		hasSample <= False;
		return data;
	endmethod


	method Action adc(Bool done);
		adcDone <= done;
	endmethod

	method Action miso(Bit#(1) val);
		spi.miso(val);
	endmethod

	method Bool sck();
		return spi.sck();
	endmethod

	method Bit#(1) mosi();
		return spi.mosi();
	endmethod

	method Bool ss();
		return spi.ss();
	endmethod

endmodule

endpackage: ADC