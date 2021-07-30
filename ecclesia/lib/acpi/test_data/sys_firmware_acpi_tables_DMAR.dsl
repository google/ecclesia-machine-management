/*
 * Disassembly of APIC_test_data/sys_firmware_acpi_tables_DMAR, Wed Dec 30 23:57:39 2020
 *
 * ACPI Data Table [DMAR]
 *
 * Format: [HexOffset DecimalOffset ByteLength]  FieldName : FieldValue
 *
 * This file can be generated from the binary version through the tool //third_party/acpica:iasl.
 */

[000h 0000   4]                    Signature : "DMAR"    [DMA Remapping table]
[004h 0004   4]                 Table Length : 00000160
[008h 0008   1]                     Revision : 01
[009h 0009   1]                     Checksum : DC
[00Ah 0010   6]                       Oem ID : "ALASKA"
[010h 0016   8]                 Oem Table ID : "A M I "
[018h 0024   4]                 Oem Revision : 00000001
[01Ch 0028   4]              Asl Compiler ID : "INTL"
[020h 0032   4]        Asl Compiler Revision : 20160527

[024h 0036   1]           Host Address Width : 2D
[025h 0037   1]                        Flags : 01
[026h 0038  10]                     Reserved : 00 00 00 00 00 00 00 00 00 00

[030h 0048   2]                Subtable Type : 0000 [Hardware Unit Definition]
[032h 0050   2]                       Length : 0060

[034h 0052   1]                        Flags : 00
[035h 0053   1]                     Reserved : 00
[036h 0054   2]           PCI Segment Number : 0000
[038h 0056   8]        Register Base Address : 00000000D37FC000

[040h 0064   1]            Device Scope Type : 03 [IOAPIC Device]
[041h 0065   1]                 Entry Length : 08
[042h 0066   2]                     Reserved : 0000
[044h 0068   1]               Enumeration ID : 0F
[045h 0069   1]               PCI Bus Number : 80

[046h 0070   2]                     PCI Path : 05,04


[048h 0072   1]            Device Scope Type : 01 [PCI Endpoint Device]
[049h 0073   1]                 Entry Length : 08
[04Ah 0074   2]                     Reserved : 0000
[04Ch 0076   1]               Enumeration ID : 00
[04Dh 0077   1]               PCI Bus Number : 80

[04Eh 0078   2]                     PCI Path : 04,00


[050h 0080   1]            Device Scope Type : 01 [PCI Endpoint Device]
[051h 0081   1]                 Entry Length : 08
[052h 0082   2]                     Reserved : 0000
[054h 0084   1]               Enumeration ID : 00
[055h 0085   1]               PCI Bus Number : 80

[056h 0086   2]                     PCI Path : 04,01


[058h 0088   1]            Device Scope Type : 01 [PCI Endpoint Device]
[059h 0089   1]                 Entry Length : 08
[05Ah 0090   2]                     Reserved : 0000
[05Ch 0092   1]               Enumeration ID : 00
[05Dh 0093   1]               PCI Bus Number : 80

[05Eh 0094   2]                     PCI Path : 04,02


[060h 0096   1]            Device Scope Type : 01 [PCI Endpoint Device]
[061h 0097   1]                 Entry Length : 08
[062h 0098   2]                     Reserved : 0000
[064h 0100   1]               Enumeration ID : 00
[065h 0101   1]               PCI Bus Number : 80

[066h 0102   2]                     PCI Path : 04,03


[068h 0104   1]            Device Scope Type : 01 [PCI Endpoint Device]
[069h 0105   1]                 Entry Length : 08
[06Ah 0106   2]                     Reserved : 0000
[06Ch 0108   1]               Enumeration ID : 00
[06Dh 0109   1]               PCI Bus Number : 80

[06Eh 0110   2]                     PCI Path : 04,04


[070h 0112   1]            Device Scope Type : 01 [PCI Endpoint Device]
[071h 0113   1]                 Entry Length : 08
[072h 0114   2]                     Reserved : 0000
[074h 0116   1]               Enumeration ID : 00
[075h 0117   1]               PCI Bus Number : 80

[076h 0118   2]                     PCI Path : 04,05


[078h 0120   1]            Device Scope Type : 01 [PCI Endpoint Device]
[079h 0121   1]                 Entry Length : 08
[07Ah 0122   2]                     Reserved : 0000
[07Ch 0124   1]               Enumeration ID : 00
[07Dh 0125   1]               PCI Bus Number : 80

[07Eh 0126   2]                     PCI Path : 04,06


[080h 0128   1]            Device Scope Type : 01 [PCI Endpoint Device]
[081h 0129   1]                 Entry Length : 08
[082h 0130   2]                     Reserved : 0000
[084h 0132   1]               Enumeration ID : 00
[085h 0133   1]               PCI Bus Number : 80

[086h 0134   2]                     PCI Path : 04,07


