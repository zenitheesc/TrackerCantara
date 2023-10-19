
void dataHandlerTask(void* pvParameters) {

  SX127X_t* SX127X = (SX127X_t*)pvParameters;

  uint8_t RxData[255];
  tracker_packet_t tracker_packet;
  uint8_t ReceivedDataSize;
  uint8_t rssi;
  bool CRCStatus;

  uint16_t lastPacketID = 0xFFFF;  // Begins with the max value so that any other packet ID can be stored, including 0.

  //UBaseType_t watermark;

  LoRaConfig(SX127X);
  SX127X_set_op_mode(SX127X, RX);

  for (;;) {

    //watermark = uxTaskGetStackHighWaterMark(xDataHandlerTask);
    //Serial.printf("\nHighWaterMark: %d",watermark);

    //LoRaConfig(SX127X);
    //SX127X_set_op_mode(SX127X, RX);  // I HATE MY LIFE AAAAAAAAAAAAAAAAAAAAAAAAAAAA

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Waits for DIO0 Notification

    LoRa_ReadPacket(SX127X, RxData, sizeof(RxData), &ReceivedDataSize, &CRCStatus);
    LoRa_get_packet_rssi(SX127X, &rssi);

    // Serial.printf("\nReceived data size: %d\n", ReceivedDataSize);
    // Serial.println("Data:\n");

    if (!CRCStatus) {                       // Good CRC :)
      xTaskNotifyGive(xGoodPacketWarning);  // Beeps and blinks appropriately
    } else {                                // Bad CRC :(
      xTaskNotifyGive(xBadPacketWarning);   // Beeps and blinks appropriately
    }

    memcpy(tracker_packet.raw, RxData, sizeof(tracker_packet));
    printToSerial(&tracker_packet.values, ReceivedDataSize, CRCStatus, rssi);

    if (CRCStatus == false) {
      printToBluetooth(&tracker_packet.values, ReceivedDataSize, CRCStatus, rssi);  // Only send good packets to bluetooth
    }
    // Tracker sends 5 identical packets. We will ignore repeated packets, only one good packet per bunch will be stored.
    if (CRCStatus == false && tracker_packet.values.packet_id != lastPacketID) {
      storeIntoFlash(&tracker_packet.values, rssi);
      lastPacketID = tracker_packet.values.packet_id;
    }
  }
}


void commandHandlerTask(void* pvParameters) {
  String* serialCommand = (String*)pvParameters;
  bool deciding = false;
  bool eraseState;

  for (;;) {

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Waits for DIO0 Notification

    Serial.print("\nserialCommand: ");
    Serial.print(*serialCommand);
    Serial.print("\n");

    if (*serialCommand == String("dump")) {
      xTaskNotifyGive(xFlashDumpTask);

    } else if (*serialCommand == String("ledr")) {
      standbyColor = 0x220000;

    } else if (*serialCommand == String("ledg")) {
      standbyColor = 0x001100;

    } else if (*serialCommand == String("ledb")) {
      standbyColor = 0x000011;

    } else if (*serialCommand == String("ledw")) {
      standbyColor = 0x221111;

    } else if (*serialCommand == String("wipe")) {
      Serial.print("\nAre you sure? You are about to erase ALL memory inside the flash!");
      Serial.print("\nSend \"$YESS\" if you want to do this or \"$NOOO\" if you want to cancel.");
      deciding = true;

    } else if (deciding == true && *serialCommand == String("NOOO")) {
      Serial.print("\nErasing operation canceled. That was close huh?");
      deciding = false;

    } else if (deciding == true && *serialCommand == String("YESS")) {
      Serial.print("\nOh crap, they are really doing it. Ok then.");
      if (eTaskGetState(xFlashDumpTask) == eBlocked) {
        eraseState = flash->eraseChip();
        if (eraseState) Serial.print("\nThe flash chip has been cleaned. There's no going back now >:D");
        else Serial.print("\nThere was an error erasing the flash chip.\nCheck if other writing operations were happening in other tasks or if there are any hardware issues.");

        Serial.print("\nRestarting the flash chip.");
        eraseState = flash->begin();
        if (eraseState) Serial.print("\nFlash chip successfully restarted :)");
        else Serial.print("\nThere was an error restarting the flash chip.\nCheck if other writing operations were happening in other tasks or if there are any hardware issues.");

      } else {
        Serial.print("\nAnother task is curently using the flash. Tray again later.");
      }

      deciding = false;
    }

    *serialCommand = "";
  }
}

