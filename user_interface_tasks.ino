// -------------------- LED Control -------------------- //

void softBlinkRGBTask(void *pvParameters) {
  rgb_led_params_t *rgb_led_params_parameters = (rgb_led_params_t *)pvParameters;

  // LED fading look-up table:
  //static float fadingPattern[5] = { 0, 0.1170, 0.4132, 0.7500, 1 };
  static float fadingPattern[10] = { 0, 0.0271, 0.1054, 0.2265, 0.3773, 0.5413, 0.7008, 0.8386, 0.9397, 1 };
  uint8_t redTones[20];
  uint8_t greenTones[20];
  uint8_t blueTones[20];

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    rgb_led_params_parameters->busy = true;
    rgb_led_params_parameters->stop_please = false;

    // Calculates the values for each colour:
    for (int i = 0; i < 10 && rgb_led_params_parameters->stop_please == false; i++) {
      redTones[i] = (uint8_t)(fadingPattern[i] * (uint8_t)(rgb_led_params_parameters->hex_value >> 16));
      redTones[19 - i] = redTones[i];

      greenTones[i] = (uint8_t)(fadingPattern[i] * (uint8_t)(rgb_led_params_parameters->hex_value >> 8));
      greenTones[19 - i] = greenTones[i];

      blueTones[i] = (uint8_t)(fadingPattern[i] * (uint8_t)(rgb_led_params_parameters->hex_value));
      blueTones[19 - i] = blueTones[i];
    }

    for (int i = 0; i < rgb_led_params_parameters->blink_number && rgb_led_params_parameters->stop_please == false; i++) {
      for (int j = 0; j < 20 && rgb_led_params_parameters->stop_please == false; j++) {
        analogWrite(RED_LED, redTones[j]);
        analogWrite(GREEN_LED, greenTones[j]);
        analogWrite(BLUE_LED, blueTones[j]);
        vTaskDelay(pdMS_TO_TICKS(rgb_led_params_parameters->on_time / 20));
      }

      if (i < (rgb_led_params_parameters->blink_number - 1)) vTaskDelay(pdMS_TO_TICKS(rgb_led_params_parameters->off_time));
    }
    rgb_led_params_parameters->busy = false;
    rgb_led_params_parameters->stop_please = false;
  }
}



// -------------------- Buzzer Control -------------------- //

void beepsTask(void *pvParameters) {
  buzzer_params_t *buzzer_params_parameters = (buzzer_params_t *)pvParameters;

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    buzzer_params_parameters->busy = true;

    for (int i = 0; i < buzzer_params_parameters->beep_number && buzzer_params_parameters->stop_please == false; i++) {
      tone(BUZZER, buzzer_params_parameters->frequency);
      vTaskDelay(buzzer_params_parameters->on_time);
      noTone(BUZZER);

      if (i < (buzzer_params_parameters->beep_number - 1)) vTaskDelay(buzzer_params_parameters->off_time);
    }
    noTone(BUZZER);
    buzzer_params_parameters->busy = false;
     buzzer_params_parameters->stop_please = false;
  }
}


// -------------------- User Interface Pre-made Patterns -------------------- //

void goodPacketWarning(void *pvParameters) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (rgb_led_params.busy) {
      rgb_led_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xSoftBlinkRGBTask);
    }
    rgb_led_params.blink_number = 1;
    rgb_led_params.hex_value = 0x0088FF;
    rgb_led_params.on_time = 1000;
    rgb_led_params.off_time = 100;
    xTaskNotifyGive(xSoftBlinkRGBTask);

    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2100;
    buzzer_params.on_time = 15;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(16);


    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2250;
    buzzer_params.on_time = 15;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(16);


    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2500;
    buzzer_params.on_time = 15;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(16);


    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2750;
    buzzer_params.on_time = 20;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(21);

    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 3000;
    buzzer_params.on_time = 30;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);
  }
}

void badPacketWarning(void *pvParameters) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (rgb_led_params.busy) {
      rgb_led_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xSoftBlinkRGBTask);
    }
    rgb_led_params.blink_number = 1;
    rgb_led_params.hex_value = 0xFF0088;
    rgb_led_params.on_time = 1000;
    rgb_led_params.off_time = 100;
    xTaskNotifyGive(xSoftBlinkRGBTask);

    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2100;
    buzzer_params.on_time = 15;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(16);


    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 3000;
    buzzer_params.on_time = 15;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(16);


    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2750;
    buzzer_params.on_time = 15;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(16);


    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2500;
    buzzer_params.on_time = 20;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);

    vTaskDelay(21);

    if (buzzer_params.busy) {
      buzzer_params.stop_please = true;  // Stops it if it's already running
      xTaskAbortDelay(xBeepsTask);
    }
    buzzer_params.beep_number = 1;
    buzzer_params.frequency = 2100;
    buzzer_params.on_time = 30;
    buzzer_params.off_time = 0;
    xTaskNotifyGive(xBeepsTask);
  }
}

void ImAliveBeeps() {
  tone(BUZZER, 4525);
  delay(80);
  noTone(BUZZER);
  delay(180);
  tone(BUZZER, 4525);
  delay(80);
  noTone(BUZZER);
  delay(180);
  tone(BUZZER, 4025);
  delay(100);
  noTone(BUZZER);
  delay(45);
  tone(BUZZER, 4525);
  delay(80);
  noTone(BUZZER);
  delay(180);
  tone(BUZZER, 5372);
  delay(80);
  noTone(BUZZER);
  delay(200);
  tone(BUZZER, 4525);
  delay(80);
  noTone(BUZZER);
}