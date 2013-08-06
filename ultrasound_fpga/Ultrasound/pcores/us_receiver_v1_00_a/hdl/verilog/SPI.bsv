// SPI Interface for Ultrasound Robot
package SPI;

interface SPI;
	method Action startSendRead(Bit#(8) data);
	method Action startRead();
	method ActionValue#(Bit#(8)) readByte();

	(* always_ready *)
	method Bool busy();

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
module mkSPI (SPI);
	
	Reg#(Bit#(8)) dataOut <- mkReg(0);
	Reg#(Bit#(8)) dataIn <- mkReg(0);

	Reg#(Bool) working <- mkReg(False);
	
	Reg#(Bit#(1)) bitIn <- mkReg(0);
	Reg#(Bit#(1)) bitOut <- mkReg(0);
	Reg#(Bool) outEnable <- mkReg(False);
	Reg#(UInt#(4)) bitCount <- mkReg(0);
	Reg#(Bool) transferDone <- mkReg(False);
	Reg#(Bool) hasData <- mkReg(False);

	Reg#(Bool) spiClk <- mkReg(False);
	Reg#(Bool) clkEnable <- mkReg(False);
	Reg#(UInt#(4)) clkDivide <- mkReg(0);
	Reg#(Bool) clockDataOut <- mkReg(False);
	Reg#(Bool) clockDataIn <- mkReg(False);

	Reg#(Bool) waitForSS <- mkReg(False);
	Reg#(UInt#(2)) ssTimer <- mkReg(0);
	

	// Send data
	rule sendBit (working && clockDataOut && !clockDataIn && bitCount != 0 && !waitForSS);
		clockDataOut <= False;
		bitOut <= dataOut[bitCount-1];
	endrule

	rule sendLastBit (working && clockDataOut && !clockDataIn && bitCount == 0 && !waitForSS);
		clockDataOut <= False;
		bitOut <= dataOut[0];
		clkEnable <= False;
	endrule

	// Receive data
	rule readBit (working && clockDataIn && !clockDataOut && bitCount != 0 && !waitForSS);
		clockDataIn <= False;
		dataIn[bitCount-1] <= bitIn;
		bitCount <= bitCount - 1;
	endrule

	rule readLastBit (working && clockDataIn && !clockDataOut && bitCount == 0 && !waitForSS);
		clockDataIn <= False;
		dataIn[0] <= bitIn;
		clkEnable <= False;
	endrule

	// Transfer is complete (8 bits sent/received)
	rule transferComplete (transferDone && working);
		transferDone <= False;
		clkDivide <= 0;
		outEnable <= False;
		hasData <= True;
		working <= False;
	endrule


	// Wait at least 40ns between SS and first SCLK
	rule ssWait (waitForSS && working && ssTimer != 0);
		ssTimer <= ssTimer - 1;
	endrule

	rule ssWaitDone (waitForSS && working && ssTimer == 0);
		waitForSS <= False;
		clkEnable <= True;
		bitOut <= dataOut[7];
		bitCount <= 8;
	endrule
	

	// Divide core clock by 16 for SPI clock rate of 6.25MHz
	rule clkCount(clkDivide != 0 && !transferDone);
		clkDivide <= clkDivide - 1;
	endrule

	rule clkUp(clkDivide == 0 && clkEnable && !spiClk && !clockDataIn && !clockDataOut && !transferDone);
		clkDivide <= 15;
		spiClk <= True;
		clockDataIn <= True;
	endrule

	rule clkDown(clkDivide == 0 && clkEnable && spiClk && !clockDataIn && !clockDataOut && !transferDone);
		clkDivide <= 15;
		spiClk <= False;
		clockDataOut <= True;
	endrule

	rule clkOff(clkDivide == 0 && !clkEnable && !transferDone && bitCount == 0 && working && !waitForSS);
		transferDone <= True;
	endrule


	// Start sending and reading data
	method Action startSendRead(Bit#(8) data) if (!working);
		working <= True;
		hasData <= False;
		dataOut <= data;
		outEnable <= True;
		ssTimer <= 3;
		waitForSS <= True;
	endmethod

	// Start reading data (sends 0x00)
	method Action startRead() if (!working);
		working <= True;
		hasData <= False;
		dataOut <= 0;
		outEnable <= True;
		ssTimer <= 3;
		waitForSS <= True;
	endmethod
	
	// Read received data
	method ActionValue#(Bit#(8)) readByte() if (hasData && !working);
		hasData <= False;
		return dataIn;
	endmethod

	// SPI bus busy
	method Bool busy();
		return working;
	endmethod


	// SPI bus signals
	method Action miso(Bit#(1) val);
		bitIn <= val;
	endmethod

	method Bool sck();
		return spiClk;
	endmethod

	method Bit#(1) mosi();
		return bitOut;
	endmethod

	method Bool ss();
		return !outEnable;
	endmethod

endmodule

endpackage: SPI