void storeIntoFlash(tracker_packet_values_t* tracker_packet, uint8_t rssi) {
  static flash_packet_values_t flash_packet;
  static int address;

  flash_packet.dead_beef = 0xDEADBEEF;
  flash_packet.tracker_packet = *tracker_packet;
  flash_packet.rssi = rssi;

  address = flash->getAddress((uint16_t)sizeof(flash_packet));

  if (address < 0) address = 0;  // It becomes -1 after a chip erase.

  flash->writeAnything(address, flash_packet);

  // flash_packet_values_t tracker_packet_from_flash;
  // flash->readAnything(address, tracker_packet_from_flash);
  // Serial.printf("\n\nAddress: %d", address);
  // Serial.printf("\nAmount of bytes to be stored: %d", sizeof(flash_packet));
  // Serial.printf("\nData read from flash:");
  // Serial.printf("\nCRCStatus: %d", tracker_packet_from_flash.CRCStatus);
  // Serial.printf("\nrssi: %d", tracker_packet_from_flash.rssi);
  // Serial.printf("\nlatitude: %f", tracker_packet_from_flash.tracker_packet.latitude);
  // Serial.printf("\nlongitude: %f", tracker_packet_from_flash.tracker_packet.longitude);
  // Serial.printf("\naltitude: %f", tracker_packet_from_flash.tracker_packet.altitude);
  // Serial.printf("\nseconds: %d", tracker_packet_from_flash.tracker_packet.seconds);
  // Serial.printf("\nminutes: %d", tracker_packet_from_flash.tracker_packet.minutes);
  // Serial.printf("\nhours: %d", tracker_packet_from_flash.tracker_packet.hours);
  // Serial.printf("\npacket_id: %d\n\n", tracker_packet_from_flash.tracker_packet.packet_id);
}

void printPacket(tracker_packet_values_t* tracker_packet, uint8_t rssi) {
  static unsigned char hours;

  Serial.printf("%d", rssi);
  Serial.printf(";");
  Serial.printf("%d", tracker_packet->packet_id);
  Serial.printf(";");
  Serial.printf("%.6f", tracker_packet->latitude);
  Serial.printf(";");
  Serial.printf("%.6f", tracker_packet->longitude, 6);
  Serial.printf(";");
  Serial.printf("%.6f", tracker_packet->altitude);
  Serial.printf(";");
  Serial.printf("\"");
  if (tracker_packet->hours < UTC_OFFSET) hours = 24 - tracker_packet->hours - UTC_OFFSET;
  else hours = tracker_packet->hours - UTC_OFFSET;
  if ((hours) < 10) Serial.printf("0");
  Serial.printf("%d", hours);
  Serial.printf(":");
  if (tracker_packet->minutes < 10) Serial.printf("0");
  Serial.printf("%d", tracker_packet->minutes);
  Serial.printf(":");
  if (tracker_packet->seconds < 10) Serial.printf("0");
  Serial.printf("%d", tracker_packet->seconds);
  Serial.printf("\"\n");
}

void flashDumpTask(void* pvParameters) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Packet variable:
    flash_union_t data_packet;
    
    // Control variables:
    uint8_t searchCounter = 0;
    uint8_t packetBytesCounter = 0;
    bool deadBeefFound;
    uint8_t byte = 0;
    unsigned long lastPacketTimeStamp = millis();

    for (uint32_t address = 0; address < 100000; address++) {
      byte = flash->readByte(address);
      //Serial.write(byte);

      if (deadBeefFound == false && searchCounter == 0 && byte == 0xEF) {
        searchCounter++;
      } else if (searchCounter == 1 && byte == 0xBE) {
        searchCounter++;
      } else if (searchCounter == 2 && byte == 0xAD) {
        searchCounter++;
      } else if (searchCounter == 3 && byte == 0xDE) {
        searchCounter = 4;
      } else if (searchCounter == 4) {
        deadBeefFound = true;
        searchCounter = 0;
      } else {
        searchCounter = 0;
      }

      if (deadBeefFound) {
        lastPacketTimeStamp = millis();
        data_packet.raw[packetBytesCounter] = byte;
        packetBytesCounter++;

        if (packetBytesCounter == 18) {
          printPacket(&data_packet.values.tracker_packet, data_packet.values.rssi);
          packetBytesCounter = 0;
          deadBeefFound = false;
        }
      }

      // if 5 seconds went by without finding new packets the flash is probably empty.
      if(millis() - lastPacketTimeStamp > 5000){
        break;
      }

      vTaskDelay(1);
    }

    // if (finalAddress < 0) {
    //   Serial.print("\nThe Flash chip is empty!");
    // } else {
    //   Serial.printf("\nGoing to print until the address: %d", finalAddress - sizeof(flash_packet_values_t));
    //   Serial.printf("\nThat should be about %d tracker packets.\n\n", finalAddress / sizeof(flash_packet_values_t));
    //   finalAddress = finalAddress - sizeof(flash_packet_values_t);

    //   uint8_t byte = 0;
    //   // for (uint32_t address = 0; address < finalAddress; address++) {
    //   //   byte = flash->readByte(address);
    //   //   Serial.printf("%c", byte);
    //   // }

    //   for (uint32_t address = 0; address < finalAddress; address += sizeof(flash_packet_values_t)) {
    //     flash->readAnything(address, tracker_packet_from_flash);
    //     Serial.print(address);
    //     Serial.print(';');
    //     printToSerial(&tracker_packet_from_flash.tracker_packet, 0, 1, tracker_packet_from_flash.rssi);
    //   }

    //   // for (uint8_t numero = 28; numero < 42; numero++) {
    //   //   Serial.printf("\n\n -------------------- %d -------------------- \n\n", numero);
    //   //   for (uint32_t address = numero; address < finalAddress; address += sizeof(flash_packet_values_t)) {
    //   //     flash->readAnything(address, tracker_packet_from_flash);
    //   //     Serial.print(address);
    //   //     Serial.print(';');
    //   //     printToSerial(&tracker_packet_from_flash.tracker_packet, 0, 1, tracker_packet_from_flash.rssi);
    //   //   }
    //   // }
    // }
  }
}


