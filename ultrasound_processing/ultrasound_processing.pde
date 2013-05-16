import processing.serial.*;
import java.awt.event.KeyEvent;
import java.io.*;
import javax.swing.*;

//###############################################################################

//Room temp
final static float TEMPERATURE = 22;

//Config
final static int SERIAL_PORT = 0; //Serial port to listen on (-1 for disabled)

//mBed
/*
final static int SAMPLE_TIME = 4000; //uS
final static int SAMPLE_RATE = 80000; //Hz

final static int VAL_MIN = 0; //12-bit
final static int VAL_MAX = 4095; //12-bit

final static float VOLT_MIN = 0;
final static float VOLT_MAX = 3.3;
/*

//Arduino
/*
final static int SAMPLE_TIME = 4000; //uS
final static int SAMPLE_RATE = 63291; //Hz

final static int VAL_MIN = 0; //10-bit
final static int VAL_MAX = 1023; //10-bit

final static float VOLT_MIN = 0;
final static float VOLT_MAX = 5;
*/

//FPGA
final static int SAMPLE_TIME = 142858; //uS
final static int SAMPLE_RATE = 35000; //Hz

final static int VAL_MIN = 0; //10-bit
final static int VAL_MAX = 1023; //10-bit

final static float VOLT_MIN = 0;
final static float VOLT_MAX = 3.3;

//###############################################################################

final static int DISPLAY_MESSAGE_TIME = 5; //Seconds to display onscreen messages for

final static String LOG_FILE_NAME = "ultrasound_log_"; //Prefix of log file name

final static int DEFAULT_PAGE_SIZE = 250; //Samples per screen
final static int PAGE_CHANGE = 10; //Amount to inc / dec page size by per key stroke

final static boolean ENABLE_TRIGGER_BASE_LINE = false; //Enable line at trigger base (op-amp ref voltage)
final static boolean ENABLE_MIN_VALUES = true; //Enable overall min values

final static float SCALE_PRES_VERT = 0.1; //V
final static float SCALE_PRES_HORIZ = 100; //uS
final static int MARKER_WIDTH = 5;

final static float TRIGGER_NEAR_FAR_CHANGE = 1200; // Time at which far trigger is used instead of near trigger
final static float TRIGGER_NEAR_FAR_CHANGE_CHANGE = 5; // Amount to inc / dec trigger switch time by
final static float TRIGGER_BASE = 1.6; //Default trigger voltage for distance detection
final static float TRIGGER_DEFAULT_OFFSET_NEAR = 0.05; //Default amount near trigger is away from base value
final static float TRIGGER_DEFAULT_OFFSET_FAR = 0.05; //Default amount far trigger is away from base value
final static float TRIGGER_CHANGE = 0.0125; //Amount to inc / dec trigger voltage by per key stroke
final static float SPEED_OF_SOUND = ((331.3 + 0.606 * TEMPERATURE) * 100) / 1000000; //cm / uS

final static float SAMPLE_PERIOD = (1.0 / SAMPLE_RATE) * 1000000;
final static int DATA_SIZE = int(SAMPLE_TIME / SAMPLE_PERIOD);

//States
final static char WAITING = 0;
final static char SAMPLING = 1;
final static char COMPLETE = 2;
final static char ERROR = 3;

//Serial port
Serial serialPort;

//File chooser
JFileChooser fc = new JFileChooser();

//Writing files
File fFolder = null;
FileOutputStream fos = null;
PrintWriter pw = null;

//Reading files
FileInputStream fis = null;
BufferedReader br = null;

//On-screen message
String sDisplayMessage = "";
int iDisplayMessageEndTime = 0;

//Sampling state
char state = WAITING;
boolean continuous = true;

//Data samples
int dataIndex = 0;
float[] data = new float[DATA_SIZE];

//Cycle counter
int cycleCount = 0;

//Display variables
int pageSize = DEFAULT_PAGE_SIZE;
int pageNo = 0;

//Reference distance (cm) to draw line
float refDist = 0;

//Trigger to edit
boolean editFar = false;

//Trigger change time
float triggerNearFarChangeTime = 820; //TRIGGER_NEAR_FAR_CHANGE;