[088h 0136   1]            Device Scope Type : 02 [PCI Bridge Device]
[089h 0137   1]                 Entry Length : 08
[08Ah 0138   2]                     Reserved : 0000
[08Ch 0140   1]               Enumeration ID : 00
[08Dh 0141   1]               PCI Bus Number : 80

[08Eh 0142   2]                     PCI Path : 00,00


[090h 0144   2]                Subtable Type : 0000 [Hardware Unit Definition]
[092h 0146   2]                       Length : 0020

[094h 0148   1]                        Flags : 00
[095h 0149   1]                     Reserved : 00
[096h 0150   2]           PCI Segment Number : 0000
[098h 0152   8]        Register Base Address : 00000000E0FFC000

[0A0h 0160   1]            Device Scope Type : 03 [IOAPIC Device]
[0A1h 0161   1]                 Entry Length : 08
[0A2h 0162   2]                     Reserved : 0000
[0A4h 0164   1]               Enumeration ID : 10
[0A5h 0165   1]               PCI Bus Number : 85

[0A6h 0166   2]                     PCI Path : 05,04


[0A8h 0168   1]            Device Scope Type : 02 [PCI Bridge Device]
[0A9h 0169   1]                 Entry Length : 08
[0AAh 0170   2]                     Reserved : 0000
[0ACh 0172   1]               Enumeration ID : 00
[0ADh 0173   1]               PCI Bus Number : 85

[0AEh 0174   2]                     PCI Path : 00,00


[0B0h 0176   2]                Subtable Type : 0000 [Hardware Unit Definition]
[0B2h 0178   2]                       Length : 0020

[0B4h 0180   1]                        Flags : 00
[0B5h 0181   1]                     Reserved : 00
[0B6h 0182   2]           PCI Segment Number : 0000
[0B8h 0184   8]        Register Base Address : 00000000EE7FC000

[0C0h 0192   1]            Device Scope Type : 03 [IOAPIC Device]
[0C1h 0193   1]                 Entry Length : 08
[0C2h 0194   2]                     Reserved : 0000
[0C4h 0196   1]               Enumeration ID : 11
[0C5h 0197   1]               PCI Bus Number : AE

[0C6h 0198   2]                     PCI Path : 05,04


[0C8h 0200   1]            Device Scope Type : 02 [PCI Bridge Device]
[0C9h 0201   1]                 Entry Length : 08
[0CAh 0202   2]                     Reserved : 0000
[0CCh 0204   1]               Enumeration ID : 00
[0CDh 0205   1]               PCI Bus Number : AE

[0CEh 0206   2]                     PCI Path : 00,00


[0D0h 0208   2]                Subtable Type : 0001 [Reserved Memory Region]
[0D2h 0210   2]                       Length : 0020

[0D4h 0212   2]                     Reserved : 0000
[0D6h 0214   2]           PCI Segment Number : 0000
[0D8h 0216   8]                 Base Address : 0000000075460000
[0E0h 0224   8]          End Address (limit) : 0000000075470FFF

[0E8h 0232   1]            Device Scope Type : 01 [PCI Endpoint Device]
[0E9h 0233   1]                 Entry Length : 08
[0EAh 0234   2]                     Reserved : 0000
[0ECh 0236   1]               Enumeration ID : 00
[0EDh 0237   1]               PCI Bus Number : 00

[0EEh 0238   2]                     PCI Path : 14,00


[0F0h 0240   2]                Subtable Type : 0002 [Root Port ATS Capability]
[0F2h 0242   2]                       Length : 0020

[0F4h 0244   1]                        Flags : 00
[0F5h 0245   1]                     Reserved : 00
[0F6h 0246   2]           PCI Segment Number : 0000

[0F8h 0248   1]            Device Scope Type : 02 [PCI Bridge Device]
[0F9h 0249   1]                 Entry Length : 08
[0FAh 0250   2]                     Reserved : 0000
[0FCh 0252   1]               Enumeration ID : 00
[0FDh 0253   1]               PCI Bus Number : 17

[0FEh 0254   2]                     PCI Path : 00,00


[100h 0256   1]            Device Scope Type : 02 [PCI Bridge Device]
[101h 0257   1]                 Entry Length : 08
[102h 0258   2]                     Reserved : 0000
[104h 0260   1]               Enumeration ID : 00
[105h 0261   1]               PCI Bus Number : 3A

[106h 0262   2]                     PCI Path : 00,00


[108h 0264   1]            Device Scope Type : 02 [PCI Bridge Device]
[109h 0265   1]                 Entry Length : 08
[10Ah 0266   2]                     Reserved : 0000
[10Ch 0268   1]               Enumeration ID : 00
[10Dh 0269   1]               PCI Bus Number : 5D

[10Eh 0270   2]                     PCI Path : 00,00


