// HomeSpan Television Service Example

// Covers all Characteristics of the Television Service that appear to
// be supported in the iOS 15 version of the Home App.  Note these Services
// are not documented by Apple and are not officially part HAP-R2.
//
// For Service::Television():
//
//    * Characteristic::Active()
//    * Characteristic::ConfiguredName()
//    * Characteristic::ActiveIdentifier()
//    * Characteristic::RemoteKey()
//    * Characteristic::PowerModeSelection()
//
// For Service::InputSource():
//
//    * Characteristic::ConfiguredName()
//    * Characteristic::ConfiguredNameStatic()        // a HomeSpan-specific variation of ConfiguredName()
//    * Characteristic::Identifier()
//    * Characteristic::IsConfigured()
//    * Characteristic::CurrentVisibilityState()
//    * Characteristic::TargetVisibilityState()

// NOTE: This example is only designed to demonstrate how Television Services and Characteristics
// appear in the Home App, and what they each control.  To keep things simple, actions for the
// Input Sources have NOT been implemented in the code below.  For example, the code below does not include
// any logic to update CurrentVisibilityState when the TargetVisibilityState checkboxes are clicked.

#include "HomeSpan.h"
#include "CEC_Device.h"

#define CEC_GPIO 13
#define CEC_DEVICE_TYPE CEC_Device::CDT_PLAYBACK_DEVICE
#define CEC_PHYSICAL_ADDRESS 0x3000

class HomeSpanTV;

// implement application specific CEC device
class MyCEC_Device : public CEC_Device
{
  HomeSpanTV* connectedTV;
  public:
    void SetTVDevice(HomeSpanTV* tv);
  protected:
    virtual bool LineState();
    virtual void SetLineState(bool);
    virtual void OnReady(int logicalAddress);
    virtual void OnReceiveComplete(unsigned char* buffer, int count, bool ack);
    virtual void OnTransmitComplete(unsigned char* buffer, int count, bool ack);
};

bool MyCEC_Device::LineState()
{
  int state = digitalRead(CEC_GPIO);
  return state != LOW;
}

void MyCEC_Device::SetLineState(bool state)
{
  if (state) {
    pinMode(CEC_GPIO, INPUT_PULLUP);
  } else {
    digitalWrite(CEC_GPIO, LOW);
    pinMode(CEC_GPIO, OUTPUT);
  }
  // give enough time for the line to settle before sampling it
  delayMicroseconds(50);
}

void MyCEC_Device::OnReady(int logicalAddress)
{
  // This is called after the logical address has been allocated

  unsigned char buf[4] = {0x84, CEC_PHYSICAL_ADDRESS >> 8, CEC_PHYSICAL_ADDRESS & 0xff, CEC_DEVICE_TYPE};

  DbgPrint("Device ready, Logical address assigned: %d\n", logicalAddress);

  TransmitFrame(0xf, buf, 4); // <Report Physical Address>
}

void MyCEC_Device::OnTransmitComplete(unsigned char* buffer, int count, bool ack)
{
  // This is called after a frame is transmitted.
  DbgPrint("Packet sent at %ld: %02X", millis(), buffer[0]);
  for (int i = 1; i < count; i++)
    DbgPrint(":%02X", buffer[i]);
  if (!ack)
    DbgPrint(" NAK");
  DbgPrint("\n");
}

MyCEC_Device device;

struct HomeSpanTVSpeaker : Service::TelevisionSpeaker {
    SpanCharacteristic *volume = new Characteristic::VolumeSelector();
    SpanCharacteristic *volumeType = new Characteristic::VolumeControlType(3);

    HomeSpanTVSpeaker(const char *name) : Service::TelevisionSpeaker() {
      new Characteristic::ConfiguredName(name);
      Serial.printf("Configured Speaker: %s\n", name);
    }

    boolean update() override {
      if (volume->updated()) {
        if (volume->getNewVal() == 0) {
          Serial.printf("Volume Up");
          device.TransmitFrame(0x40, (unsigned char*)"\x46", 1);
        } else {
          Serial.printf("Volume Down");
          device.TransmitFrame(0x40, (unsigned char*)"\x8f", 1);
        }
      }

      if (volumeType->updated()) {
        Serial.printf("Updated volume type\n");
      }

      return (true);
    }
};

struct HomeSpanTV : Service::Television {

  SpanCharacteristic *active = new Characteristic::Active(0);                     // TV On/Off (set to Off at start-up)
  SpanCharacteristic *activeID = new Characteristic::ActiveIdentifier(3);         // Sets HDMI 3 on start-up
  SpanCharacteristic *remoteKey = new Characteristic::RemoteKey();                // Used to receive button presses from the Remote Control widget
  SpanCharacteristic *settingsKey = new Characteristic::PowerModeSelection();     // Adds "View TV Setting" option to Selection Screen

  HomeSpanTV(const char *name) : Service::Television() {
    new Characteristic::ConfiguredName(name);             // Name of TV
    Serial.printf("Configured TV: %s\n", name);
  }

