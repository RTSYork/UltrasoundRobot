import processing.serial.*;
import java.awt.event.KeyEvent;
import java.io.*;
import javax.swing.*;

//###############################################################################

//Config
final static int SERIAL_PORT = 0; //Serial port to listen on (-1 for disabled)

//mBed
final static int SAMPLE_TIME = 4000; //uS
final static int SAMPLE_RATE = 80000; //Hz

final static int VAL_MIN = 0; //12-bit
final static int VAL_MAX = 4095; //12-bit

final static float VOLT_MIN = 0;
final static float VOLT_MAX = 3.3;

//Arduino
/*
final static int SAMPLE_TIME = 4000; //uS
final static int SAMPLE_RATE = 63291; //Hz

final static int VAL_MIN = 0; //10-bit
final static int VAL_MAX = 1023; //10-bit

final static float VOLT_MIN = 0;
final static float VOLT_MAX = 5;
*/

//###############################################################################

final static int DISPLAY_MESSAGE_TIME = 5; //Seconds to display onscreen messages for

final static String LOG_FILE_NAME = "ultrasound_log_"; //Prefix of log file name

final static int DEFAULT_PAGE_SIZE = 250; //Samples per screen
final static int PAGE_CHANGE = 10; //Amount to inc / dec page size by per key stroke

final static boolean ENABLE_TRIGGER_BASE_LINE = false; //Enable line at trigger base (op-amp ref voltage)

final static float SCALE_PRES_VERT = 0.1; //V
final static float SCALE_PRES_HORIZ = 100; //uS
final static int MARKER_WIDTH = 5;

final static float TRIGGER_BASE = 1.6; //Default trigger voltage for distance detection
final static float TRIGGER_DEFAULT_OFFSET = 0.05; //Default amount trigger is away from base value
final static float TRIGGER_CHANGE = 0.025; //Amount to inc / dec trigger voltage by per key stroke
final static float SPEED_OF_SOUND = 0.03432; //cm / uS

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

//Display variables
int pageSize = DEFAULT_PAGE_SIZE;
int pageNo = 0;

//Reference distance (cm) to draw line
float refDist = 0;

//Trigger levels
float triggerMin = TRIGGER_BASE - TRIGGER_DEFAULT_OFFSET;
float triggerMax = TRIGGER_BASE + TRIGGER_DEFAULT_OFFSET;

//Stats - first to exceed triggers
int peakIndexFirstMin = -1;
int peakIndexFirstMax = -1;
float peakTimeFirstMin = 0;
float peakTimeFirstMax = 0;

//Stats - minimum and maxinimum values exceeding triggers
int peakIndexMin = -1;
int peakIndexMax = -1;
float peakValMin = Float.MAX_VALUE;
float peakValMax = Float.MIN_VALUE;
float peakTimeMin = 0;
float peakTimeMax = 0;

//Averages
float avgPeakTimeMin = 0;
int avgPeakTimeMinCount = 0;
float avgPeakTimeMax = 0;
int avgPeakTimeMaxCount = 0;
float avgPeakTimeFirstMin = 0;
int avgPeakTimeFirstMinCount = 0;
float avgPeakTimeFirstMax = 0;
int avgPeakTimeFirstMaxCount = 0;