//Trigger levels
float triggerMinNear = 1.4; //TRIGGER_BASE - TRIGGER_DEFAULT_OFFSET_NEAR;
float triggerMaxNear = 1.6375; //TRIGGER_BASE + TRIGGER_DEFAULT_OFFSET_NEAR;
float triggerMinFar = 1.4625; //TRIGGER_BASE - TRIGGER_DEFAULT_OFFSET_FAR;
float triggerMaxFar = 1.575; //TRIGGER_BASE + TRIGGER_DEFAULT_OFFSET_FAR;

//Stats - first to exceed triggers
int peakIndexMin = -1;
int peakIndexMax = -1;

//Stats - minimum values to exceed triggers
int minPeakIndexMin = 0;
int minPeakIndexMax = 0;

//Averages
float avgPeakTimeMin = 0;
int avgPeakTimeMinCount = 0;
float avgPeakTimeMax = 0;
int avgPeakTimeMaxCount = 0;

void setup() { 
  //Set window size
  size(500, 250);

  //Make sketch resizable and set title
  if (frame != null) {
    frame.setResizable(true);
    frame.setTitle("Ultrasound!");
  }

  //Enable anti-aliasing
  smooth();

  //Set UIManager look and feel
  try { 
    UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
  } 
  catch (Exception ex) { 
    ex.printStackTrace();
    exit();
  }
  
  //Capture serial ports list
  String[] serialPorts = Serial.list();
  
  //Print serial ports list
  println(serialPorts);
  
  //Check serial enabled
  if(SERIAL_PORT != -1) {
    //Init serial port
    serialPort = new Serial(this, serialPorts[SERIAL_PORT], 115200);
  
    //Generate a serialEvent only when a newline character is received
    serialPort.bufferUntil('\n');
    
    //Display message
    displayMessage("Ready, Serial Port: " + serialPorts[SERIAL_PORT]);
  } else {
    //Display message
    displayMessage("Ready, Serial Port Disabled!");
  }
}

void displayMessage(String sMsg) {
  //Save message and set timer
  sDisplayMessage = sMsg;
  iDisplayMessageEndTime = millis() + (DISPLAY_MESSAGE_TIME * 1000);
}

void selectWrite() {
  SwingUtilities.invokeLater(new Runnable() {
    public void run() {
      //Disable multiple selections
      fc.setMultiSelectionEnabled(false);
  
      //Select only directories
      fc.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
      
      //Present dialog and store response 
      int returnVal = fc.showSaveDialog(frame);
    
      //Check return value
      if (returnVal == JFileChooser.APPROVE_OPTION) {   
        //Store selected folder
        fFolder = fc.getSelectedFile();
               
        //Open file for output
        openWrite();
      }
    }
  });
}

void openWrite() {
  //Close current file if open
  closeWrite();
  
  //Check folder selected
  if(fFolder == null) return;
  
  //Find unused file number
  File fOutput;
  int iFileNum = 0;
  while((fOutput = new File(fFolder, LOG_FILE_NAME + iFileNum++ + ".log")).exists());
  
  //Open file output stream
  try {
    fos = new FileOutputStream(fOutput);
  } catch(FileNotFoundException ex) {
    ex.printStackTrace();
    exit();
  }
  
  //Create print writer for file
  pw = new PrintWriter(new OutputStreamWriter(fos));
  
  //Display message
  displayMessage("OUTPUT :: Opened: " + fOutput.getAbsolutePath());
}

void closeWrite() {
  //Close print writer
  if(pw != null) {
    pw.close();
    pw = null;
  }
  
  //Close file output stream
  if(fos != null) {
    try {
      fos.close();
    } catch(IOException ex) {
      ex.printStackTrace();
      exit();
    }
    fos = null;
  }
  
  //Display message
  displayMessage("OUTPUT :: Closed file");
}

void writeFile(String sLine) {
  try {
    if(pw != null) {
      //Write to file
      pw.println(sLine);
      
      //Flush writer
      pw.flush();
    }
  } catch(Exception ex) {
    ex.printStackTrace();
  }
}

