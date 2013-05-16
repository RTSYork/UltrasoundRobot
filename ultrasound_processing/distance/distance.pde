import processing.serial.*;
import java.awt.event.KeyEvent;
import java.io.*;
import javax.swing.*;

//###############################################################################

//Config
final static int SERIAL_PORT = 0; //Serial port to listen on (-1 for disabled)

//mBed
/*
final static int SENSOR_COUNT = 1;
/*

//Arduino
/*
final static int SENSOR_COUNT = 1;
*/

//FPGA
final static int SENSOR_COUNT = 10;

//###############################################################################

final static int DISPLAY_MESSAGE_TIME = 5; //Seconds to display onscreen messages for

final static String LOG_FILE_NAME = "ultrasound_log_"; //Prefix of log file name

final static boolean SENSOR_AUTOSCALE = true; //Automatically scale rather than using sensor max range
final static float SENSOR_MAX_RANGE = 300; //mm
final static float SCREEN_EDGE_OFFSET = 50; //pixels - do not allow line to get closer than this to edge

final static float SENSOR_ANGLE = TWO_PI / SENSOR_COUNT;

final static float SENSOR_ANGLE_OFFSET = HALF_PI;

final static float ROBOT_CHASSIS_RADIUS = 47.5; //mm
final static float ROBOT_WHEEL_RADIUS = 16; //mm
final static float ROBOT_WHEEL_DEPTH = 6; //mm
final static float ROBOT_WHEEL_OFFSET = 10; //mm

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
int[] data = {120, 113, 050, 134, 200, 120, 075, 070, 022, 160}; //new int[SENSOR_COUNT];

//Cycle counter
int cycleCount = 0;

void setup() { 
  //Set window size
  size(500, 500);

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
      //Select files
      fc.setFileSelectionMode(JFileChooser.FILES_ONLY);
      
      //Present dialog and store response 
      int returnVal = fc.showOpenDialog(frame);
    
      //Check return value
      if (returnVal == JFileChooser.APPROVE_OPTION) {
        //Retrieve selected file
        File fInput = fc.getSelectedFile();
        
        //Open file input stream
        try {
          fis = new FileInputStream(fInput);
        } catch(FileNotFoundException ex) {
          ex.printStackTrace();
          exit();
        }
        
        //Create buffered reader for file input stream
        br = new BufferedReader(new InputStreamReader(fis));

        //Load one sample (or all samples in continuous mode)
        readFile();
        
        //Display message
        displayMessage("INPUT :: Opened: " + fInput.getAbsolutePath());
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

        //Change state
        state = SAMPLING;
      }
    } 
    else if (match(sInput, "END") != null) {
      //Check state
      if (state == SAMPLING) {
        //Check index
        if (dataIndex == data.length) {         
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
      int fInput = int(sInput);

      //Check index
      if (dataIndex >= data.length) {
        //Change state
        state = ERROR;

        //Return early
        return;
      }

      //Store value
      data[dataIndex++] = fInput;
    }
  }
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
      //Do nothing
      break;
    case KeyEvent.VK_RIGHT:
      //Do nothing
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

  //Output times and distances
  text("Count: " + cycleCount, width, tmpY); tmpY += 20;
  
  //Disable filling
  noFill();
  
/*
final static float SENSOR_MAX_RANGE = 300; //mm

final static float SENSOR_ANGLE = TWO_PI / SENSOR_COUNT;

final static float ROBOT_CHASSIS_RADIUS = 47.5; //mm
final static float ROBOT_WHEEL_RADIUS = 16; //mm
final static float ROBOT_WHEEL_DEPTH = 6; //mm
*/

  //Compute smallest dimension
  float smallestDim = min(width - SCREEN_EDGE_OFFSET, height - SCREEN_EDGE_OFFSET);
  
  //Compute scale
  float scale = smallestDim / (2 * (ROBOT_CHASSIS_RADIUS + (SENSOR_AUTOSCALE ? max(data) : SENSOR_MAX_RANGE)));
  
  //Compute center point
  float cenX = width / 2;
  float cenY = height / 2;

  //Compute scale dimensions
  float scaleRobotChassisRadius = ROBOT_CHASSIS_RADIUS * scale;
  float scaleWheelRad = ROBOT_WHEEL_RADIUS * scale;
  float scaleWheelDepth = ROBOT_WHEEL_DEPTH * scale;
  float scaleWheelOffset = ROBOT_WHEEL_OFFSET * scale;
  
  //Draw robot chassis
  ellipse(cenX, cenY, 2 * scaleRobotChassisRadius, 2 * scaleRobotChassisRadius);

  //Draw wheels
  rect(cenX - scaleRobotChassisRadius + scaleWheelOffset, cenY - scaleWheelRad, scaleWheelDepth, scaleWheelRad * 2);
  rect(cenX + scaleRobotChassisRadius - scaleWheelDepth - scaleWheelOffset, cenY - scaleWheelRad, scaleWheelDepth, scaleWheelRad * 2);
  
  //Set text alignment
  textAlign(CENTER, CENTER);
  
  //Draw distances
  for(int i = 0; i < data.length; i++) {
    //Compute arc radius for current and next distances
    float arcRadius = data[i] * scale + scaleRobotChassisRadius;
    float arcRadiusNext = data[(i + 1) % data.length] * scale + scaleRobotChassisRadius;
    
    //Compute arc start and end angle
    float arcAngleStart = i * SENSOR_ANGLE + SENSOR_ANGLE_OFFSET;
    float arcAngleEnd = (i + 1) * SENSOR_ANGLE + SENSOR_ANGLE_OFFSET;

    //Draw arc
    arc(cenX, cenY, 2 * arcRadius, 2 * arcRadius, arcAngleStart, arcAngleEnd);

    //Compute line end point
    float lineStartX = cenX + (cos(arcAngleEnd) * scaleRobotChassisRadius);
    float lineStartY = cenY + (sin(arcAngleEnd) * scaleRobotChassisRadius);
    float lineEndX = cenX + (cos(arcAngleEnd) * max(arcRadius, arcRadiusNext));
    float lineEndY = cenY + (sin(arcAngleEnd) * max(arcRadius, arcRadiusNext));
    
    //Draw line
    dottedLine(lineStartX, lineStartY, lineEndX, lineEndY, 5, 0.5, #616D7E);
    
    //Compute text position
    float textX = cenX + (cos(arcAngleStart + ((arcAngleEnd - arcAngleStart) / 2)) * arcRadius);
    float textY = cenY + (sin(arcAngleStart + ((arcAngleEnd - arcAngleStart) / 2)) * arcRadius);
    
    //Draw text
    text(str(data[i]) + "mm", textX, textY);
  }
}

void dottedLine(float x1, float y1, float x2, float y2, float sep, float rad, color c) {
  //Work out how many dots to use based on distance and separation
  float dots = dist(x1, y1, x2, y2) / sep;

  //Save and modify style
  pushStyle();
  fill(c);
  noStroke();

  //Draw series of cicles between points
  for(int i = 0; i <= dots; i++) {
    float x = lerp(x1, x2, i/dots);
    float y = lerp(y1, y2, i/dots);
    ellipse(x, y, 2 * rad, 2 * rad);
  }
  
  //Restore style
  popStyle();
}
