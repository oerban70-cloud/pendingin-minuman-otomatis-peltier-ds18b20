/*
  Pendingin Minuman Otomatis dengan Peltier + DS18B20 — ESP32
  ==========================================================
  Cara kerja:
  Sensor suhu DS18B20 ditempel di gelas/botol minuman (atau dicelup
  dalam wadah berisi air). ESP32 membaca suhu lewat protokol OneWire
  di satu kabel data. Kalau suhu minuman di atas 10 derajat Celsius,
  MOSFET IRLZ44N menyalakan modul Peltier TEC1-12706 yang didinginkan
  sisi dinginnya menempel ke wadah minuman. Begitu suhu turun di bawah
  8 derajat, Peltier dimatikan lagi. Selisih 2 derajat ini disebut
  histeresis — supaya Peltier tidak hidup-mati terus di sekitar satu
  angka yang sama. Suhu dan status tampil di LCD 16x2.

  KEJUJURAN TEKNIS (baca dulu):
  - Peltier TEC1-12706 menarik arus besar (sampai sekitar 6 ampere di
    12 volt). JANGAN catu dari pin 5V ESP32 — pakai adaptor 12V
    terpisah yang sanggup 6 ampere atau lebih.
  - Sisi PANAS Peltier WAJIB dipasang heatsink + kipas. Tanpa itu,
    panas menumpuk, sisi dingin ikut panas, dan modul bisa rusak.
  - MOSFET yang dipakai IRLZ44N (logic-level): gate 3,3 volt dari
    ESP32 sudah cukup membukanya penuh. IRFZ44N TIDAK cocok langsung
    karena butuh tegangan gate sekitar 10 volt.
  - Peltier bersifat resistif, bukan induktif, jadi rangkaian ini
    tidak memakai dioda flyback.
  - Yang didinginkan adalah wadah/minuman yang menempel di sisi
    dingin Peltier — bukan udara ruangan. Pendinginan satu gelas
    butuh beberapa menit, bukan hitungan detik.

  Wiring:
    - DS18B20: DQ -> GPIO4 (resistor pull-up 4,7 kOhm ke 3V3),
      VCC -> 3V3, GND -> GND.
    - LCD 16x2 I2C alamat 0x27: SDA -> GPIO21, SCL -> GPIO22,
      VCC -> 5V, GND -> GND.
    - MOSFET IRLZ44N: Gate -> GPIO5 (lewat resistor 100 Ohm),
      Drain -> kaki negatif Peltier, Source -> GND.
      Resistor 10 kOhm dari Gate ke GND supaya MOSFET mati saat
      ESP32 belum jalan (pull-down).
    - Peltier TEC1-12706: kaki positif -> 12V adaptor,
      kaki negatif -> Drain MOSFET.
    - LED indikator: anoda -> GPIO2 (lewat resistor 220 Ohm),
      katoda -> GND. LED nyala saat Peltier aktif.
    - Adaptor 12V: +12V ke Peltier, GND-nya DIGABUNG dengan GND ESP32.
      ESP32 dicatu lewat USB 5V terpisah.

  Library yang dibutuhkan:
    - OneWire 2.3.8
    - DallasTemperature 4.0.6
    - LiquidCrystal_I2C 1.1.2

  Video penjelasan (60 detik): link menyusul setelah upload
  Repo: https://github.com/oerban70-cloud/pendingin-minuman-otomatis-peltier-ds18b20
*/

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

// ---------- Konfigurasi pin ----------
const uint8_t PIN_DS18B20_DATA = 4;    // DQ sensor DS18B20 (OneWire)
const uint8_t PIN_MOSFET_GATE  = 5;    // gate IRLZ44N, HIGH = Peltier nyala
const uint8_t PIN_LED_AKTIF    = 2;    // LED indikator Peltier aktif
const uint8_t PIN_SDA_LCD      = 21;   // SDA bus I2C
const uint8_t PIN_SCL_LCD      = 22;   // SCL bus I2C

// ---------- Konfigurasi LCD ----------
const uint8_t ALAMAT_LCD       = 0x27;  // alamat I2C LCD 16x2
const uint8_t KOLOM_LCD        = 16;    // lebar layar (karakter)
const uint8_t BARIS_LCD        = 2;     // jumlah baris layar

// ---------- Konfigurasi suhu dan histeresis ----------
const float   SUHU_NYALA       = 10.0;  // Peltier nyala bila suhu >= nilai ini
const float   SUHU_MATI        = 8.0;   // Peltier mati bila suhu <= nilai ini
const float   SUHU_SENSOR_LEPAS = -127.0;  // kode DallasTemperature bila sensor tidak terbaca
const uint8_t RESOLUSI_SENSOR  = 12;    // resolusi DS18B20 (bit), 0,0625 derajat per langkah
const uint8_t INDEKS_SENSOR    = 0;     // sensor pertama di bus OneWire

// ---------- Konfigurasi waktu ----------
const unsigned long INTERVAL_BACA_MS = 2000;  // baca suhu tiap 2 detik