void openRead() {
  SwingUtilities.invokeLater(new Runnable() {
    public void run() {
      //Enable multiple selections
      fc.setMultiSelectionEnabled(true);
  
      //Select files
      fc.setFileSelectionMode(JFileChooser.FILES_ONLY);
      
      //Present dialog and store response 
      int returnVal = fc.showOpenDialog(frame);
    
      //Check return value
      if (returnVal == JFileChooser.APPROVE_OPTION) {
        //Retrieve selected file
        File[] files = fc.getSelectedFiles();
    
        for(int i = 0; i < files.length; i++) {
          //Retrieve file
          File fInput = files[i];
          
          //Open file input stream
          try {
            fis = new FileInputStream(fInput);
          } catch(FileNotFoundException ex) {
            ex.printStackTrace();
            exit();
          }
          
          //Create buffered reader for file input stream
          br = new BufferedReader(new InputStreamReader(fis));

          //Reset averages
          resetAverages();
  
          //Load one sample (or all samples in continuous mode)
          readFile();
  
          //If continuous mode selected whole file will have been processed, output stats
          if(continuous) printAverages(fInput.getName());
          
          //Display message
          displayMessage("INPUT :: Opened: " + fInput.getAbsolutePath());
        }
      }
    }
  });
}

void resetRead() {
  //Check file input stream and buffered reader available
  if(fis != null && br != null) {
    //Reset input stream
    try {
      fis.getChannel().position(0);
    } catch(IOException ex) {
      ex.printStackTrace();
      exit();
    }
    
    //Create new buffered reader
    br = new BufferedReader(new InputStreamReader(fis));
      
    //Read sample from file
    readFile();
   
    //Display message
    displayMessage("INPUT :: Reset reader"); 
  }
}

void readFile() {
  if(br != null) {
    //Change state
    state = WAITING;
    
    //Read file until state machine complete
    while (state == WAITING || state == SAMPLING) {
      //Attempt to read line from file
      String sLine = null;
      try {
        if(br.ready()) sLine = br.readLine();
      } catch(IOException ex) {
        ex.printStackTrace();
        exit();
      }
      
      if (sLine != null) {
        //Process line, don't bother logging to file
        processSample(sLine, false);
      } 
      else {
        //Change state
        state = ERROR;
      }
    }
  }
}

void serialEvent(Serial port) {
  //Process sample from serial port, logging it to file one a complete sample is recieved (if logging enabled)
  processSample(port.readStringUntil('\n'), true);
}

void processSample(String sInput, boolean log) {
  if (sInput != null) {
    //Remove whitespace (new line) character
    sInput = trim(sInput);

    //Check input against start / end
    if (match(sInput, "START") != null) {
      //Check state
      if (state == WAITING) {
        //Reset index
        dataIndex = 0;

        //Reset peak index and peak
        peakIndexMin = -1;
        peakIndexMax = -1;

        //Change state
        state = SAMPLING;
      }
    } 
    else if (match(sInput, "END") != null) {
      //Check state
      if (state == SAMPLING) {
        //Check index
        if (dataIndex == data.length) {         
          //Update first min time
          if (peakIndexMin != -1) {
            //Update time
            float peakTimeMin = sampleToTime(peakIndexMin) / 2;
            
            //Update min time
            if(minPeakIndexMin == 0 || peakTimeMin< sampleToTime(minPeakIndexMin)) {
              minPeakIndexMin = peakIndexMin;
            }

            //Update average
            avgPeakTimeMin += peakTimeMin;
            avgPeakTimeMinCount++;
          }

          //Update first max time
          if (peakIndexMax != -1) {
            //Update time
            float peakTimeMax = sampleToTime(peakIndexMax) / 2;
            
            //Update max time
            if(minPeakIndexMax == 0 || peakTimeMax < sampleToTime(minPeakIndexMax)) {
              minPeakIndexMax = peakIndexMax;
            }

            //Update average
            avgPeakTimeMax += peakTimeMax;
            avgPeakTimeMaxCount++;
          }
          
          //Log sample
          if(log) {
            writeFile("START");
            for(int i = 0; i < data.length; i++) writeFile(String.valueOf(data[i]));
            writeFile("END");
          }
          
          //Update cycle count
          cycleCount++;

          //Change state                   
          state = (continuous ? WAITING : COMPLETE);
        } 
        else {
          //Change state
          state = ERROR;
        }
      }
    } 
    else if (state == SAMPLING) {
      //Convert string to float
      float fInput = float(sInput);

      //Check index
      if (dataIndex >= data.length) {
        //Change state
        state = ERROR;

        //Return early
        return;
      }

      //Store value
      data[dataIndex++] = fInput;

      //Map value to voltage
      float fVoltage = map(fInput, VAL_MIN, VAL_MAX, VOLT_MIN, VOLT_MAX);
      
      //Map sample index to sample time
      float fSampleTime = sampleToTime(dataIndex);

      //Check lower trigger
      if ((fSampleTime < triggerNearFarChangeTime && fVoltage < triggerMinNear) || (fSampleTime >= triggerNearFarChangeTime && fVoltage < triggerMinFar)) {
        //Set first min index the first time trigger exceeded
        if(peakIndexMin == -1) peakIndexMin = dataIndex;
      }

      //Check upper trigger
      if ((fSampleTime < triggerNearFarChangeTime && fVoltage > triggerMaxNear) || (fSampleTime >= triggerNearFarChangeTime && fVoltage > triggerMaxFar)) {
        //Set first max index the first time trigger exceeded
        if(peakIndexMax == -1) peakIndexMax = dataIndex;
      }
    }
  }
}

