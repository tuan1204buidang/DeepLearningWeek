#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_camera.h"

// Enter your WiFi ssid and password
const char* ssid     = "myhotspot";   //your network SSID
const char* password = "password";   //your network password

//Google Apps Script Parameters
String deploymentID = "AKfycbx5a0hyg6avjI0sBRZJsHyf1Q6wZheKZsYZLOU_8hFm5-z8SsIBDHImBCcKln3I2qLB"; //Deploy your Google Apps Script and replace the ID path.
String myScript = "/macros/s/" + deploymentID + "/exec";   
String myFoldername = "&myFoldername=ESP32-CAM"; //this is set which folder in your drive to save the images to...based on our apps script code it will create a new folder if it does not exist
String myFilename = "&myFilename=ESP32-CAM.jpg"; // base filename to save the image...based on our apps script code it will add a timestamp to the image file name for easier sorting 
String myImage = "&myFile="; //this parameter will store the base64 encoded image that we will upload

// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200); //Serial Monitor set baudrate to 115200 as well
  delay(10);
  
  WiFi.mode(WIFI_STA);
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //Try connecting to the WiFi Network
  WiFi.begin(ssid, password);  
  
  long int StartTime=millis();
  //Wait for a maximum of 10 seconds for it to connect
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    if ((StartTime+10000) < millis()) break;
  } 
  Serial.println("");
  Serial.println("STAIP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
  
  pinMode(4, OUTPUT); // Init FLASH Led
  //check if it is connected
  if (WiFi.status() != WL_CONNECTED) {
    //NOT CONNECTED TO WiFi -> Flash LED Once before Restarting esp32 -> Runs the code from the top again
    Serial.println("Reseting...");
    digitalWrite(4, HIGH);
    delay(200);
    digitalWrite(4, LOW);
    delay(200);    
        
    delay(1000);
    ESP.restart();
  }
  else {
    //WiFi Connection Good
    Serial.println("WiFi Connection Successful");
    // LED Flashes 3 times when wifi connection is good
    for (int i=0;i<3;i++) {
    digitalWrite(4, HIGH);
    delay(200);
    digitalWrite(4, LOW);
    delay(200);    
    }
  }
  //Configure the camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }else{
    Serial.println("Camera init Success");
    }
  
  //************** CHANGE YOUR CAMERA SETTINGS HERE **************
  sensor_t * s = esp_camera_sensor_get();
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_VGA);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
  // VGA is of resolution: 640x480
  //change the camera setting accordingly
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 1);          // 0 = disable , 1 = enable //flip the camera vertically so it aligns with breadboard orientation
}

void loop()
{
  SendCapturedImage();
  delay(30000); // delay is in miliseconds so 30000ms = 30000 /1000 = 30 seconds // 
}

//Captures the image -> Converts it to Base64 -> Sends Data using POST Request to Apps Script
String SendCapturedImage() {
  
  const char* myDomain = "script.google.com";
  String getAll="", getBody = "";
  
  //Capture Image
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    //If image capture failed, restart ESP32
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  

  // Connect to Google Apps Script using TCP Client to send POST Requests over
  Serial.println("Connect to " + String(myDomain));
  WiFiClientSecure client_tcp;
  client_tcp.setInsecure();   //run version 1.0.5 or above
  
  if (client_tcp.connect(myDomain, 443)) {
    Serial.println("Connection successful"); 
    
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    String imageFile = "data:image/jpeg;base64,"; //imageFile starts off with this info which will be used later by AppsScript
    //encode the image and use urlencode() function to get the final encoded image in Base64 code
    for (int i=0;i<fb->len;i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) imageFile += urlencode(String(output));
    }
    //combine the 3 parameters to form Data
    String Data = myFoldername+myFilename+myImage;
    Serial.println("ImageFile:");
    Serial.println(imageFile);
    
    client_tcp.println("POST " + myScript + " HTTP/1.1");
    client_tcp.println("Host: " + String(myDomain));
    client_tcp.println("Content-Length: " + String(Data.length()+imageFile.length()));
    client_tcp.println("Content-Type: application/x-www-form-urlencoded");
    client_tcp.println("Connection: keep-alive");
    client_tcp.println();
    Serial.println("Data:");
    Serial.println(Data);
    
    client_tcp.print(Data);
    //Usually the imageFile taken by esp32cam is of more than 20,000 characters for 640x480 res image
    //we cannot send all of the 20,000+ characters via the post request at 1 go due to limits
    //instead we will send the imagefile in chunks of 1000 char continously as shown below 
    int Index;
    for (Index = 0; Index < imageFile.length(); Index = Index+1000) {
      client_tcp.print(imageFile.substring(Index, Index+1000)); 
    }
    esp_camera_fb_return(fb);

    // ***This part is to get the output message upon the POST Request so we know if it was successful or not
    // However due to some updates w Apps Script we are unable to get the output which is fine
    int waitTime = 10000;   // set max wait time to 10 seconds
    long startTime = millis();
    boolean state = false;
    //while the max end time (startTime + waitTime) is less than the current time
    while ((startTime + waitTime) > millis())
    {
      Serial.print(".");
      delay(100);      
      while (client_tcp.available()) 
      {
          char c = client_tcp.read();
          if (state==true) getBody += String(c);        
          if (c == '\n') 
          {
            if (getAll.length()==0) state=true; 
            getAll = "";
          } 
          else if (c != '\r')
            getAll += String(c);
          startTime = millis();
       }
       if (getBody.length()>0) break;
    }
    //disconnect the connection made 
    client_tcp.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to " + String(myDomain) + " failed.";
    Serial.println("Connected to " + String(myDomain) + " failed.");
  }
  
  return getBody;
}

//this is the function in charge of encoding the image into base64
String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}
