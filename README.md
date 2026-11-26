# Pendingin Minuman Otomatis — ESP32 + Peltier TEC1-12706 + DS18B20

Minuman hangat bikin haus makin parah? Alat kecil ini mendinginkan
minumanmu sampai suhu target, lalu berhenti sendiri. Sensor DS18B20
membaca suhu minuman, ESP32 mengatur modul Peltier TEC1-12706 lewat
MOSFET IRLZ44N, dan suhu + status tampil di LCD 16x2. Begitu suhu
turun di bawah 8 derajat, Peltier dimatikan — lalu nyala lagi kalau
suhu naik melewati 10 derajat (histeresis 2 derajat, supaya MOSFET
tidak hidup-mati terus).

Video penjelasan (60 detik):
link menyusul setelah upload

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Peltier TEC1-12706 | Modul pendingin (sisi dingin ke minuman, sisi panas ke heatsink + kipas) |
| DS18B20 | Sensor suhu minuman (OneWire, tahan air) |
| MOSFET IRLZ44N | Saklar daya Peltier (N-channel, logic-level) |
| LCD 16x2 I2C 0x27 | Tampil suhu + status |
| Resistor 4,7 kOhm | Pull-up DQ DS18B20 ke 3V3 |
| Resistor 100 Ohm | Seri gate MOSFET |
| Resistor 10 kOhm | Pull-down gate MOSFET ke GND |
| Adaptor 12V, minimal 6A | Catu Peltier (terpisah, JANGAN dari ESP32) |

## Wiring

| Dari | Ke |
|---|---|
| DS18B20 DQ | ESP32 GPIO4 (pull-up 4,7 kOhm ke 3V3) |
| DS18B20 VCC | ESP32 3V3 |
| LCD SDA / SCL | ESP32 GPIO21 / GPIO22 |
| LCD VCC | ESP32 VIN 5V |
| MOSFET Gate | ESP32 GPIO5 (lewat resistor 100 Ohm, pull-down 10 kOhm ke GND) |
| MOSFET Drain | Peltier kaki negatif |
| MOSFET Source | GND |
| Peltier kaki positif | +12V adaptor |
| Adaptor GND | Digabung ke GND ESP32 |
| Semua GND | Digabung |

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

1. DS18B20 mengukur suhu minuman sampai sepersepuluh derajat, dibaca
   ESP32 tiap 2 detik lewat satu kabel OneWire.
2. Kalau suhu naik melewati 10 derajat, ESP32 menyalakan MOSFET —
   Peltier dialiri 12 volt dan sisi dinginnya mendinginkan minuman.
3. Kalau suhu turun sampai 8 derajat, MOSFET dimatikan. Di antara 8
   dan 10 derajat, status terakhir dipertahankan (histeresis).
4. LCD menampilkan suhu satu desimal dan status: "DINGIN... ON" saat
   Peltier aktif, "Dingin OK - OFF" saat suhu sudah tercapai.
5. Kalau sensor lepas atau tidak terbaca, Peltier dimatikan dan LCD
   menampilkan "Sensor lepas!" — alat tidak pernah mendinginkan buta.

## Perhatian — baca sebelum merangkai

- Peltier TEC1-12706 menarik arus sampai sekitar 6 ampere di 12 volt.
  Pakai adaptor 12V yang sanggup 6 ampere atau lebih, dan JANGAN catu
  dari pin 5V ESP32.
- Sisi PANAS Peltier WAJIB dipasang heatsink + kipas. Tanpa itu, panas
  menumpuk, sisi dingin ikut panas, dan modul bisa rusak.
- MOSFET yang dipakai IRLZ44N (logic-level): gate 3,3 volt dari ESP32
  sudah cukup membukanya penuh. IRFZ44N TIDAK cocok langsung karena
  butuh tegangan gate sekitar 10 volt.
- Peltier bersifat resistif, bukan induktif, jadi rangkaian ini tidak
  memakai dioda flyback.
- Yang didinginkan adalah wadah/minuman yang menempel di sisi dingin
  Peltier — bukan udara ruangan. Mendinginkan satu gelas butuh
  beberapa menit, bukan hitungan detik.

## Konfigurasi (di awal sketch)

- `SUHU_NYALA` (10.0) — Peltier nyala bila suhu mencapai nilai ini.
- `SUHU_MATI` (8.0) — Peltier mati bila suhu turun ke nilai ini.
- `PIN_DS18B20_DATA` (4) / `PIN_MOSFET_GATE` (5) — pin sensor dan gate.
- `PIN_SDA_LCD` (21) / `PIN_SCL_LCD` (22) — pin bus I2C LCD.
- `ALAMAT_LCD` (0x27) — alamat I2C LCD. Kalau LCD tidak tampil, coba
  0x3F.
- `INTERVAL_BACA_MS` (2000) — seberapa sering suhu dibaca.

## Library yang dibutuhkan

| Library | Versi | Keterangan |
|---|---|---|
| OneWire | 2.3.8 | Bus OneWire DS18B20 |
| DallasTemperature | 4.0.6 | Sensor DS18B20 |
| LiquidCrystal_I2C | 1.1.2 | LCD 16x2 I2C |

Semua library di atas juga tersedia sebagai ZIP di folder [`libraries/`](libraries/)
— install lewat Arduino IDE: Sketch → Include Library → Add .ZIP Library.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 PendinginMinumanPeltier
```

Terverifikasi 26 Nov 2026: sketch 320.953 byte (24% flash),
RAM 24.680 byte (7%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
