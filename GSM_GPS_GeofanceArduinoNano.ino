#include <SoftwareSerial.h>  //library serial untuk modul gsm
#include <AltSoftSerial.h>   //library serial untuk modul gps
#include <TinyGPS++.h>       //library untuk membaca data modul GPS

//Nomor yang akan dikirimkan notif sms
const String PHONE = "+6281292135029";

//Konfigirasi pin modul gsm (GSM Module RX pin to Arduino 3, GSM Module TX pin to Arduino 2)
#define rxPin 2
#define txPin 3
SoftwareSerial SIM900(rxPin, txPin);

//Konfigurasi pin modul gps (GPS Module RX pin to Arduino 9, GPS Module TX pin to Arduino 8)
AltSoftSerial neogps;
TinyGPSPlus gps;

//Variabel timer untuk mengatur interval pengiriman notif sms
unsigned long int prevSendNotif = 0;
int interval = 1; //kirim data tiap 1 detik jika mobil keluar dari batasan

//Varianel ukuran geofance atau radius batasan yang dibuat (dalam satuan meter)
const float maxDistance = 60;

//Variabel titik awal atau titik pusat geofance (letak awal kendaraan)
float initialLatitude = -7.434986056166462;
float initialLongitude = 109.25175829414225;
float latitude = 0;
float longitude = 0;

void setup()
{
  //Inisialisasi baudrate komunikasi serial
  Serial.begin(9600); //komunikasi serial antara arduino dengan PC
  SIM900.begin(9600); //komunikasi serial antara arduino dengan gsm
  neogps.begin(9600); //komunikasi serial antara arduino dengan gps

  // Check and configurasi awal modul gsm
  SIM900.println("AT"); // Check GSM module
  delay(1000);
  SIM900.println("ATE1"); // Echo ON
  delay(1000);
  SIM900.println("AT+CPIN?"); // Check SIM ready
  delay(1000);
  SIM900.println("AT+CMGF=1"); // SMS text mode
  delay(1000);
  SIM900.println("AT+CNMI=1,1,0,0,0"); // Decides how newly arrived SMS should be handled
  delay(1000);

  //Memberikan waktu untuk GPS mendapatkan data lokasi
  unsigned long prevMillis = millis();
  while (latitude == 0 && longitude == 0) {
    getGps(latitude, longitude);
    if (millis() - prevMillis > 2000) {
      Serial.println("Menunggu data GPS");
      prevMillis = millis();
    }
  }
}

void loop()
{
  //Mendapatkan data lokasi terkini
  getGps(latitude, longitude);

  //Mengukur jarak dari terkini dari lokasi awal
  float distance = getDistance(latitude, longitude, initialLatitude, initialLongitude);

  // Print data GPS dan jarak dari pusat geofance
  Serial.print("Latitude        = "); Serial.println(latitude, 8);          //latitude terkini
  Serial.print("Longitude       = "); Serial.println(longitude, 8);         //longtitude terkini
  Serial.print("initialLatitude = "); Serial.println(initialLatitude, 8);   //latitude titik awal
  Serial.print("initialLongitude= "); Serial.println(initialLongitude, 8);  //longtitude titik awal
  Serial.print("Current Distance= "); Serial.println(distance);             //jarak antara lokasi terkini dengan lokasi awal

  //Mengirimkan notifikasi sms jika melewati batasan, tiap interval waktu yang ditentukan
  if (distance > maxDistance) {
    if (millis() - prevSendNotif > interval*1000){
      sendAlert();
      prevSendNotif = millis();
    }
  }

  //Menangani data serial yang dikirim oleh modul gsm ke serial monitor
  while (SIM900.available()) {
    Serial.println(SIM900.readString());
  }
  //Menangani data serial yang dikirmkan oleh pengguna (serial monitor) ke modul gsm
  while (Serial.available()) {
    SIM900.println(Serial.readString());
  }
}

//Fungsi untuk menghitung jarak antara titik terkini dengan titik awal
float getDistance(float flat1, float flon1, float flat2, float flon2) {
  float diflat = radians(flat2 - flat1);
  flat1 = radians(flat1);
  flat2 = radians(flat2);
  float diflon = radians((flon2) - (flon1));

  float dist_calc = (sin(diflat / 2.0) * sin(diflat / 2.0));
  float dist_calc2 = cos(flat1);
  dist_calc2 *= cos(flat2);
  dist_calc2 *= sin(diflon / 2.0);
  dist_calc2 *= sin(diflon / 2.0);
  dist_calc += dist_calc2;

  dist_calc = (2 * atan2(sqrt(dist_calc), sqrt(1.0 - dist_calc)));
  dist_calc *= 6371000.0; // Converting to meters

  return dist_calc;
}

//Fungsi untuk mendapatkan data latitude dan longtitude dari modul gps
void getGps(float& latitude, float& longitude)
{
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;) {  //menunggu data dari modul gps
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
        break;
      }
    }
  }
  if (newData) {  //jika data baru terdeteksi, ambil data latitude dan longtitude
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    newData = false;
  } 
  else {  //jika data baru tidak terdeteksi, print no gps data
    Serial.println("No GPS data is available");
    latitude = 0;
    longitude = 0;
  }
}

//Fungsi untuk mengirimkan notifikasi sms
void sendAlert()
{
  String sms_data;
  sms_data = "Peringatan! Mobil berada diluar batasan.\r";
  sms_data += "http://maps.google.com/maps?q=loc:";
  sms_data += String(latitude, 8) + "," + String(longitude, 8);  //data koordinat

  SIM900.print("AT+CMGF=1\r");
  delay(1000);
  SIM900.print("AT+CMGS=\"" + PHONE + "\"\r");
  delay(1000);
  SIM900.print(sms_data);
  Serial.println(sms_data);
  delay(100);
  SIM900.write(0x1A); // ASCII code for Ctrl-26
  delay(1000);
  Serial.println("SMS Berhasil Dikirim.");
}