[110h 0272   2]                Subtable Type : 0002 [Root Port ATS Capability]
[112h 0274   2]                       Length : 0028

[114h 0276   1]                        Flags : 00
[115h 0277   1]                     Reserved : 00
[116h 0278   2]           PCI Segment Number : 0000

[118h 0280   1]            Device Scope Type : 02 [PCI Bridge Device]
[119h 0281   1]                 Entry Length : 08
[11Ah 0282   2]                     Reserved : 0000
[11Ch 0284   1]               Enumeration ID : 00
[11Dh 0285   1]               PCI Bus Number : 80

[11Eh 0286   2]                     PCI Path : 00,00


[120h 0288   1]            Device Scope Type : 02 [PCI Bridge Device]
[121h 0289   1]                 Entry Length : 08
[122h 0290   2]                     Reserved : 0000
[124h 0292   1]               Enumeration ID : 00
[125h 0293   1]               PCI Bus Number : 85

[126h 0294   2]                     PCI Path : 00,00


[128h 0296   1]            Device Scope Type : 02 [PCI Bridge Device]
[129h 0297   1]                 Entry Length : 08
[12Ah 0298   2]                     Reserved : 0000
[12Ch 0300   1]               Enumeration ID : 00
[12Dh 0301   1]               PCI Bus Number : AE

[12Eh 0302   2]                     PCI Path : 00,00


[130h 0304   1]            Device Scope Type : 02 [PCI Bridge Device]
[131h 0305   1]                 Entry Length : 08
[132h 0306   2]                     Reserved : 0000
[134h 0308   1]               Enumeration ID : 00
[135h 0309   1]               PCI Bus Number : D7

[136h 0310   2]                     PCI Path : 00,00


[138h 0312   2]                Subtable Type : 0003 [Remapping Hardware Static Affinity]
[13Ah 0314   2]                       Length : 0014

[13Ch 0316   4]                     Reserved : 00000000
[140h 0320   8]                 Base Address : 000000009D7FC000
[148h 0328   4]             Proximity Domain : 00000000

[14Ch 0332   2]                Subtable Type : 0003 [Remapping Hardware Static Affinity]
[14Eh 0334   2]                       Length : 0014

[150h 0336   4]                     Reserved : 00000000
[154h 0340   8]                 Base Address : 00000000AAFFC000
[15Ch 0348   4]             Proximity Domain : 00000000

Raw Table Data: Length 352 (0x160)

  0000: 44 4D 41 52 60 01 00 00 01 DC 41 4C 41 53 4B 41  // DMAR`.....ALASKA
  0010: 41 20 4D 20 49 20 00 00 01 00 00 00 49 4E 54 4C  // A M I ......INTL
  0020: 27 05 16 20 2D 01 00 00 00 00 00 00 00 00 00 00  // '.. -...........
  0030: 00 00 60 00 00 00 00 00 00 C0 7F D3 00 00 00 00  // ..`.............
  0040: 03 08 00 00 0F 80 05 04 01 08 00 00 00 80 04 00  // ................
  0050: 01 08 00 00 00 80 04 01 01 08 00 00 00 80 04 02  // ................
  0060: 01 08 00 00 00 80 04 03 01 08 00 00 00 80 04 04  // ................
  0070: 01 08 00 00 00 80 04 05 01 08 00 00 00 80 04 06  // ................
  0080: 01 08 00 00 00 80 04 07 02 08 00 00 00 80 00 00  // ................
  0090: 00 00 20 00 00 00 00 00 00 C0 FF E0 00 00 00 00  // .. .............
  00A0: 03 08 00 00 10 85 05 04 02 08 00 00 00 85 00 00  // ................
  00B0: 00 00 20 00 00 00 00 00 00 C0 7F EE 00 00 00 00  // .. .............
  00C0: 03 08 00 00 11 AE 05 04 02 08 00 00 00 AE 00 00  // ................
  00D0: 01 00 20 00 00 00 00 00 00 00 46 75 00 00 00 00  // .. .......Fu....
  00E0: FF 0F 47 75 00 00 00 00 01 08 00 00 00 00 14 00  // ..Gu............
  00F0: 02 00 20 00 00 00 00 00 02 08 00 00 00 17 00 00  // .. .............
  0100: 02 08 00 00 00 3A 00 00 02 08 00 00 00 5D 00 00  // .....:.......]..
  0110: 02 00 28 00 00 00 00 00 02 08 00 00 00 80 00 00  // ..(.............
  0120: 02 08 00 00 00 85 00 00 02 08 00 00 00 AE 00 00  // ................
  0130: 02 08 00 00 00 D7 00 00 03 00 14 00 00 00 00 00  // ................
  0140: 00 C0 7F 9D 00 00 00 00 00 00 00 00 03 00 14 00  // ................
  0150: 00 00 00 00 00 C0 FF AA 00 00 00 00 00 00 00 00  // ................