float sampleToTime(int sampleIndex) {
  return (sampleIndex * SAMPLE_PERIOD);
}

void getRefDistance() {
  SwingUtilities.invokeLater(new Runnable() {
    public void run() {       
      //Read reference distance
      String sRefDist = JOptionPane.showInputDialog(null, "Distance (cm)", "Distance Ref", JOptionPane.PLAIN_MESSAGE);
      
      //Check and parse response
      if(sRefDist != null && sRefDist != "") {
        refDist = parseFloat(sRefDist);
      } else {
        refDist = 0;
      }
    }
  });
}

void resetAverages() {
  cycleCount = 0;
  minPeakIndexMin = 0;
  minPeakIndexMax = 0;
  avgPeakTimeMin = 0;
  avgPeakTimeMinCount = 0;
  avgPeakTimeMax = 0;
  avgPeakTimeMaxCount = 0;
}

void keyPressed() { 
  if (key == CODED) {
    //Handle keycode
    switch(keyCode) {
    case KeyEvent.VK_UP:
      //Do nothing
      break;
    case KeyEvent.VK_DOWN:
      //Do nothing
      break;
    case KeyEvent.VK_LEFT:
      //Decrement page no
      if (pageNo > 0) pageNo--;
      break;
    case KeyEvent.VK_RIGHT:
      //Incremenet page no
      if (pageNo < (ceil((float) DATA_SIZE / pageSize) - 1)) pageNo++;
      break;
    }
  } 
  else {    
    //Handle character
    switch(key) {
    case 's': //Capture
      //Change state
      state = WAITING;
      break;
    case 'c': //Continuous
      //Toggle continuous mode
      continuous = !continuous;

      //Change state if entering mode
      if (continuous) state = WAITING;
      break;
    case 'a':
      //Decrement page size
      pageSize -= PAGE_CHANGE;

      //Reset page number
      pageNo = 0;
      break;
    case 'd':
      //Increment page size
      pageSize += PAGE_CHANGE;

      //Reset page number
      pageNo = 0; 
      break;
    case 'p':
      //Switch trigger to edit
      editFar = !editFar;
      break;
    case 'i':
      //Decrement trigger
      if(editFar) {
        triggerMinFar -= TRIGGER_CHANGE;
      } else {
        triggerMinNear -= TRIGGER_CHANGE;
      }

      break;
    case 'y':
      //Increment trigger
      if(editFar) {
        triggerMinFar += TRIGGER_CHANGE;
      } else {
        triggerMinNear += TRIGGER_CHANGE;
      }

      break;
    case 'o':
      //Decrement trigger
      if(editFar) {
        triggerMaxFar -= TRIGGER_CHANGE;
      } else {
        triggerMaxNear -= TRIGGER_CHANGE;
      }

      break;
    case 'u':
      //Increment trigger
      if(editFar) {
        triggerMaxFar += TRIGGER_CHANGE;
      } else {
        triggerMaxNear += TRIGGER_CHANGE;
      }

      break;
    case 'q':
      //Decrement trigger switch change time
      triggerNearFarChangeTime -= TRIGGER_NEAR_FAR_CHANGE_CHANGE;
      
      break;
    case 'e':
      //Increment trigger switch change time
      triggerNearFarChangeTime += TRIGGER_NEAR_FAR_CHANGE_CHANGE;
     
      break;
    case 'w':
      //Display trigger values
      String s = "";
      s += "Near Far Change: " + triggerNearFarChangeTime + "\n";
      s += "Min Near: " + triggerMinNear + " - Max Near: " + triggerMaxNear + "\n";
      s += "Min Far: " + triggerMinFar + " - Max Far: " + triggerMaxFar;
      displayMessage(s);
      
      break;
    case 'r':
      //Read reference distance
      getRefDistance();
      
      break;
    case 't':
      //Reset averages
      resetAverages();
      
      break;
    case 'l':     
      //Open file, automatically reset averages and provide summary if in continuous mode
      openRead();
      
      break;
    case 'x':
      //Read next sample from file
      readFile();
      
      break;
    case 'z':
      //Reset reader if file loaded
      resetRead();
      
      break;
    case 'h':
      //Select file to write to
      selectWrite();
      
      break;
    case 'j':
      //Open new log file
      openWrite();
      
      break;
    case 'k':
      //Close current log file
      closeWrite();
      
      break;
    }
  }
}

