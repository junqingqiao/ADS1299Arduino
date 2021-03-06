
#include <SPI.h>
#include "wiring_private.h"
#include "ads1299wifi.h"
#include <WiFi101.h>
#include <WiFiUdp.h>


SPIClass mySPI (&sercom1, 12, 13, 11, SPI_PAD_0_SCK_1, SERCOM_RX_PAD_3);

const int pCS = 10; //chip select pin
const int pDRDY = 6; //data ready pin
const int pCLKSEL = 9;
const int LED = 1 ;
//Over clocked spi of the ADS1299 to 40M Hz
const int SPI_CLK = 20000000;

boolean deviceIDReturned = false;
boolean continuousRead = false ;
boolean startRead = false ;

int ch[8];
int cnt = 0; 
String spiData;


//For WIFI
int status = WL_IDLE_STATUS;
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
unsigned int localPort = 8899;      // local port to listen on
String sendingString;
char sendingBuf[10000];
int sendingBufferSize = 0;

WiFiUDP Udp;


void drdyIRS()
{
    if (!continuousRead)
    {
        //Serial.println("Continuous Read");
        continuousRead = true ;
        digitalWrite(pCS, LOW);
        mySPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE1));
        mySPI.transfer(START);
        mySPI.transfer(RDATAC);
        digitalWrite(pCS, HIGH);
    }
    //When interrupt happends read new data from ADS1299 to buffer.
    digitalWrite(pCS, LOW);
    int j = 0 ;
    spiData = String();
    for (int i = 0; i < 27; i++)
    { 
        byte temp = mySPI.transfer(0x00);
        if (i < 2)
        { 
            spiData += temp ;
            spiData += "," ;
        }
        else if (i == 2)
        { 
            spiData += cnt ;
            spiData += "," ;
        }
        else
        {
            ch[j] += temp << (3 - (i % 3)) * 8 ;
            if (i % 3 == 2)
            {
                ch[j] = ch[j] >> 8;
                //Serial.print(ch[j]);
                spiData += ch[j] ;
                if (j < 7)
                {
                    spiData += ",";
                }
                else
                {
                    spiData += "\n";
                }
                j++ ;
            }
        }
    }
    digitalWrite(pCS, HIGH);
    sendingString += spiData;
    cnt++ ;
    sendingBufferSize++;
}



void setup()
{   
    Serial.begin(115200);

    //start the SPI library:
    mySPI.begin();
    pinPeripheral(11, PIO_SERCOM);
    pinPeripheral(12, PIO_SERCOM);
    pinPeripheral(13, PIO_SERCOM);
    delay(3000);

    //Setup the wifi
    
    //Configure pins for Adafruit ATWINC1500 Feather
    WiFi.setPins(8,7,4,2);
    // check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD) 
    {
        Serial.println("WiFi shield not present");
        // don't continue:
        while (true);
    }
    // attempt to connect to WiFi network:
    // print the network name (SSID);
    Serial.print("Creating access point named: ");
    Serial.println(ssid);

    // Create open network. Change this line if you want to create an WEP network:
    status = WiFi.beginAP(ssid);
    if (status != WL_AP_LISTENING) 
    {
        Serial.println("Creating access point failed");
        // don't continue
        while (true);
    }


    
    Udp.begin(localPort);



    //Setup ADS1299
    Serial.flush();
    Serial.println("ADS1299");
    // initalize the  data ready and chip select pins:
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    ADS_INIT();
    digitalWrite(LED, LOW);
    delay(10);  //delay to ensure connection
    ADS_RREAD(0, 24);
    //ADS_WREG(0x03,0xE0); // Enable Internal Reference Buffer 6 -OFF , E - ON
    ADS_WREG(0x03, 0xE8); // Enable Internal Reference Buffer 6 -OFF , E - ON
    delay(50);
    ADS_WREG(CHn, 0x01); // Input Shorted
    delay(1);
    for (int i = 0; i < 10; i++)
        ADS_ReadContinuous();
    ADS_STOP();
    delay(1);
    ADS_WREG(0x01, 0x94); // Sample Rate 96 - 250 , 95 - 500, 90 - 16k
    ADS_WREG(0x02, 0xD1); // Test Signal 2Hz Square Wave
    //ADS_WREG(CHn,0x10); // Active channels
    ADS_WREG(CHn, 0x00); // Ch on Test Signals with Gain 0
    // ADS_WREG(CH1,0x00);
    //ADS_WREG(0x15,0x20);//SRB1 To Negative Inputs
    ADS_RREAD(0, 24);

    

    //Setup interrupt
    attachInterrupt(digitalPinToInterrupt(pDRDY), drdyIRS, FALLING);
    mySPI.transfer(START);
    mySPI.transfer(RDATAC);
    Serial.println("Start Streaming");

}