void setup() { 
  //Set window size
  size(1300, 1000);

  //Make sketch resizable
  if (frame != null) frame.setResizable(true);

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
        peakIndexFirstMin = -1;
        peakIndexFirstMax = -1;
        peakIndexMin = -1;
        peakIndexMax = -1;
        peakValMin = Float.MAX_VALUE;
        peakValMax = Float.MIN_VALUE;

        //Change state
        state = SAMPLING;
      }
    } 
    else if (match(sInput, "END") != null) {
      //Check state
      if (state == SAMPLING) {
        //Check index
        if (dataIndex == data.length) {
          //Update min peak time
          if (peakIndexMin != -1) {
            //Update time
            peakTimeMin = (peakIndexMin * SAMPLE_PERIOD) / 2;
            
            //Update average
            avgPeakTimeMin += peakTimeMin;
            avgPeakTimeMinCount++;
          } 
          else {
            peakTimeMin = 0;
          }

          //Update max peak time
          if (peakIndexMax != -1) {
            //Update time
            peakTimeMax = (peakIndexMax * SAMPLE_PERIOD) / 2;
            
            //Update average
            avgPeakTimeMax += peakTimeMax;
            avgPeakTimeMaxCount++;
          } 
          else {
            peakTimeMax = 0;
          }
          
          //Update first min time
          if (peakIndexFirstMin != -1) {
            //Update time
            peakTimeFirstMin = (peakIndexFirstMin * SAMPLE_PERIOD) / 2;
            
            //Update average
            avgPeakTimeFirstMin += peakTimeFirstMin;
            avgPeakTimeFirstMinCount++;

          } 
          else {
            peakTimeFirstMin = 0;
          }

          //Update first max time
          if (peakIndexFirstMax != -1) {
            //Update time
            peakTimeFirstMax = (peakIndexFirstMax * SAMPLE_PERIOD) / 2;
            
            //Update average
            avgPeakTimeFirstMax += peakTimeFirstMax;
            avgPeakTimeFirstMaxCount++;
          } 
          else {
            peakTimeFirstMax = 0;
          }
          
          //Log sample
          if(log) {
            writeFile("START");
            for(int i = 0; i < data.length; i++) writeFile(String.valueOf(data[i]));
            writeFile("END");
          }

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

      //Check lower trigger
      if (fVoltage < triggerMin) {
        //Set first min index the first time trigger exceeded
        if(peakIndexFirstMin == -1) peakIndexFirstMin = dataIndex;
        
        //Update min index if minimum voltage exceeded
        if(fVoltage < peakValMin) {
          peakIndexMin = dataIndex;
          peakValMin = fVoltage;
        }
      }

      //Check upper trigger
      if (fVoltage > triggerMax) {
        //Set first max index the first time trigger exceeded
        if(peakIndexFirstMax == -1) peakIndexFirstMax = dataIndex;
        
        //Update max index if maximum voltage exceeded
        if(fVoltage > peakValMax) {
          peakIndexMax = dataIndex;
          peakValMax = fVoltage;
        }
      }
    }
  }
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
  avgPeakTimeMin = 0;
  avgPeakTimeMinCount = 0;
  avgPeakTimeMax = 0;
  avgPeakTimeMaxCount = 0;
  avgPeakTimeFirstMin = 0;
  avgPeakTimeFirstMinCount = 0;
  avgPeakTimeFirstMax = 0;
  avgPeakTimeFirstMaxCount = 0;
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
    case 'i':
      //Decrement trigger
      triggerMin -= TRIGGER_CHANGE;

      break;
    case 'y':
      //Increment trigger
      triggerMin += TRIGGER_CHANGE;

      break;
    case 'o':
      //Decrement trigger
      triggerMax -= TRIGGER_CHANGE;

      break;
    case 'u':
      //Increment trigger
      triggerMax += TRIGGER_CHANGE;

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

float calcAvgPeakTimeFirstMin() {
  return (avgPeakTimeFirstMinCount > 0 ? avgPeakTimeFirstMin / avgPeakTimeFirstMinCount : 0);
}

float calcAvgPeakTimeFirstMax() {
  return (avgPeakTimeFirstMaxCount > 0 ? avgPeakTimeFirstMax / avgPeakTimeFirstMaxCount : 0);
}

void printAverages(String prefix) {
  //Calculate average times
  float avgPeakMinTime = calcAvgPeakTimeMin();
  float avgPeakMaxTime = calcAvgPeakTimeMax();
  float avgPeakFirstMinTime = calcAvgPeakTimeFirstMin();
  float avgPeakFirstMaxTime = calcAvgPeakTimeFirstMax();
  
  //Output csv string
  println(prefix + (prefix != "" ? "," : "") + avgPeakMinTime + "," + avgPeakMaxTime + "," + avgPeakFirstMinTime + "," + avgPeakFirstMaxTime);
}

void draw() {  
  //Clear background
  background(255);

  //Set colours
  stroke(0);
  fill(0);
  
  //Output message if available
  if(millis() < iDisplayMessageEndTime) {
    textAlign(CENTER, CENTER);
    text(sDisplayMessage, width / 2, 10);
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

  //Calculate average times
  float avgPeakMinTime = calcAvgPeakTimeMin();
  float avgPeakMaxTime = calcAvgPeakTimeMax();
  float avgPeakFirstMinTime = calcAvgPeakTimeFirstMin();
  float avgPeakFirstMaxTime = calcAvgPeakTimeFirstMax();

  //Output times and distances
  text("Min: " + nf(peakTimeFirstMin, 1, 2) + "uS / " + nf(peakTimeFirstMin * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
  text("Max: " + nf(peakTimeFirstMax, 1, 2) + "uS / " + nf(peakTimeFirstMax * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;

  text("Min Avg: " + nf(avgPeakFirstMinTime, 1, 2) + "uS / " + nf(avgPeakFirstMinTime * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
  text("Max Avg: " + nf(avgPeakFirstMaxTime, 1, 2) + "uS / " + nf(avgPeakFirstMaxTime * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;

  /*  
  text("Min Val: " + nf(peakTimeMin, 1, 2) + "uS / " + nf(peakTimeMin * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
  text("Max Val: " + nf(peakTimeMax, 1, 2) + "uS / " + nf(peakTimeMax * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;
  
  text("Min Val Avg: " + nf(avgPeakMinTime, 1, 2) + "uS / " + nf(avgPeakMinTime * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 15;
  text("Max Val Avg: " + nf(avgPeakMaxTime, 1, 2) + "uS / " + nf(avgPeakMaxTime * SPEED_OF_SOUND, 1, 2) + "cm", width, tmpY); tmpY += 20;
  */

  //Output trigger levels
  float tMin = map(triggerMin, VOLT_MIN, VOLT_MAX, 0, height);
  float tMax = map(triggerMax, VOLT_MIN, VOLT_MAX, 0, height);
  stroke(0, 0, 255);
  line(0, height - tMin, width, height - tMin);
  stroke(255, 0, 0);
  line(0, height - tMax, width, height - tMax);
  
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