float calcAvgPeakTimeMin() {
  return (avgPeakTimeMinCount > 0 ? avgPeakTimeMin / avgPeakTimeMinCount : 0);
}

float calcAvgPeakTimeMax() {
  return (avgPeakTimeMaxCount > 0 ? avgPeakTimeMax / avgPeakTimeMaxCount : 0);
}

void printAverages(String prefix) {
  //Calculate average times
  float avgPeakMinTime = calcAvgPeakTimeMin();
  float avgPeakMaxTime = calcAvgPeakTimeMax();
  
  //Output csv string
  println(prefix + (prefix != "" ? "," : "") + avgPeakMinTime + "," + avgPeakMaxTime);
}

void draw() {  
  //Clear background
  background(255);

  //Set colours
  stroke(0);
  fill(0);
  
  //Output message if available
  if(millis() < iDisplayMessageEndTime) {
    textAlign(CENTER, TOP);
    text(sDisplayMessage, width / 2, 0);
  }

  //Generate text
  String strState = "";
  switch(state) {
  case WAITING:
    strState = "Waiting...";
    break;
  case SAMPLING:
    strState = "Sampling...";
    break;
  case COMPLETE:
    strState = "Complete!";
    break;
  case ERROR:
    strState = "Error!";
    break;
  }
  if (continuous) strState = "Continuous " + strState;
  
  //Label y position
  int tmpY = 10;

  //Set text alignment and output text
  textAlign(RIGHT, CENTER);
  text(strState, width, tmpY);
  tmpY += 20;

  //Calculate last min and max times
  float peakTimeMin = (peakIndexMin != -1 ? sampleToTime(peakIndexMin) / 2 : 0);
  float peakTimeMax = (peakIndexMin != -1 ? sampleToTime(peakIndexMax) / 2 : 0);
  
  //Calculate overall min and max times
  float minPeakTimeMin = (minPeakIndexMin != -1 ? sampleToTime(minPeakIndexMin) / 2 : 0);
  float minPeakTimeMax = (minPeakIndexMax != -1 ? sampleToTime(minPeakIndexMax) / 2 : 0);

  //Calculate average times
  float avgPeakMinTime = calcAvgPeakTimeMin();
  float avgPeakMaxTime = calcAvgPeakTimeMax();

  //Output times and distances
  text("Count: " + cycleCount, width, tmpY); tmpY += 20;
  
  text("Last Min: " + nf(peakTimeMin, 1, 2) + "uS / " + nf(peakTimeMin * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
  text("Last Max: " + nf(peakTimeMax, 1, 2) + "uS / " + nf(peakTimeMax * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;

  text("Avg Min: " + nf(avgPeakMinTime, 1, 2) + "uS / " + nf(avgPeakMinTime * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
  text("Avg Max: " + nf(avgPeakMaxTime, 1, 2) + "uS / " + nf(avgPeakMaxTime * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;

  if(ENABLE_MIN_VALUES) {
    text("Overall Min: " + nf(minPeakTimeMin, 1, 2) + "uS / " + nf(minPeakTimeMin * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
    text("Overall Min: " + nf(minPeakTimeMax, 1, 2) + "uS / " + nf(minPeakTimeMax * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;
  }

  //Output trigger levels
  float tMinNear = map(triggerMinNear, VOLT_MIN, VOLT_MAX, 0, height);
  float tMaxNear = map(triggerMaxNear, VOLT_MIN, VOLT_MAX, 0, height);
  float tMinFar = map(triggerMinFar, VOLT_MIN, VOLT_MAX, 0, height);
  float tMaxFar = map(triggerMaxFar, VOLT_MIN, VOLT_MAX, 0, height);
  float tChangeTimeNearFar = map(triggerNearFarChangeTime, 0, (SAMPLE_PERIOD * pageSize), 0, width);
  stroke(0, 0, 255);
  line(0, height - tMinNear, tChangeTimeNearFar, height - tMinNear);
  line(tChangeTimeNearFar, height - tMinFar, width, height - tMinFar);
  stroke(255, 0, 0);
  line(0, height - tMaxNear, tChangeTimeNearFar, height - tMaxNear);
  line(tChangeTimeNearFar, height - tMaxFar, width, height - tMaxFar);
  
  //Select black
  stroke(0);

  //Output center
  if(ENABLE_TRIGGER_BASE_LINE) {
    float trigBase = map(TRIGGER_BASE, VOLT_MIN, VOLT_MAX, 0, height); 
    line(0, height - trigBase, width, height - trigBase);
  }

  //Set text alignment and output horizontal scale
  textAlign(LEFT, CENTER);
  for (float i = 0; i < (SAMPLE_PERIOD * pageSize); i += SCALE_PRES_HORIZ) {
    //Calc x position
    float xPos = map(i, 0, (SAMPLE_PERIOD * pageSize), 0, width);

    //Calc value
    float val = (SAMPLE_PERIOD * pageNo * pageSize) + i;

    //Draw marker
    line(xPos, height - MARKER_WIDTH, xPos, height);

    //Draw text   
    pushMatrix();
    translate(xPos, height - MARKER_WIDTH - 2);
    rotate(-HALF_PI);
    text(nf(val, 1, 0), 0, 0);
    popMatrix();
  }

  //Set text alignment and output vertical scale
  textAlign(LEFT, CENTER);
  for (float i = 0; i < VOLT_MAX; i += SCALE_PRES_VERT) {
    //Calc y position
    float yPos = map(i, VOLT_MIN, VOLT_MAX, 0, height);

    //Draw marker
    line(0, height - yPos, 5, height - yPos);

    //Draw text
    text(nf(i, 1, 1), 6, height - yPos);
  }

  //Compute indexes and sizes
  int indexStart = (pageSize * pageNo) + 1;
  float lineWidth = ((float) width / (float) pageSize);
  
  //Output reference distance
  if(refDist > 0) {
    //Calculate corresponding time of flight
    float refDistTime = refDist / SPEED_OF_SOUND;
    
    //Check time of flight against screen width
    if(refDistTime <= (SAMPLE_PERIOD * pageSize)) {
      //Scale x position
      float xPos = map(refDistTime, 0, (SAMPLE_PERIOD * pageSize), 0, width);
      
      //Draw line
      stroke(0, 255, 0);
      line(xPos, 0, xPos, height);
    }
  }

  //Redraw data points
  for (int i = 0; i < pageSize; i++) {
    //Check index
    if ((indexStart + i + 1) == data.length) break;

    //Calculate x positions
    float x1 = i * lineWidth;
    float x2 = (i + 1) * lineWidth;

    //Scale y positions
    float y1 = map(data[indexStart + i], VAL_MIN, VAL_MAX, 0, height);
    float y2 = map(data[indexStart + i + 1], VAL_MIN, VAL_MAX, 0, height);
    
    //Select colour
    if(i == peakIndexMin && i == peakIndexMax) {
      //Set colour
      stroke(255, 0, 255);
    } else if(i == peakIndexMin) {
      //Set colour
      stroke(0, 0, 255);
    } else if(i == peakIndexMax) {
      //Set colour
      stroke(255, 0, 0);
    } else {
      //Set colour
      stroke(0);
    }

    //Draw line
    line(x1, height - y1, x2, height - y2);
  }
}