void loop() 
{
        if(sendingBufferSize > 80)
        {
            //send the data
//            Serial.println(sendingString);
//            sendingString.toCharArray(sendingBuf,1000);
            sendingString.toCharArray(sendingBuf,100*sendingBufferSize);
            Udp.beginPacket("192.168.1.100", 8899);
        
            Udp.write(sendingBuf);
            Udp.endPacket();
    
            //Clean the buffer
            sendingBufferSize = 0;
            sendingString = "";
        } 

}

















void ADS_INIT()
{
  pinMode(pDRDY, INPUT);
  pinMode(pCS, OUTPUT);
  pinMode(pCLKSEL, OUTPUT);
  digitalWrite(pCLKSEL, HIGH);
  delay(1);
  digitalWrite(pCS, HIGH);
  delay(1000);  //delay to ensure connection
  digitalWrite(pCS, LOW);
  mySPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE1));
  mySPI.transfer(RESET);
  mySPI.endTransaction();
  delay(100);
  digitalWrite(pCS, HIGH);
}

void ADS_RREAD(byte r , int n) {
  if (r + n > 24)
    n = 24 - r ;
  digitalWrite(pCS, LOW);
  Serial.println("Register Data");
  mySPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE1));
  mySPI.transfer(SDATAC);
  mySPI.transfer(RREG | r); //RREG
  mySPI.transfer(n); // 24 Registers
  for (int i = 0; i < n; i++)
  { byte temp = mySPI.transfer(0x00);
    Serial.println(temp, HEX);
  }
  mySPI.endTransaction();
  digitalWrite(pCS, HIGH);
}

void ADS_WREG(byte n, byte t)
{ if (n == 0 || n == 18 || n == 19)
    Serial.println("Error: Read-Only Register");
  else if (n == 0xFF)
  { digitalWrite(pCS, LOW);
    mySPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE1));
    mySPI.transfer(SDATAC);
    for (int i = 5; i < 13; i++)
    { mySPI.transfer(WREG | i); //RREG
      mySPI.transfer(0x00);
      mySPI.transfer(t);
    }
    mySPI.endTransaction();
    digitalWrite(pCS, HIGH);
    Serial.println("Written All Channels");
  }
  else
  { digitalWrite(pCS, LOW);
    mySPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE1));
    mySPI.transfer(SDATAC);
    mySPI.transfer(WREG | n); //RREG
    mySPI.transfer(0x00); // 24 Registers
    mySPI.transfer(t);
    mySPI.endTransaction();
    digitalWrite(pCS, HIGH);
    Serial.println("Written Register");
  }
}

void ADS_ReadContinuous()
{
    if (!continuousRead)
    {
        //Serial.println("Continuous Read");
        continuousRead = true ;
        digitalWrite(pCS, LOW);
        mySPI.beginTransaction(SPISettings(SPI_CLK, MSBFIRST, SPI_MODE1));
        mySPI.transfer(START);
        mySPI.transfer(RDATAC);
        digitalWrite(pCS, HIGH);
    }
    
    while (digitalRead(pDRDY));

    
    
}


void ADS_STOP()
{ if (continuousRead)
  { digitalWrite(pCS, LOW);
    mySPI.transfer(STOP);
    mySPI.transfer(SDATAC);
    digitalWrite(pCS, HIGH);
    continuousRead = false ;
  }
}


void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