  boolean update() override {

    if (active->updated()) {
      Serial.printf("Set TV Power to: %s\n", active->getNewVal() ? "ON" : "OFF");
      if (active->getNewVal()) {
        device.TransmitFrame(0x0, (unsigned char*)"\x0d", 1);
      } else {
        device.TransmitFrame(0x0F, (unsigned char*)"\x36", 1);
      }
    }

    if (activeID->updated()) {
      Serial.printf("Set Input Source to HDMI-%d\n", activeID->getNewVal());
    }

    if (settingsKey->updated()) {
      Serial.printf("Received request to \"View TV Settings\"\n");
    }

    if (remoteKey->updated()) {
      Serial.printf("Remote Control key pressed: ");
      switch (remoteKey->getNewVal()) {
        case 4:
          Serial.printf("UP ARROW\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x01", 2);
          break;
          
        case 5:
          Serial.printf("DOWN ARROW\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x02", 2);
          break;
          
        case 6:
          Serial.printf("LEFT ARROW\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x03", 2);
          break;
        case 7:
          Serial.printf("RIGHT ARROW\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x04", 2);
          break;
        case 8:
          Serial.printf("SELECT\n");
          //device.TransmitFrame(0x48, (unsigned char*)"\x44\x00", 2);
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x44", 2);
          break;
        case 9:
          Serial.printf("BACK\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x09", 2);
          break;
        case 11:
          Serial.printf("PLAY/PAUSE\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x44", 2);
          break;
        case 15:
          Serial.printf("INFO\n");
          device.TransmitFrame(0x48, (unsigned char*)"\x44\x35", 2);
          break;
        default:
          Serial.print("UNKNOWN KEY\n");
      }
    }

    return (true);
  }
};

void MyCEC_Device::SetTVDevice(HomeSpanTV* tv) {
  connectedTV = tv;
  //tv::active->setVal(true);
};

void MyCEC_Device::OnReceiveComplete(unsigned char* buffer, int count, bool ack)
{
  // This is called when a frame is received.  To transmit
  // a frame call TransmitFrame.  To receive all frames, even
  // those not addressed to this device, set Promiscuous to true.
  DbgPrint("Packet received at %ld: %02X", millis(), buffer[0]);
  for (int i = 1; i < count; i++)
    DbgPrint(":%02X", buffer[i]);
  if (!ack)
    DbgPrint(" NAK");
  DbgPrint("\n");

  // Ignore messages not sent to us
  if ((buffer[0] & 0xf) != LogicalAddress())
    return;

  // No command received?
  if (count < 1)
    return;

  DbgPrint("This one's for us...\n");
  switch (buffer[1]) {
    case 0x83: { // <Give Physical Address>
        unsigned char buf[4] = {0x84, CEC_PHYSICAL_ADDRESS >> 8, CEC_PHYSICAL_ADDRESS & 0xff, CEC_DEVICE_TYPE};
        TransmitFrame(0xf, buf, 4); // <Report Physical Address>
        break;
      }
    case 0x8c: // <Give Device Vendor ID>
      TransmitFrame(0xf, (unsigned char*)"\x87\x01\x23\x45", 4); // <Device Vendor ID>
      break;
    case 0x46: // Give OSD Name
      Serial.println("Give OSD name...\n");
      TransmitFrame(0x4f, (unsigned char*)"\x47\x52\x44\x4d", 4); //4F:47:52:44:4D
      break;
  }

  if (buffer[0] == 0x04 && buffer[1] == 0x90) {
    DbgPrint("Hello, TV...\n");
    if (buffer[2] == 0x00) {
      connectedTV->active->setVal(true);
    }
  }
}

///////////////////////////////

void setup() {

  Serial.begin(115200);

  homeSpan.enableOTA();
  homeSpan.begin(Category::Television, "HomeSpan Television");

  device.Initialize(CEC_PHYSICAL_ADDRESS, CEC_DEVICE_TYPE, true);

  SPAN_ACCESSORY();

  // Below we define 10 different InputSource Services using different combinations
  // of Characteristics to demonstrate how they interact and appear to the user in the Home App

  SpanService *hdmi1 = new Service::InputSource();    // Source included in Selection List, but excluded from Settings Screen
  new Characteristic::ConfiguredName("HDMI 1");
  new Characteristic::Identifier(1);

  SpanService *hdmi2 = new Service::InputSource();
  new Characteristic::ConfiguredName("Chromecast");
  new Characteristic::Identifier(2);
  new Characteristic::IsConfigured(1);              // Source excluded from both the Selection List and the Settings Screen

  SpanService *hdmi3 = new Service::InputSource();
  new Characteristic::ConfiguredName("HDMI-CEC");
  new Characteristic::Identifier(3);
  new Characteristic::IsConfigured(0);              // Source included in both the Selection List and the Settings Screen

  SpanService *speaker = new HomeSpanTVSpeaker("My Speaker");
  new Characteristic::VolumeSelector();
  new Characteristic::VolumeControlType(3);

  HomeSpanTV* tv = (new HomeSpanTV("Test TV"));                         // Define a Television Service.  Must link in InputSources!
  tv->addLink(hdmi1)
  ->addLink(hdmi2)
  ->addLink(hdmi3)
  ->addLink(speaker)
  ;

  device.SetTVDevice(tv);

  pinMode(CEC_GPIO, INPUT_PULLUP);
  device.Initialize(CEC_PHYSICAL_ADDRESS, CEC_DEVICE_TYPE, true); // Promiscuous mode
  homeSpan.autoPoll();
}

///////////////////////////////

void loop() {
  //homeSpan.poll();
  device.Run();
}