void printToSerial(tracker_packet_values_t* tracker_packet, uint8_t ReceivedDataSize, bool CRCStatus, uint8_t rssi) {
  static unsigned char hours;

  Serial.print(ReceivedDataSize);
  Serial.print(';');
  Serial.print(CRCStatus);
  Serial.print(';');
  Serial.print(rssi);
  Serial.print(';');
  Serial.print(tracker_packet->packet_id);
  Serial.print(';');
  Serial.print(tracker_packet->latitude, 6);
  Serial.print(';');
  Serial.print(tracker_packet->longitude, 6);
  Serial.print(';');
  Serial.print(tracker_packet->altitude);
  Serial.print(';');
  Serial.print('\"');
  if (tracker_packet->hours < UTC_OFFSET) hours = 24 - tracker_packet->hours - UTC_OFFSET;
  else hours = tracker_packet->hours - UTC_OFFSET;
  if ((hours) < 10) Serial.print('0');
  Serial.print(hours);
  Serial.print(':');
  if (tracker_packet->minutes < 10) Serial.print('0');
  Serial.print(tracker_packet->minutes);
  Serial.print(':');
  if (tracker_packet->seconds < 10) Serial.print('0');
  Serial.print(tracker_packet->seconds);
  Serial.println('\"');
}

void printToBluetooth(tracker_packet_values_t* tracker_packet, uint8_t ReceivedDataSize, bool CRCStatus, uint8_t rssi) {

  static unsigned char hours;

  //SerialBT.print(ReceivedDataSize);
  //SerialBT.print(';');
  //SerialBT.print(CRCStatus);
  //SerialBT.print(';');
  SerialBT.print(rssi);
  SerialBT.print(';');
  SerialBT.print(tracker_packet->packet_id);
  SerialBT.print(';');
  if (tracker_packet->latitude < 0) SerialBT.print('-');
  SerialBT.print(tracker_packet->latitude * -1, 6);
  SerialBT.print(';');
  if (tracker_packet->longitude < 0) SerialBT.print('-');
  SerialBT.print(tracker_packet->longitude * -1, 6);
  SerialBT.print(';');
  SerialBT.print(tracker_packet->altitude);
  SerialBT.print(';');
  SerialBT.print('\"');
  if (tracker_packet->hours < UTC_OFFSET) hours = 24 - tracker_packet->hours - UTC_OFFSET;
  else hours = tracker_packet->hours - UTC_OFFSET;
  if ((hours) < 10) SerialBT.print('0');
  SerialBT.print(hours);
  SerialBT.print(':');
  if (tracker_packet->minutes < 10) SerialBT.print('0');
  SerialBT.print(tracker_packet->minutes);
  SerialBT.print(':');
  if (tracker_packet->seconds < 10) SerialBT.print('0');
  SerialBT.print(tracker_packet->seconds);
  SerialBT.println('\"');
}


/* -------------------------- Register Sweep -------------------------- */
HAL_StatusTypeDef RegisterSweep(SX127X_t* SX127X) {
  /*
     Lembrar de mudar o SPI_read_register pelo macro READ_REG quando for adicionar
     essa função na biblioteca.
  */
  uint8_t LeituraSPI = 0;
  uint16_t Reg = 0;
  uint8_t SweepErrorCounter = 0;
  HAL_StatusTypeDef status = HAL_OK;

  for (Reg = 0; Reg <= 255; Reg++) {
    status = SPI_read_register(SX127X->spi_bus, SX127X->ss_pin, Reg, &LeituraSPI);
    if (status != HAL_OK) {
      Serial.print("\nErro na Leitura");
      SweepErrorCounter++;
      status = HAL_OK;
    } else
      //UART_print(uart_bus, "\nRegistrador %d = %d", Reg, LeituraSPI);
      Serial.print("\n");
    Serial.print(LeituraSPI);
  }
  Serial.print("\nDONE! Wow, that was a lot of work | Reading errors encountered: ");
  Serial.println(SweepErrorCounter);
  return HAL_OK;
}