// ---------- Objek perangkat ----------
OneWire busOneWire(PIN_DS18B20_DATA);
DallasTemperature sensorSuhu(&busOneWire);
LiquidCrystal_I2C layarLcd(ALAMAT_LCD, KOLOM_LCD, BARIS_LCD);

// ---------- Status berjalan ----------
float suhuTerakhir = 0.0;        // suhu terakhir yang berhasil dibaca
bool  sensorTerbaca = false;     // false bila sensor lepas / tidak merespons
bool  pendinginAktif = false;    // true selama Peltier menyala
unsigned long waktuBacaTerakhir = 0;  // millis() saat baca suhu terakhir

void setup() {
  Serial.begin(115200);
  pinMode(PIN_MOSFET_GATE, OUTPUT);
  pinMode(PIN_LED_AKTIF, OUTPUT);
  digitalWrite(PIN_MOSFET_GATE, LOW);
  digitalWrite(PIN_LED_AKTIF, LOW);

  // Bus I2C untuk LCD (pin SDA/SCL sesuai wiring di atas).
  Wire.begin(PIN_SDA_LCD, PIN_SCL_LCD);

  // Sensor suhu: resolusi tertinggi (12 bit) supaya langkahnya halus.
  sensorSuhu.begin();
  sensorSuhu.setResolution(RESOLUSI_SENSOR);

  // LCD: gagal init = tidak ada gunanya lanjut tanpa layar.
  layarLcd.init();
  layarLcd.backlight();
  layarLcd.clear();
  layarLcd.setCursor(0, 0);
  layarLcd.print("Pendingin Minuman");
  layarLcd.setCursor(0, 1);
  layarLcd.print("Peltier + DS18B20");
  delay(1500);

  // Baca sekali di awal supaya layar langsung berisi angka.
  bacaSuhu();
  perbaruiPendingin();
  tampilkanLcd();
}

void loop() {
  unsigned long sekarang = millis();

  // Baca suhu tiap INTERVAL_BACA_MS, tanpa delay() supaya loop
  // tetap responsif di antara pembacaan.
  if (sekarang - waktuBacaTerakhir >= INTERVAL_BACA_MS) {
    waktuBacaTerakhir = sekarang;
    bacaSuhu();
    perbaruiPendingin();
    tampilkanLcd();
  }
}

// Minta DS18B20 mengukur suhu, simpan hasilnya di suhuTerakhir.
// Kalau sensor tidak merespons, sensorTerbaca = false dan suhu
// terakhir dipertahankan (bukan diisi angka ngawur).
void bacaSuhu() {
  sensorSuhu.requestTemperatures();
  float suhu = sensorSuhu.getTempCByIndex(INDEKS_SENSOR);
  if (suhu == SUHU_SENSOR_LEPAS) {
    sensorTerbaca = false;
    Serial.println("DS18B20 tidak terbaca, cek wiring DQ + pull-up 4,7k!");
  } else {
    sensorTerbaca = true;
    suhuTerakhir = suhu;
    Serial.print("Suhu minuman: ");
    Serial.print(suhuTerakhir, 1);
    Serial.println(" C");
  }
}

// Histeresis: Peltier nyala hanya bila suhu naik melewati SUHU_NYALA,
// dan mati hanya bila suhu turun melewati SUHU_MATI. Di antara
// keduanya, status dipertahankan — inilah yang mencegah MOSFET
// hidup-mati terus di sekitar satu angka yang sama.
void perbaruiPendingin() {
  if (!sensorTerbaca) {
    // Sensor lepas: matikan Peltier supaya tidak mendinginkan buta.
    pendinginAktif = false;
  } else if (suhuTerakhir >= SUHU_NYALA) {
    pendinginAktif = true;
  } else if (suhuTerakhir <= SUHU_MATI) {
    pendinginAktif = false;
  }

  digitalWrite(PIN_MOSFET_GATE, pendinginAktif ? HIGH : LOW);
  digitalWrite(PIN_LED_AKTIF, pendinginAktif ? HIGH : LOW);
}

// Tampilkan suhu (satu desimal) dan status pendingin di LCD.
// Baris 1: "Suhu: 12,5 C" — baris 2: "DINGIN..." / "Dingin OK" /
// "Sensor lepas!".
void tampilkanLcd() {
  layarLcd.clear();
  layarLcd.setCursor(0, 0);
  if (sensorTerbaca) {
    layarLcd.print("Suhu: ");
    layarLcd.print(suhuTerakhir, 1);
    layarLcd.print(" C");
  } else {
    layarLcd.print("Sensor lepas!");
  }

  layarLcd.setCursor(0, 1);
  if (!sensorTerbaca) {
    layarLcd.print("Cek kabel DQ");
  } else if (pendinginAktif) {
    layarLcd.print("DINGIN... ON");
  } else {
    layarLcd.print("Dingin OK - OFF");
  }
}
